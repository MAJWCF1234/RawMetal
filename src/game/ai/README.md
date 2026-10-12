# AI layout

NPC and creature logic lives under `src/game/ai/` by **shared capability**, then **mode**, then thin **role hooks**.

| Path | Owns |
|------|------|
| `Perception.h` / `Navigation.h` / `Hearing.cpp` | Shared senses and movement primitives |
| `enemy/` | Creature AI (`updateEnemies`) |
| `friendly/` | Streaming, tick orchestration, civilian/escort intent |
| `guard/` | Guard FSM (Mind), combat, tactics, danger, squads, roles (`Config.h`), `GuardCtx` |
| `engineer/` | Drone hardware + Engineer supervision |
| `grenades/` | Tactical grenades |
| `debug/` | Visual inspection harnesses |
| `guard/tests/` | Guard regression entry points |

**Expand here:** new shared behavior → `guard/` (or `friendly/`). New mercenary role → `Config.h` profile + optional hook in `Support.cpp`. Prefer `GuardCtx` / `makeGuardCtx` over new get/put lambdas.
