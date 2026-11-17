# Front Mission (SNES) Native PC Port — Roadmap

## Gate Status
- [x] **Phase 0: Repository Hygiene, Build Skeleton & HiROM Boot (Gate 0)** — *Passed*
  - [x] `AGENTS.md` (Mandatory GitHub push discipline)
  - [x] `.gitignore` & `tools/git-hooks/pre-commit`
  - [x] `ROADMAP.md`, `DEVLOG.md`, `CMakeLists.txt` (`-Wall -Wextra -Werror`, ASan/UBSan preset)
  - [x] Port `snes_ui_rasterizer.c`, `config.c`, `gui_menu.c`
  - [x] Native C IPS/BPS in-memory patcher + `0xFFC0` HiROM checksum updater (`boot_loader.c`)
  - [x] Verify JP (`FE27E061`), EN F.H. v1.0b, and EN + Addendum v1.03 (`4 MB`) boot with video & SPC700 audio via `LakeSnes`
- [x] **Phase 1: CDL Capture, HiROM Static Lifting & Lockstep Oracle (Gate 1)** — *Passed*
  - [x] `.fmrec` input replay layer (`replay.c`) & canonical JP/EN traces (`fm_record_traces`)
  - [x] HiROM CDL collector (`fm_cdl_collector`) & manifest (`config/routines_to_lift.txt`, 521 entries)
  - [x] HiROM static lifter (`tools/lift_all_banks.py`, 139 routines across Banks `$C1, $C2, $C3, $C7, $C9` with Bank `$C0`/`$EA` on `LakeSnes`)
  - [x] `RM_BOTH` lockstep oracle (`fm_verify_trace`) + HiROM `func_table` patch eviction (`test_replay_and_eviction`)
- [ ] **Phase 2: WRAM Mapping & Three-Pillar Direct-State Mouse Input (Gate 2)**
- [ ] **Phase 3: 16:9 Widescreen & Read-Only Tactical Telemetry Wings (Gate 3)**
- [ ] **Phase 4: Polish, QoL & Cross-Platform Release (Gate 4)**
