# Front Mission (SNES) — Walkthrough-Driven CDL Trace Plan

This document maps the actual gameplay progression and UI/engine modes of *Front Mission (1995, SNES)* to plan **Phase 1 CDL (Code/Data Log) capture**, **Gate 2 Mouse UI models**, and **Gate 3 16:9 Telemetry Wings**.

---

## 1. Complete Gameplay Progression & Mode Transitions

1. **Boot $\rightarrow$ Title $\rightarrow$ Opening Cutscene**
   - Boot sequence $\rightarrow$ Title screen (`New Game` / `Continue`) $\rightarrow$ Larcus District prologue cutscene (Lloyd, Karen, Driscol) $\rightarrow$ transitions directly into **Mission 1: Larcus Incident**.
2. **Tactical Mission Loop (Mission 1+)**
   - **Isometric Grid Cursor:** `D-Pad` moves cursor; `A` selects unit / confirms move / attacks; `B` cancels or opens unit context menu (`Move`, `Machine Status`, `Pilot Status`, `Equipment View`); `L`/`R` cycles units; `Y`/`X`/`Select` jumps to unit or inspects enemy movement range (`Y` twice on enemy); `Start` opens Mission System Menu (`Player Phase End`, `Configuration` Fast/Slow, `Interruption` suspend save).
   - **Post-Move Action Menu:** `Attack`, `Use Item` (`Repair S`, `Flash`, `Chaff`), `End`, `Cancel`, or `Supply` (when starting/standing adjacent to a Supply Truck like Peewee).
   - **Combat & Counterattack:** Short-range (`MG`, `Rifle`, `Shotgun`, `Flamethrower`), Melee (`Rod`, `Tonfa`, `Knuckle`), Long-range (`Missile`, `Bazooka`, `Grenade Launcher`), `Guard` (with/without Shield). Battle animation cutscene tracks 4-part HP (`Body`, `L-Arm`, `R-Arm`, `Legs`), Skill triggers (`Duel`, `Guide`, `Switch`, `Speed`, `First`, `Stun`, `Double`), and 4-pool Proficiency EXP (`Fight`, `Short`, `Long`, `Agility`).
   - **Post-Mission Resolution:** Story dialogue $\rightarrow$ Commission payout & destroyed-Wanzer repair expense deductions $\rightarrow$ scripted 1v1 Coliseum-style duel (Sazaland after Mission 1) $\rightarrow$ Town arrival (**Barinden**).
3. **Town Hub Loop (`Shop`, `Military Office`, `Bar`, `Coliseum`, `Status`, `Set Up`, `Save/Load`, `Exit`)**
   - **`Shop`:** `Set up` (compare-before-buying), `Buy`, `Sell`, `Talk`, `Exit`.
   - **`Set Up`:** `Machine` (Body/L-Arm/R-Arm/Legs/Backpack/COM/Weapons/Items, Power-to-Weight `W/P` budget, Paint, Rename) and `Pilot` (Proficiency EXP & Skills).
   - **`Coliseum`:** `Entry` (1v1 opponent & odds selection, betting, combat with `Give up` option) and `VS Play` (2-player password battle).
   - **`Military Office` & `Bar`:** Mission briefings, story progression, and secret pilot recruit triggers (e.g., Paul, Alder, Yang, Gregorio).
   - **`Save/Load`:** Native 32 KB `.srm` battery save.
4. **Huffman Island World Map**
   - `Exit` from town opens the island map (`B` previews destination) to travel between towns and trigger the next mission's **Pre-Battle Unit Deployment** screen.

---

## 2. Planned Phase 1 Canonical CDL Traces (`.fmrec`)

To achieve complete `M`/`X` flag-width coverage, jump-table discovery, and Gate 1 `RM_BOTH` lockstep parity, record these **6 headless traces**:

