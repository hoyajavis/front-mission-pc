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
