# AI layout

NPC and creature logic lives under `src/game/ai/` by **shared capability**, then **mode**, then thin **role hooks**.

| Path | Owns |
|------|------|
| `Perception.h` / `Navigation.h` / `Hearing.cpp` | Shared senses and movement primitives |
| `Nodes.h` / `NavGraph.cpp` | Retained per-chunk hint/cover graph and live path hops |
| `Scheduler.h` | Actor-independent bounded task-list execution |
| `enemy/` | Creature AI (`updateEnemies`) |
| `friendly/` | Streaming, tick orchestration, civilian/escort intent |
| `guard/` | Awareness (Mind), shared schedule executor, combat, tactics, danger, squads, roles (`Config.h`), `GuardCtx` |
| `engineer/` | Drone hardware + Engineer supervision |
| `grenades/` | Tactical grenades |
| `debug/` | Visual inspection harnesses |
| `guard/tests/` | Guard regression entry points |

**Expand here:** new shared behavior → `guard/` (or `friendly/`). New mercenary role → `Config.h` profile + optional hook in `Support.cpp`. Prefer `GuardCtx` / `makeGuardCtx` over new get/put lambdas.

Engineer hardware owns the finite five-unit fleet and one-recipient maintenance
clock. Supervision respects shared defensive cover before choosing a rear goal;
close threats interrupt work and grenade danger takes priority. See
`docs/engineer-support-pass-20261012.md` for that integration's runtime evidence.

`guard/Schedule.h` preserves saved schedule/task identities. `ScheduleTables.h`
declares named task lists, arguments, failure destinations, sound interests,
interrupt masks and permitted work. `Plans.cpp` selects a typed intent through
mode and role dispatch; `Schedule.cpp` binds that intent to `Scheduler.h` and
owns completion and failure recovery. Mind updates fresh senses before execution. Movement reports
actual waypoint progress; deferred navigation does not charge failed time.
Combat, grenades, treatment and Engineer hardware must hold the running work
task before consuming an event. Completed attacks may finish their animation;
an interrupted reload keeps its remaining time and consumed event mask.
See `docs/guard-schedule-pass-20261012.md` for the validation and parity boundary.

`Casualties.cpp` binds a wounded moving squad member to its reserved Medic.
The patient withdraws to a fixed, physically reachable rear rendezvous and
holds through treatment, then resumes its authored role. `Squads.cpp` grants
local covering opportunities for reload, retreat, treatment and drone work;
the shooter still needs its own hostile memory and a safe physical shot.
See `docs/squad-support-tables-pass-20261012.md` and
`docs/goldsrc-ai-gap-analysis.md` for current evidence and remaining work.

Creatures now run Idle/Investigate/Search/Chase/Attack through
`enemy/Schedule.cpp` and `MaintainSchedule`. Species locomotion, Warden
stalking and Huntsman climb modes stay in `EnemyAI.cpp` and are gated by the
active schedule phase. Civilians/escorts run Idle/Follow/Flee/Hold/Recovery
through `friendly/Schedule.cpp`; scripted actors hand Flee Escape to that
catalog while fear lasts, then return to timeline control.

`Nodes.cpp` builds a retained cover/path lattice beside solid structures and
merges authored `NAV_HINT` Path/Cover markers from custom maps. FIND_*_COVER
catalogue helpers (`findLateralCover`, `findCoverFromEnemy`, near/far/origin/
sound node cover) feed defensive `guardReposition`. Guard movement uses graph
hops before the span flood. Live walk tests still own doors and dynamic bodies. Friendly NPCs share
player jump/crouch gravity (`vv`/`grounded`/`crouched`): crouch under low
clearance, defensive cover, or a crouched followed player; hop ledges up to
~1.05 m when the walk step cannot.

`GuardCtx` and `friendly/Mind.h` (`CivilianMindView`) expose typed accessors
over the same `friendlyKey` save keys. Creature schedule fields on `Enemy`
remain ephemeral and rebuild after load.
