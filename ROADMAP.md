# Front Mission (SNES) Native PC Port — Roadmap

## Gate Status
- [ ] **Phase 0: Repository Hygiene, Build Skeleton & HiROM Boot (Gate 0)** — *In Progress*
  - [x] `AGENTS.md` (Mandatory GitHub push discipline)
  - [x] `.gitignore` & `tools/git-hooks/pre-commit`
  - [ ] `ROADMAP.md`, `DEVLOG.md`, `CMakeLists.txt` (`-Wall -Wextra -Werror`, ASan/UBSan preset)
  - [ ] Port `snes_ui_rasterizer.c`, `config.c`, `gui_menu.c`
  - [ ] Native C IPS/BPS in-memory patcher + `0xFFC0` HiROM checksum updater (`boot_loader.c`)
  - [ ] Verify JP & EN v1.03 boot to title screen with audio via `LakeSnes`
- [ ] **Phase 1: CDL Capture, HiROM Static Lifting & Lockstep Oracle (Gate 1)**
- [ ] **Phase 2: WRAM Mapping & Three-Pillar Direct-State Mouse Input (Gate 2)**
- [ ] **Phase 3: 16:9 Widescreen & Read-Only Tactical Telemetry Wings (Gate 3)**
- [ ] **Phase 4: Polish, QoL & Cross-Platform Release (Gate 4)**
