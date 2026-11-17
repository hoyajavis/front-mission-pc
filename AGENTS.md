# Agent Guidelines — `front-mission-pc`

## 1. Ephemeral VM & GitHub Push Discipline (Mandatory)
Cloud VMs are ephemeral outside `~/.gemini`; local-only `git commit`s are lost if a VM resets or a long-running command times out (`RPC::CANCELLED`).

- **Push before every long-running command:** Before running any potentially long-running or CPU-intensive task (such as `cdl_collector`, `verify_trace` / `RM_BOTH` lockstep runs, full ROM lifting, or multi-minute builds/tests), **stage, commit, and `git push` all current source/config changes to GitHub (`origin`)**.
- **Push after every sub-milestone:** Never batch an entire phase locally. Commit and push incrementally as each self-contained component builds/passes.
- **Keep ROMs/patches/saves out of git:** Never commit `.sfc`, `.smc`, `.ips`, `.bps`, `.fmsav`, or `.srm` files. Verify `git status` before committing.
