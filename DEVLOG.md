# Development Log — `front-mission-pc`

*(Archive rotation rule: rotate entries to `docs/devlog_archive_*.md` every 400–500 lines.)*

## 2026-11-16 — Phase 0 (Gate 0) Complete
- Codified ephemeral VM GitHub push discipline in `AGENTS.md`.
- Initialized `.gitignore`, `tools/git-hooks/pre-commit`, `ROADMAP.md`, and `scripts/setup_third_party.sh`.
- Cloned `sp00nznet/snesrecomp` + `LakeSnes` into `third_party/snesrecomp`.
- Ported BGR555 9-slice UI rasterizer (`snes_ui_rasterizer.c`), INI config (`config.c`), and Pre-Boot Launcher / Settings overlay (`gui_menu.c`).
- Implemented native C IPS & BPS in-memory patchers, CRC32/SHA-1, `0xFFC0` HiROM checksum updater, and modified-range tracking in `boot_loader.c` (with 512-byte SMC offset compensation for `FMSNE10B.IPS`).
- Verified `ctest` (`test_boot_loader` 1/1 passed under ASan/UBSan + `-Wall -Wextra -Werror`).
- Verified `fm_boot_verify` across all 3 profiles (JP Original 3 MB, EN F.H. v1.0b, EN + Addendum v1.03 4 MB) with active PPU video and SPC700 audio.

## 2026-11-16 — Phase 1 (Gate 1) Complete
- Implemented `.fmrec` input replay format (`include/replay.h`, `src/engine/replay.c`) and recorded canonical headless traces (`tools/record_traces.c`):
  - `jp_trace_01_boot_title.fmrec` (568 frames: Boot -> Title)
  - `jp_trace_02_prologue_mission1.fmrec` (11,861 frames: Title -> Name Entry -> Larcus Prologue -> Mission 1)
  - `jp_trace_03_mission1_tactical.fmrec` (12,308 frames: Mission 1 Tactical Map)
  - `en_trace_01_boot_vwf.fmrec` (3,431 frames: EN + Addendum v1.03 Boot -> Title -> VWF)
- Built HiROM CDL collector (`tools/cdl_collector.c`, `tools/opcodes_65816.h`) using `LakeSnes` `g_cpuRecompHook`; decoded 269.4M instructions across 35,534 code bytes and generated `config/routines_to_lift.txt` (521 single-width routines).
- Built HiROM static lifter (`tools/lift_all_banks.py`) producing `src/gen/fm_bank_{C1,C2,C3,C7,C9}.c` (139 lifted routines, top-128 ranked by CDL hit count for `recomp_timed_add_intercept`, keeping Bank `$C0` system/NMI and Bank `$EA` SPC700 driver on `LakeSnes`).
- Built `RM_BOTH` lockstep oracle (`tools/verify_trace.c`) and per-routine HiROM `func_table` patch eviction (`fm_hirom_addr_to_offset`, `fm_patch_range_overlaps`).
- Verified all 139 lifted routines in lockstep validation across 24,737 JP frames (0 register/M-X/VRAM/CGRAM/OAM/SRAM errors) and verified `en_trace_01_boot_vwf.fmrec` (3,431 frames, 0-byte drift with patch eviction).
- Verified `ctest` (`test_boot_loader` and `test_replay_and_eviction`, 2/2 passed under ASan/UBSan + `-Wall -Wextra -Werror`).

## 2026-11-16 — Phase 2 (Gate 2) Complete
- Detailed step-by-step documentation archived in `docs/phase2_wram_and_mouse.md`.
- Mapped core WRAM addresses via `tools/wram_probe.c` against `jp_trace_04_mission1_combat.fmrec` and `src/gen/fm_bank_C1.c`:
  - Tactical cursor screen coordinates: `$7E:02A0` (X), `$7E:02A2` (Y)
  - Camera scroll mirrors: `$7E:02AE/$02B0` (target), `$7E:0D72..$0D7C` (BG1–BG3 H/V), `$7E:0D8A..$0D94` (shadow mirrors)
  - Menu cursor index & mirror: `$7E:0334`, `$7E:0366`
  - Joypad held/pressed/released: `$7E:0DC8..$0DD3`
  - Active unit structs (`$7E:D580`, `$7E:D60D`, stride `0x8D`) & inventory (`$7E:CF80–$7E:D1FF`)
- Created non-breaking `FmWramOverlay` C union (`include/fm_wram.h`) with compile-time `_Static_assert` offset verification.
- Implemented Three-Pillar Direct-State Mouse Input (`include/input.h`, `src/engine/input.c`) and wired into `src/main.c`:
  - Aspect-correct `4:3` and `16:9` (`800×448` with `144px` wings) coordinate transform (`fm_input_window_to_snes`)
  - Pillar 1: Isometric `32×16` tile projection + direct WRAM cursor write + Right/Middle-drag camera pan
  - Pillar 2: Deterministic 2D bounding-box menu hit-testing + direct menu slot write + Left/Right-click `A`/`B` injection
  - Pillar 3: Instant zero-invasion yield when mouse is stationary (0-byte WRAM mutation)
- Verified `ctest` (`test_boot_loader`, `test_replay_and_eviction`, `Gate2MouseTest` — 3/3 passed under `-Wall -Wextra -Werror` + ASan/UBSan).

