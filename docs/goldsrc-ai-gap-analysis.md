# GoldSrc AI gap analysis

Overall equality remains unproven. Guard task ownership, finite support jobs,
streamed pursuit, a retained cover/path lattice with FIND_*_COVER catalogue
helpers, creature/civilian/scripted MaintainSchedule participation, authored
NAV_HINT nodes and typed mind accessors over save-compatible state keys are
implemented.

| Priority | Requirement | Current boundary |
| --- | --- | --- |
| P0 | Declarative task lists and explicit failure schedules | Guard, creature and civilian catalogs use the shared bounded executor; planning is separate mode/role dispatch. |
| P0 | Broad conditions and schedule-specific sound interrupts | Guards collect audible categories, occlusion, witnessed death, physical ranged/melee readiness and fresh light/heavy damage. Creatures collect sight, hearing, melee readiness and pain. Civilians collect fear, follow, hurt and proximity. Catalog rows filter interrupts and sounds. |
| P0 | Node/hint graph for routes and cover | Done: retained per-chunk cover/path lattice, authored `NAV_HINT` Path/Cover markers, and FIND_*_COVER catalogue helpers (`findLateralCover`, `findCoverFromEnemy`, near/far/origin/sound node cover). |
| P1 | Creatures use the same executor | Done: `enemy/Schedule.cpp` binds Idle/Investigate/Search/Chase/Attack through `MaintainSchedule`; sensory Enemy::State and species locomotion stay in `EnemyAI.cpp`. |
| P1 | Scripted and autonomous AI handoff | Done for fear: `scriptedHandoffIntent` adopts Flee Escape through the civilian catalog, then clears when fear ends so the timeline regains control. |
| P1 | Civilian/escort schedule participation | Done: `friendly/Schedule.cpp` binds Idle/Follow/Flee/Hold/Recovery; intent still chooses goals while the executor owns phase and work. |
| P2 | Typed per-NPC mind with save compatibility | Done for hot fields: `GuardCtx` and `CivilianMindView` typed accessors over the existing `friendlyKey`/`m_states` store; creature schedule mind stays ephemeral on `Enemy` and rebuilds after load. |

The latest slice preserves Medic handgun/kit work, Engineer pistol/terminal
and finite five-drone supervision, attack event masks, squad leases and
multi-chunk pursuit. Wounded moving squad members can now rendezvous with
their Medic while local anchors cover actual support work.

Guards, creatures, civilians/escorts and scripted fear handoff share
MaintainSchedule with a retained cover catalogue and typed mind views.
Acceptance still requires matched encounters and broader mind-field migration
beyond the hot set, while preserving these existing systems. A reusable
kernel alone does not prove overall parity.

Primary comparison: Valve's [schedule definitions](https://github.com/ValveSoftware/halflife/blob/master/dlls/schedule.h),
[default schedules](https://github.com/ValveSoftware/halflife/blob/master/dlls/defaultai.cpp),
[monster conditions](https://github.com/ValveSoftware/halflife/blob/master/dlls/monsters.h)
and [navigation graph](https://github.com/ValveSoftware/halflife/blob/master/dlls/nodes.cpp).
RawMetal's implementation is independent; this is a capability boundary,
not a claim of identical Half-Life behavior.