| Trace File | Sequence Covered | Key Subsystems Exercised |
| :--- | :--- | :--- |
| `trace_01_boot_to_mission1.fmrec` | Reset $\rightarrow$ Title $\rightarrow$ Opening Cutscene $\rightarrow$ Mission 1 start | Cold-boot DMA, NMI/HDMA, cutscene VM, dialogue text, isometric map init |
| `trace_02_tactical_combat_items.fmrec` | Mission 1 Player Phase + Enemy Phase + `Interruption` suspend | Cursor/pathfinding, `B`/`Y`/`L`/`R` menus, `Use Item`, MG/Missile/Melee combat, Guard/counter, suspend save |
| `trace_03_mission1_clear_to_town.fmrec` | Mission 1 clear $\rightarrow$ Commission/Repair $\rightarrow$ Sazaland duel $\rightarrow$ Barinden | Victory triggers, payout math, scripted 1v1 battle, town hub init |
| `trace_04_town_shop_setup_save.fmrec` | Barinden `Shop` (`Set up`/`Buy`/`Sell`), `Set Up`, `Bar`, `Military Office`, `Save/Load` | Inventory WRAM (`$7E:CF80+`), `W/P` calculation, paint/rename, `.srm` save/load |
| `trace_05_coliseum_arena.fmrec` | Barinden `Coliseum` (`Entry`, odds/betting, 1v1 duel, `VS Play` password) | Odds/betting math, 1v1 AI, password encoder |
| `trace_06_worldmap_mission2_supply.fmrec` | `Exit` $\rightarrow$ Huffman Map $\rightarrow$ Deployment screen $\rightarrow$ Mission 2 + Supply Truck | World map navigation, unit deployment cap, `Supply` resupply/repair |

### 2.1 Practical Trace Recording Strategy (Dialogue & Confirmation Handling)
*Front Mission* has long, multi-box dialogue sequences and confirmation prompts (especially the Larcus prologue and mid-mission Driscol/Karen events). Blindly hardcoding frame offsets for `A` presses will desync as soon as text speed or fade timing shifts by a frame. To avoid stalling on `trace_01`–`trace_02`:
1. **State-Aware Trace Generator:** The trace recorder polls WRAM/OAM state (e.g., dialogue cursor/prompt active vs. tactical grid cursor active) and pulses `A` on a steady cadence (e.g., every 6–8 frames) to clear dialogue/confirmations until the target gameplay state is reached, recording the resulting **exact frame-by-frame joypad bitmasks** into `.fmrec`.
2. **Checkpoint Chaining:** `trace_01` starts from cold reset (`snes_reset()`); subsequent traces (`trace_02`–`trace_06`) can chain from the deterministic end state of the previous trace (or replay from boot in `RM_BOTH`) so we never have to re-script the entire prologue just to reach Town/Coliseum/Mission 2.
3. **Visual Frame Checkpoints:** Export `.ppm`/`.png` snapshots at each trace's start/end to confirm we actually reached the intended screen before running CDL or Gate 1 lockstep.

---

## 3. Design Implications for Later Gates

- **Gate 2 (Three-Pillar Direct-State Mouse):** Must handle 4 distinct cursor models: (1) Isometric Tactical Grid, (2) Town/Shop/Set Up multi-column menus, (3) Huffman Island World Map, and (4) Battle/Skill part-targeting (`Duel`/`Guide`) popups.
- **Gate 3 (`800×448` 16:9 Telemetry Wings):**
  - **Tactical Map:** Selected Wanzer 4-part HP (`Body`/`L-Arm`/`R-Arm`/`Legs`), weapon range & missile ammo count, terrain evasion %, Supply Truck adjacency cue, and Pilot Skill trigger rates.
  - **Hangar / Shop:** Live **Power-to-Weight (`W/P`)** margin gauge and stat deltas.
  - **Coliseum:** Opponent pilot proficiencies (`Fight`/`Short`/`Long`/`Agility`) & Wanzer parts vs. your unit.
