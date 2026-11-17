# Phase 2 (Gate 2): WRAM Mapping & Three-Pillar Direct-State Mouse Input

**Status:** Complete (`501d816` on `main`)  
**Verification:** `ctest --test-dir build --output-on-failure` — **3/3** passed (`test_boot_loader`, `test_replay_and_eviction`, `Gate2MouseTest`) under `-Wall -Wextra -Werror`, ASan, and UBSan.

---

## 1. WRAM Discovery Methodology

To implement zero-invasion direct-state mouse input without synthetic D-Pad pulsing, core WRAM addresses were discovered and verified using two complementary techniques:

### 1.1 Static Analysis of Lifted Routines (`src/gen/fm_bank_C1.c`)
Inspecting `gen_C10256` in the Phase 1 lifted bank output (`src/gen/fm_bank_C1.c`) confirmed the joypad polling block in Bank `$7E`:
- `$7E:0DC8` (16-bit): Player 1 held buttons
- `$7E:0DCA` (16-bit): Player 2 held buttons
- `$7E:0DCC` (16-bit): Player 1 newly pressed (edge)
- `$7E:0DCE` (16-bit): Player 2 newly pressed (edge)
- `$7E:0DD0` (16-bit): Player 1 newly released
- `$7E:0DD2` (16-bit): Player 2 newly released

### 1.2 Differential Monotonic Probing (`tools/wram_probe.c`)
We built `fm_wram_probe` (`tools/wram_probe.c`) to replay canonical `.fmrec` traces to target gameplay states, issue controlled directional/menu inputs, and detect WRAM bytes that move monotonically and reverse cleanly:

1. **Menu Cursor Discovery (`jp_trace_04_mission1_combat.fmrec`, frame `15008` — Weapon Select menu):**
   - Probing `DOWN, DOWN, UP, UP` yielded:
     - `$7E:0334`: `00 -> 01 -> 02 -> 01 -> 00` (`PAP55` -> `ZENITH PUNCH` -> `- Guard -`)
     - `$7E:0366`: `00 -> 01 -> 02 -> 01 -> 00` (menu cursor mirror)
2. **Tactical Cursor Discovery (`jp_trace_04_mission1_combat.fmrec`, frame `15883 + 400` idle — Mission 1 active player turn):**
   - Probing `RIGHT, RIGHT, LEFT, LEFT` yielded:
     - `$7E:02A0` (16-bit): `20 -> 28 -> 30 -> 28 -> 20` (`±8px` per tap in `256×224` SNES screen space)
     - `$7E:1076`: `1C -> 24 -> 2C -> 24 -> 1C` (OAM cursor sprite X shadow)
   - Probing `DOWN, DOWN, UP, UP` yielded:
     - `$7E:02A2` (16-bit): `A8 -> B0 -> B8 -> B0 -> A8` (`±8px` per tap in `256×224` SNES screen space)
     - `$7E:1077`: `A4 -> AC -> B4 -> AC -> A4` (OAM cursor sprite Y shadow)
3. **Camera Scroll & Pan Discovery (`25× RIGHT` pan from `x = 32`):**
   - `$7E:02A0` clamped at `0x00C0` (`192`), while the remaining `+40px` scrolled the tactical map:
     - `$7E:0D72`, `$7E:0D76`, `$7E:0D7A`: `0x0600 -> 0x0628` (`+40px`, BG1/BG2/BG3 H-scroll mirrors)
     - `$7E:0D8A`, `$7E:0D8E`, `$7E:0D92`: `0x0600 -> 0x0628` (`+40px`, shadow scroll mirrors)
     - `$7E:02AE`: `0x0600 -> 0x0620` (`+32px`, camera target X)
4. **Unit & Inventory Struct Verification:**
   - `$7E:D580` (`active_unit_0`) and `$7E:D60D` (`active_unit_1`, stride `0x8D`): verified against Roid's Shrike on screen (`HP 107 = 33 Body + 24 L-Arm + 24 R-Arm + 26 Legs`).
   - `$7E:CF80–$7E:D1FF`: `0x0280`-byte inventory block.

---

## 2. Non-Breaking WRAM Overlay (`include/fm_wram.h`)

- Defined `FmWramOverlay`, a `0x20000`-byte (`128 KB`) C union combining `uint8_t raw[FM_WRAM_SIZE]` with `mapped.*` fields at verified offsets.
- Used byte-backed `FmWramUnitRaw` (`uint8_t bytes[0x8D]`) for `active_unit_0` (`0xD580`) and odd-aligned `active_unit_1` (`0xD60D`) to guarantee zero unaligned-access UB under UBSan.
- Provided `fm_wram_rd16()` / `fm_wram_wr16()` little-endian helpers.
- Added 14 compile-time `_Static_assert`s verifying `sizeof(FmWramOverlay) == 0x20000` and `offsetof` for every mapped field.

