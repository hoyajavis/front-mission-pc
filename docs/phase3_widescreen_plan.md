# Phase 3 Plan: 16:9 Widescreen & Read-Only Tactical Telemetry Wings (Gate 3)

## Objective

Implement the `800×448` 16:9 widescreen compositor and strictly read-only, context-switched `144×448` tactical/hangar telemetry wings on top of the Gate 1 lockstep-verified core and Gate 2 WRAM/mouse foundation, without mutating a single byte of SNES WRAM.

---

## Milestone 3.1 — WRAM & ROM Table Discovery for Telemetry

Extend `tools/wram_probe.c` and `include/fm_wram.h` against canonical `.fmrec` traces (`jp_trace_03_mission1_tactical.fmrec`, `jp_trace_04_mission1_combat.fmrec`, and a Hangar/Shop trace):

1. **Scene / Game-Mode State Bytes:** Identify the WRAM state/mode flags that cleanly distinguish:
   - `Title / Boot`
   - `Town / Hangar / Shop / Coliseum`
   - `Tactical Map`
   - `Battle Forecast / Combat Stage`
2. **Wanzer 4-Part & Pilot Struct Layout (`$7E:D580`, stride `0x8D`):** Map exact byte offsets inside `FmWramUnitRaw` for:
   - Part IDs and current/max HP for **Body**, **L-Arm**, **R-Arm**, and **Legs**
   - Equipped weapons, ammo, and backpack/CPU slots
   - **Power-to-Weight** (`Output` vs total `Weight`)
   - **Pilot Proficiency** (`Melee`, `Short`, `Long`, `Agility`) & active **Skills** (`Duel`, `Guide`, `Switch`, `Speed`, `First`, `Stun`, `Double`)
   - Unit grid coordinates `(gx, gy)`, team allegiance, and hovered-unit / hovered-target index
3. **Read-Only ROM Stat Getters (`[MAINT-5]`):** Wrap static ROM part/weapon table reads in inline semantic getters (`fm_rom_get_part_stats()`, `fm_rom_get_weapon_stats()`) rather than scattering raw `$Dx:xxxx` offsets.

---

## Milestone 3.2 — Read-Only Telemetry Extractor (`src/engine/fm_telemetry.c` / `.h`)

Implement a strictly zero-invasion telemetry snapshot layer:

- **Const-Only Contract:** Every extractor accepts `const uint8_t *wram, const uint8_t *rom`—never `uint8_t *`—guaranteeing 0-byte WRAM mutation.
- **Cold-Boot `0x55` WRAM Guard:** Detect uninitialized/cold-boot WRAM (`0x55` fill or invalid scene markers) and suppress scene telemetry until valid game state is active.
- **Snapshot Struct (`FmWidescreenTelemetry`):**
  - **Scene & Macro:** Detected `FmSceneId`, current mission/stage ID, turn number, and funds (`Credits`).
  - **Selected / Hovered Wanzer:** 4-part HP (`Body`, `L-Arm`, `R-Arm`, `Legs`), status/destroyed flags, equipped weapons + hit%/damage/range summary, Pilot level/proficiencies/skills.
  - **Tactical Comparison:** Hovered enemy unit breakdown alongside the active player unit on the Tactical Map / Battle Forecast.
  - **Hangar / Shop Context:** `Output` vs `Weight` budget meter, part stat deltas against the currently equipped part, and fleet readiness summary.

---

## Milestone 3.3 — `800×448` Widescreen Compositor (`src/engine/widescreen.c` / `.h`)

Build the master dual-layer software compositor using the BGR555 9-slice primitives in `src/engine/snes_ui_rasterizer.h`:

- **Canvas Geometry:**
  - `800×448` master RGBA buffer (`2×` integer-scaled `512×448` SNES PPU core centered at `x = 144..655`, flanked by two `144×448` (`18×56` tile) wings at `x = 0..143` and `x = 656..799`).
- **Display Mode Cycle (`[F10]` Hotkey, wired to `FmUserConfig::display_mode`):**
  - `0`: **16:9 Tactical Wings** (`800×448` with context-switched left/right telemetry docks)
  - `1`: **16:9 Clean Horizon** (`800×448` clean extended background / tapestry framing without HUD cards)
  - `2`: **4:3 Classic** (`512×448` centered PPU output with clean black pillarboxing)
- **Context-Switched `144×448` Wing Cards:**
  - **Left Wing:** Selected Player Wanzer (`Body`/`L-Arm`/`R-Arm`/`Legs` segmented gauge bars via `draw_snes_gauge_bar()`, Pilot proficiencies, active Skills).
  - **Right Wing:** Context-dependent inspector—Hovered Enemy Wanzer & Weapon/Hit% breakdown on Tactical Map/Combat, or Power-to-Weight & Part Stat Deltas in Hangar/Shop.
  - **Title / Boot / Cutscenes:** Clean 9-slice decorative framing (`draw_snes_9slice_box()` / `draw_snes_header_plate()`) without stale gameplay numbers.

---

## Milestone 3.4 — SDL2 Main Loop & Input Integration

Wire the compositor into `src/main.c`:

- Update the SDL streaming texture from `512×224` to `800×448` (or `512×448` in `4:3 Classic`), compositing `snes_setPixels()` into the center `512×448` viewport and rendering the wings around it.
- Bind `[F10]` to cycle `cfg.display_mode` live and pass `widescreen = (cfg.display_mode != 2)` into `fm_input_on_mouse_move()` / `fm_input_on_mouse_button()` so mouse projection stays aligned across all 3 modes.
- Keep `fm_telemetry_extract()` strictly post-frame / read-only so `FM_INPUT_MODE_*` mouse logic and `LakeSnes` execution remain isolated.

---

## Milestone 3.5 — Gate 3 Automated Verification & Visual QA

Add Gate 3 targets to `CMakeLists.txt` and run full verification under `-Wall -Wextra -Werror` + ASan/UBSan:

1. **`Gate3TelemetryZeroInvasionTest`:** Snapshot `128 KB` WRAM before and after running `fm_telemetry_extract()` and `fm_widescreen_compose()` across all scene types (`Title`, `Hangar/Shop`, `Tactical Map`, `Combat`), asserting `memcmp == 0` (0-byte WRAM mutation) and verifying `0x55` cold-boot guard behavior.
2. **`Gate3Widescreen16x9Test`:** Verify pixel dimensions, `4:3` vs `16:9` coordinate bounds, `[F10]` mode cycling, and export headless `.ppm` renders via `fm_snes_ui_save_ppm()` for each scene to visually inspect layout, text clipping, and 9-slice alignment.
3. **Regression Check:** Verify all existing `ctest` suites (`test_boot_loader`, `test_replay_and_eviction`, `Gate2MouseTest`) plus the new Gate 3 tests pass, pushing incrementally to `origin/main` per `AGENTS.md`.
