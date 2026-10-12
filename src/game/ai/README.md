# AI layout

NPC and creature logic lives under `src/game/ai/` by **shared capability**, then **mode**, then thin **role hooks**.

| Path | Owns |
|------|------|
| `Perception.h` / `Navigation.h` / `Hearing.cpp` | Shared senses and movement primitives |
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

`Schedule.h` defines shared task identities, interrupt masks and allowed work.
`Schedule.cpp` owns prepare, physical movement, work, completion and bounded
failure recovery. Mind updates fresh senses before execution. Movement reports
actual waypoint progress; deferred navigation does not charge failed time.
Combat, grenades, treatment and Engineer hardware must hold the running work
task before consuming an event. Completed attacks may finish their animation;
an interrupted reload keeps its remaining time and consumed event mask.
See `docs/guard-schedule-pass-20261012.md` for the validation and parity boundary.