---

## 3. Three-Pillar Direct-State Mouse Input (`include/input.h`, `src/engine/input.c`, `src/main.c`)

### 3.1 Aspect-Correct Window-to-SNES Mapping (`fm_input_window_to_snes`)
- Maps host window coordinates `(win_x, win_y)` in `(win_w, win_h)` into the `512×448` PPU viewport:
  - **`16:9` Widescreen (`800×448` virtual canvas):** Center `512×448` PPU viewport spans `x = [144..655]` with `144px` left/right telemetry wings. Pointer events inside the wings return `false` and do not move the SNES cursor.
  - **`4:3` Classic (`512×448` virtual canvas):** Preserves aspect ratio with letterboxing/pillarboxing.

### 3.2 Pillar 1 — Tactical Isometric Grid & Camera Pan (`FM_INPUT_MODE_TACTICAL`)
- **Isometric Projection (`fm_input_screen_to_iso_tile` / `fm_input_iso_tile_to_screen`):**
  - Converts `512×448` PPU coordinates + `256×224` camera scroll `(cam_x, cam_y)` to `32×16` isometric diamond tile `(gx, gy)` using floor division for negative coordinates.
- **Direct Cursor Write:** On pointer movement inside the viewport, writes `(uint16_t)(snes_x >> 1)` and `(uint16_t)(snes_y >> 1)` directly to `$7E:02A0` / `$7E:02A2` before `snes_runFrame()`.
- **Right/Middle-Drag Camera Pan:** Dragging beyond `FM_DRAG_THRESHOLD_PX` (`6px`) pans BG1–BG3 scroll mirrors (`$7E:0D72..$0D7C`), shadow mirrors (`$7E:0D8A..$0D94`), and camera target (`$7E:02AE/$02B0`) by `(-dx/2, -dy/2)`. Releasing the right button after a drag suppresses `FM_BTN_B` cancel.

### 3.3 Pillar 2 — Menus, Hangar, Shop & Coliseum (`FM_INPUT_MODE_MENU`)
- **2D Bounding-Box Hit-Testing (`FmMenuLayout`, `fm_input_menu_hit_test`):**
  - Supports arbitrary single-column lists and multi-column grids (`row_w`, `row_h`, `rows`, `cols`, `col_stride`), returning row-major slot index `row * cols + col`.
- **Direct Slot Write & Click Edges:** Writes the hit-tested index directly to `$7E:0334` / `$7E:0366`, and injects `FM_BTN_A` on left-click or `FM_BTN_B` on right-click.

### 3.4 Pillar 3 — World Map & Zero-Invasion Yield (`FM_INPUT_MODE_WORLDMAP` / Stationary Idle)
- **Zero-Invasion Yield (`fm_input_apply_pre_frame`):** When no pointer motion, click, or camera drag has occurred since the previous frame, `fm_input_apply_pre_frame()` returns `false` immediately, guaranteeing **0 bytes** of WRAM or joypad mutation so keyboard/gamepad input and lockstep traces remain completely untouched.
- **SDL2 Loop Integration (`src/main.c`):** Wired `SDL_MOUSEMOTION`, `SDL_MOUSEBUTTONDOWN`, and `SDL_MOUSEBUTTONUP` into `FmMouseState` and pre-frame `fm_input_apply_pre_frame()`.

---

## 4. Gate 2 Verification (`tests/test_gate2_mouse.c`)

Added `Gate2MouseTest` to `CMakeLists.txt` and `ctest` with 6 unit test suites:
1. `test_wram_overlay_layout`: Verifies `FmWramOverlay` size (`0x20000`) and union aliasing for cursor/menu offsets.
2. `test_window_to_snes_mapping`: Verifies `16:9` (`1600×896` wing rejection + center mapping) and `4:3` (`1280×720`) aspect-correct transforms.
3. `test_isometric_projection`: Verifies exact round-trip `fm_input_iso_tile_to_screen` ↔ `fm_input_screen_to_iso_tile` across `[-4..10] × [-4..10]`.
4. `test_pillar1_tactical_and_pan`: Verifies direct `$7E:02A0/$02A2` cursor writes, Right-drag camera scroll updates across BG1–BG3 + shadows, and `FM_BTN_B` suppression on drag release.
5. `test_pillar2_menu_hit_testing`: Verifies 2-column bounding-box hit-testing (including inter-column gap rejection), direct `$7E:0334/$0366` writes, and `FM_BTN_A`/`FM_BTN_B` click injection.
6. `test_pillar3_zero_invasion_idle`: Verifies via `memcmp` that stationary mouse state produces **0-byte** WRAM mutation and leaves existing joypad masks untouched.
