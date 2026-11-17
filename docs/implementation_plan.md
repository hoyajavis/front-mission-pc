# Front Mission (SNES) Native PC Port — Implementation Plan

## Goal & Architectural Direction

Build a native desktop PC port of **Front Mission (1995, SNES)** from scratch in [`hoyajavis/front-mission-pc`](https://github.com/hoyajavis/front-mission-pc), adopting the proven hybrid architecture of [`hoyajavis/ogre-battle-pc`](https://github.com/hoyajavis/ogre-battle-pc) while avoiding the Pygame/manual-decompilation trap ("Attempt 2" / the "Zelda Illusion") and incorporating every major engineering lesson from `ogre-battle-pc`'s [`LESSONS_LEARNED.md`](https://github.com/hoyajavis/ogre-battle-pc/blob/main/docs/LESSONS_LEARNED.md).

### Core Architecture Stack
1. **Silicon & Execution Core:** **`sp00nznet/snesrecomp` + `LakeSnes`** hybrid tiered execution:
   - Cycle-accurate `LakeSnes` PPU, SPC700/DSP audio, DMA/HDMA, and HiROM cart mapping on Day 1.
   - Statically lifted 65816 C banks (`src/gen/fm_bank_*.c`) compiled into `libfm_recomp.a` with `O(1)` `func_table` dispatch.
   - **Tiered Fallback:** NMI/IRQ/HDMA-heavy Bank `$C0` (and any dynamically patched routines) run on the cycle-accurate interpreter, while gameplay/math/AI banks run natively.
2. **Pre-Boot ROM Patching Launcher (Native C):**
   - Loads a clean, user-supplied **`Front Mission (Japan).sfc`** (`3.0 MB`, unheadered HiROM) and applies **`FMSNE10B.IPS`** (F.H. v1.0b) + **`frontmission_addendum_v103.bps`/`.ips`** (mziab v1.03) **in-memory before `snes_reset()`**—no Python runtime dependency for end-users and no copyrighted ROMs in git.
   - Automatically tracks modified ROM ranges during patching and evicts affected addresses from `func_table` so ASM hooks (e.g. `JSL $Fxxxxx` VWF/script hooks) seamlessly execute on the interpreter.
3. **16:9 Widescreen Compositor (`800×448` Dual-Layer Canvas):**
   - Integer `2×` PPU core (`512×448`) framed by two `144×448 px` (`18×56` tiles) **Tactician/Mechanic Wings** rendered with `ogre-battle-pc`'s `BGR555` 9-slice [`snes_ui_rasterizer.c`](https://github.com/hoyajavis/ogre-battle-pc/blob/main/src/engine/snes_ui_rasterizer.c).
   - Strictly read-only telemetry (`const uint8_t *wram`, `const uint8_t *rom`) with `0x55` cold-boot WRAM guards.
4. **Three-Pillar Direct-State Mouse Input:**
   - Isometric tactical grid coordinate projection + direct WRAM cursor injection, deterministic menu/hangar hit-boxes, Right/Middle-drag camera panning, and instant keyboard/gamepad handoff (never synthetic D-Pad steering).

---

## Key Differences: Ogre Battle (LoROM) vs. Front Mission (HiROM)

| Dimension | `ogre-battle-pc` | `front-mission-pc` (This Plan) |
| :--- | :--- | :--- |
| **Cartridge Mapping** | 1.5 MB / 2.0 MB **LoROM** (`$00:8000`) | 3.0 MB (JP) $\rightarrow$ 4.0 MB (EN) **HiROM Mode `$21` FastROM** (`$C0:0000–$FF:FFFF`, mirrored `$00–$3F:$8000–$FFFF`) |
| **ROM Header & Checksum** | `0x7FC0`, 12Mbit mirrored sum (`1MB + 2×512KB`) | `0xFFC0`, 24Mbit (`2MB + 2×1MB`) for JP vs. linear 32Mbit (`4MB`) for EN |
| **Translation / Patching** | Clean USA ROM + optional community `.ips` $\rightarrow$ JSON | Clean JP ROM + **mandatory English IPS/BPS chain** (`FMSNE10B.IPS` + `addendum_v103`) expanding banks `$F0–$FF` |
| **Tactical Cursor Physics** | Mode-7 raycast (`$14DE/$14E0` screen + `$00BD/$00BF` camera) | **Isometric / oblique tile grid** (`(gx, gy)` tile coords + camera scroll) |
| **16:9 Telemetry Focus** | Chaos Frame, City Liberation, Tarot/Class Promotions | Wanzer 4-part HP (`Body/L-Arm/R-Arm/Legs`), Power-to-Weight budget, Weapon ranges/hit%, Pilot Skills & Proficiency EXP |

---

## Phased Execution Plan (Sequential Outer Gates)

### Phase 0: Repository Hygiene, Build Skeleton & HiROM Boot (Gate 0)
- **Governance & Guardrails:**
  - Initialize `.gitignore` and [`tools/git-hooks/pre-commit`](https://github.com/hoyajavis/ogre-battle-pc/blob/main/tools/git-hooks/pre-commit) to block `.sfc`, `.smc`, `.ips`, `.bps`, `.fmsav` (Rule 4 compliance).
  - Add `AGENTS.md`, `ROADMAP.md`, and `DEVLOG.md` (with `400–500` line archive rotation).
  - Enable `-Wall -Wextra -Werror` (and ASan/UBSan preset in CMake) from Day 1 for host engine code.
- **Core Vendor & Reusable Engine Import:**
  - Pin `sp00nznet/snesrecomp` + `LakeSnes` in `third_party/`.
  - Port the game-agnostic UI/config primitives from `ogre-battle-pc`: [`snes_ui_rasterizer.c`](https://github.com/hoyajavis/ogre-battle-pc/blob/main/src/engine/snes_ui_rasterizer.c), [`config.c`](https://github.com/hoyajavis/ogre-battle-pc/blob/main/src/engine/config.c), and [`gui_menu.c`](https://github.com/hoyajavis/ogre-battle-pc/blob/main/src/engine/gui_menu.c).
- **In-Memory IPS/BPS Patcher & Boot Menu (`src/engine/boot_loader.c`):**
  - Implement native C **IPS** and **BPS** patchers + CRC32/SHA-1 verification + `0xFFC0` HiROM checksum updater (`24Mbit` vs `32Mbit`).
  - Pre-Boot Launcher profiles: **Japanese Original (3 MB)**, **English Translation (F.H. v1.0b)**, and **English + Addendum v1.03 (Recommended, 4 MB)**.
- **Verify:** `front_mission_pc` boots `Front Mission (Japan).sfc` (both raw JP and in-memory patched EN v1.03) to the title screen and plays audio via `LakeSnes`.

### Phase 1: CDL Capture, HiROM Static Lifting & Lockstep Oracle (Gate 1)
> [!IMPORTANT]
> **Gate 1 Rule (from `ogre-battle-pc`):** No widescreen telemetry or mouse WRAM injection may merge until 4:3 static recompilation passes with **0-byte WRAM/VRAM/OAM/CGRAM drift** across canonical replay traces.

- **HiROM Address Support:**
  - Verify/extend `snesrecomp` bus mapping and `cdl_collector.c` / `lift_all_banks.py` for HiROM (`PC = (Bank & 0x3F) << 16 | Offset`).
- **Canonical Input Traces (`.fmrec`):**
  - Record 6–8 headless input traces covering:
    1. Boot $\rightarrow$ Title $\rightarrow$ New Game / Opening Cutscene (Huffman/VWF text)
    2. Town / Bar / Shop / Hangar (buying parts, Power-to-Weight equipping)
    3. Mission 1 Tactical Map (cursor movement, Wanzer movement, Melee/Short/Long combat)
    4. Coliseum Arena battle & Skill trigger
    5. Mission Clear $\rightarrow$ World Map & `.srm` Battery Save
- **Tiered Lifting & Lockstep Verification (`runner/`):**
  - Run `cdl_collector` across all `.fmrec` traces to capture exact `M`/`X` flag widths and indirect jump targets (`config/jumptables.ini`).
  - Lift gameplay/math banks (`$C1–$EF` and/or `$F0–$FF`) to `src/gen/fm_bank_*.c`, keeping Bank `$C0` (NMI/IRQ/DMA) on the interpreter.
  - Implement `func_table` eviction for any address modified by `FMSNE10B.IPS` / `addendum_v103` when switching boot profiles.
  - **Verify:** `RM_BOTH` lockstep oracle passes with 0-byte drift across all `.fmrec` traces.

### Phase 2: WRAM Mapping & Three-Pillar Direct-State Mouse Input (Gate 2)
- **Structured WRAM Overlay (`include/fm_wram.h`):**
  - Define a non-breaking C union overlay over the `128 KB` WRAM buffer (`fm_wram.raw[]` + `fm_wram.mapped.*`) as we map addresses from `cdl_collector` and trace breakpoints (e.g., `$7E:CF80–$7E:D1FF` inventory, `$7E:D580` / `$7E:D60D` active combat unit state, game mode & cursor/camera state).
- **Direct-State Mouse Layer (`src/engine/input.c`):**
  - **Pillar 1 (Tactical Isometric Grid):** Map `(mouse_x, mouse_y)` in the `512×448` PPU viewport to isometric tile `(gx, gy)` using the active camera scroll; write cursor tile coordinates directly to WRAM before the frame step (no synthetic D-Pad). Right/Middle-drag pans the tactical camera.
  - **Pillar 2 (Menus, Hangar, Shop, Coliseum):** Deterministic 2D bounding-box hit-testing for list rows/columns, writing menu slot indices directly to WRAM; left-click confirms (`A`), right-click cancels (`B`).
  - **Pillar 3 (World Map & Keyboard Handoff):** Instant zero-invasion yield when the mouse is stationary; aspect-correct window-to-SNES coordinate mapping.
- **Verify:** `Gate2MouseTest` unit tests + 0-byte idle lockstep drift + interactive playthrough of Town/Hangar/Mission 1 with mouse only.

### Phase 3: 16:9 Widescreen & Read-Only Tactical Telemetry Wings (Gate 3)
- **Master `800×448` Compositor (`src/engine/widescreen.c`):**
  - Display mode cycle (`[F10]`): `16:9 Tactical Wings` / `16:9 Clean Horizon` / `4:3 Classic`.
  - Cold-boot `0x55` WRAM guard before enabling scene detection (`Title`, `Town/Hangar`, `Shop`, `Tactical Map`, `Battle Forecast/Combat`, `Coliseum`).
- **Context-Switched `144×448` Wings (`src/engine/fm_telemetry.c`):**
  - Strictly `const uint8_t *wram, const uint8_t *rom` extractors.
  - **Tactical Map Wings:** Selected Wanzer 4-part HP bars (`Body`, `L-Arm`, `R-Arm`, `Legs`), equipped weapons & hit/damage/range summary, Pilot Proficiency (`Melee/Short/Long/Agility`) & active Skills, plus hovered enemy unit breakdown.
  - **Hangar / Shop Wings:** Power-to-Weight meter (`Output` vs `Weight`), part stat deltas, and total fleet readiness.
  - **Visual QA:** Export headless `.ppm`/`.png` frames for every scene and inspect alongside `memcmp` 0-byte WRAM mutation assertions.
- **Verify:** `Gate3Widescreen16x9Test` & `Gate3TelemetryZeroInvasionTest` pass with 0-byte WRAM mutation.

### Phase 4: Polish, QoL & Cross-Platform Release (Gate 4)
- **PC QoL Features:**
  - `[Esc]` Settings Overlay (Video scale, CRT/scanline optional filter, Audio volume, Fast-Forward `2×`/`4×` toggle for long enemy phases, Mouse sensitivity).
  - Persistent `front_mission.ini` configuration & native `32 KB` `.srm` battery save compatibility.
- **CI & Packaging:**
  - GitHub Actions workflow for Linux, macOS, and Windows (MinGW/MSVC) builds + `ctest` verification suite.

---

## Locked Architectural Decisions

1. **Lifted ROM Baseline — Original `3.0 MB` Japanese ROM (`Front Mission (Japan).sfc`):**
   - `src/gen/` is statically lifted from the untouched 1995 Japanese ROM (`3.0 MB` HiROM).
   - Both Japanese community patches and English translations (`F.H. v1.0b`, `Addendum v1.03`, or future `v1.04+`) are applied **in memory at boot**; `boot_loader.c` diffs the patched ROM against the JP baseline and automatically evicts any modified subroutines from `func_table` so ASM hooks (`JSL $Fxxxxx`, VWF, bugfixes) execute on the `LakeSnes` interpreter.
2. **Patch Loading — Native C In-Memory Patcher + Pre-Patched `.sfc` Fallback:**
   - Primary workflow: Load `Front Mission (Japan).sfc` and apply `.ips` / `.bps` patches in memory from `patches/` via the Pre-Boot Menu (zero Python runtime dependency).
   - Fallback workflow: Also accept an already-patched `.sfc` directly (detecting known CRC32s like `2B4CD5A3` / `5A6D1169` or safely falling back to the interpreter for hooked banks).
