# Utility campaign: chapters 7–10

The chapter numbers shown in the HUD are one-based; console map IDs are
zero-based. All ground and upper layer arrays live in `src/world/World.cpp`.

| Chapter | Map ID | Traversal and control |
| --- | --- | --- |
| Cable Vaults | 6 | Enter from the southwest bulkhead in Coolant Return. Detour around the flooded electrical trench, operate the local disconnect, collect the maintenance flashlight, then open Pump Annex. |
| Pump Annex | 7 | Descend from the main floor into the lower manifold, climb the two-stage stair to observation, and cross to the upper exit. Crossing observation starts the pump event. |
| Utility Junction | 8 | Arrive on the bridge, descend to the control booth and release Waste Handling. Freight and Primary Utilities remain suspended future connections. |
| Waste Handling | 9 | Follow the sorting deck down to processing. Use the local isolator to raise and stop the press; the outer service route also bypasses it. Freight dispatch is the current endpoint. |

Pump Annex has floors at -12, -9 and -4 m. Utility Junction receives the -4 m
route and descends to -9 m. Waste Handling receives -9 m and descends to -12 m.
Every stair and deck is rendered and collided from the same structure records.

The local controls use named state IDs. Electrical hazards can have periods,
active durations and phase offsets. The compactor's visible platen and damage
volume use the same cycle function. Its conveyor moves low objects and actors;
its closed stroke damages the player, kills creatures and removes crushed junk.
Isolation stops the conveyor and holds the platen raised. There is no invisible
permanent kill floor.

Map-owned door signs, ambient light, overhead pipes, terminals and compactor
records do not depend on special renderer map IDs. Sparse decks remain separate
map arrays. Save version 11 records chunk count and imports six-chunk version
10 campaign saves without discarding their player or door state.

Validation commands:

- `--campaign-extension-test`: standing-hull routes at all elevations, forward
  and reverse seam traversal, E controls, periodic arcs, compactor victims and
  save restoration.
- `--campaign-inspection`: nine player-height views across the four chapters.
- `--service-map-test`: older gallery/coolant routes, shore ramps and buoyancy.
- `--smoke-test --vulkan`: gameplay, audio, save, material and rendering checks.
- `--world-isolation-test --vulkan`: Ashfall and campaign separation.
- `--performance-test`: full-resolution turning benchmarks, including each new
  chapter, plus conservative culling equivalence.

Freight Access, a return elevator shortcut, a movable maintenance bridge and
further narrative set pieces remain future additions. This release implements
the connected playable route, rather than representing those future passages
as open doors into absent maps.
