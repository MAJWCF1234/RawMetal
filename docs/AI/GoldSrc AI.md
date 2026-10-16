---
tags: [ai, goldsrc]
aliases: [goldsrc-ai-gap-analysis, GoldSrc gap, GoldSrc AI]
date: 2026-10-16
---

# GoldSrc AI

Capability comparison to Valve Half-Life schedules/nodes. **Not cost parity** — see [[Performance/Exclusion Zone]].

Valve refs: [schedule.h](https://github.com/ValveSoftware/halflife/blob/master/dlls/schedule.h) · [defaultai.cpp](https://github.com/ValveSoftware/halflife/blob/master/dlls/defaultai.cpp) · [monsters.h](https://github.com/ValveSoftware/halflife/blob/master/dlls/monsters.h) · [nodes.cpp](https://github.com/ValveSoftware/halflife/blob/master/dlls/nodes.cpp).

## Capability status

Overall equality remains unproven. Implemented:

- Guard task ownership, finite support jobs, streamed pursuit
- Retained cover/path lattice with `FIND_*_COVER` helpers
- Creature / civilian / scripted `MaintainSchedule`
- Authored `NAV_HINT` nodes and typed mind accessors

| Priority | Requirement | Boundary |
| --- | --- | --- |
| P0 | Task lists + failure schedules | Shared executor; planning still mode/role dispatch |
| P0 | Conditions + sound interrupts | Catalog-filtered categories |
| P0 | Node/hint graph | Lattice + authored Path/Cover + FIND_* |
| P1 | Creatures on executor | `enemy/Schedule.cpp` |
| P1 | Scripted ↔ autonomous | Fear → Flee, then clear |
| P1 | Civilian/escort schedules | Intent still chooses goals |
| P2 | Typed mind | Hot fields via `GuardCtx` / `CivilianMindView` |

## Cost gap

GoldSrc was cheap because of **sparse queries**, not free schedules.

| GoldSrc trick | RawMetal |
| --- | --- |
| Node graph is walkability | Live `fits`/`support` still own steps |
| One hull move | Hull-scale pitch landed; not TraceHull authority |
| Sparse LOS / think intervals | Calm patrol LOD only |
| BSP + PVS | Not done |
| Studio models | CPU attachment remains |

Also: [[AI/ai-perception|Perception]] · [[AI]] · archived passes in [[Archive/Passes]]
