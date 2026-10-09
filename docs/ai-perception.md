# Shared AI perception

Enemy creatures and idle friendly workers use `src/game/AIPerception.h` for
range, field of view and geometry-based visibility. Detection uses vertical
separation as well as horizontal distance.

Enemies investigate the strongest audible stimulus at its actual origin.
Supported stimuli are shots, footsteps and physics-object impacts. Silent
events are ignored. Occlusion reduces shot range to 70% and quiet sound range
to 35%. Visible targets take priority over distractions; losing visibility
retains the existing last-known-position investigation/search behavior.
Player-relative audio uses the player position and eye elevation. Spatial
audio currently stores a 2D source, so perception estimates its elevation from
the source floor; explicit sound elevations remain future engine work.

Idle workers turn toward visible living enemies at up to four radians per
second. Dead enemies, blocked sight and neighboring chunks cannot provide a
target. Authored walking, attacks, death and explicit look-at keys retain
control for default scripted actors.

## Autonomous friendly actors

Workers can opt into civilian or escort AI through the shared actor definition:

```
ACTOR_HEALTH|0|0|70|worker_damage|worker_dead
ACTOR_AI|0|0|2||1.4|2|6
```

`ACTOR_AI` fields are map index, actor index, mode, optional enable-state name,
walking speed, follow distance and danger range. Modes are 0 (scripted),
1 (civilian) and 2 (escort). A named enable state keeps the authored timeline
in control until that flag is set. Actors still need their normal `ACTOR`,
`ACTOR_KEY` and `TIMED_SEQUENCE` records. Health is required for enemies to
target a worker; omit health only for intentionally invulnerable actors.

Escorts follow by default; civilians wait until commanded. E or a controller
grip near a visible worker toggles Follow/Wait. Doors, terminals and held
objects retain their existing interaction priority. Follow distance uses a
dead band to prevent constant animation toggling beside the player.

Visible enemies cause fleeing, with four seconds of last-known threat memory.
Enemy creatures can choose enabled, living friendly actors as targets and
damage them through contact attacks. Workers also react to injuries and avoid
moving closer through an enemy's body. Death plays the authored animation over
one second instead of immediately snapping to its last pose.

Navigation checks the full hull, support, rails and doors. Workers route around
walls, request ordinary unlocked doors and retry blocked movement. State/control
locks and transfer/entry doors retain their restrictions. Position, fear,
commands and death animation restore through the normal save system. NPC keys
include chunk and actor identity and use a separate namespace from authored flags.
Living enabled workers block the player/enemy hull on their own floor; scripted
actors keep their previous collision behavior.
Following currently stays within the actor's authored chunk; automatic escort
migration across stitched chunk boundaries remains unsupported.

Full waypoint searches have a shared frame budget and staggered retry timers.
Routine steering uses a short local ground probe; every actual movement step
still checks collision, so budget delays do not permit walking through walls.

## State storage

Script/NPC state uses sorted storage and binary search instead of scanning all
flags on every query. Saves allow up to 8,192 state values while retaining the
existing smaller limits for other collections and the overall 2 MB file limit.
Older insertion-ordered state payloads are sorted on load; duplicate IDs are
rejected. The systems regression compares indexed and linear results and loads
a 4,095-flag unsorted payload.

`--physics-ai-test` covers real-source investigation, silent/occluded footsteps,
friendly reactions, walls, dead targets and authored animation preservation,
alongside stairs, terrain, stalking and contact combat. `--mechanisms-test` and
`--campaign-map-test` protect the worker encounter and saved actor timelines.
`--friendly-ai-test` covers following at 60/120 Hz, PC/VR commands, wall detours,
door permissions, fleeing, save restoration, animated death and enemy attacks
on friendly actors. The Vulkan smoke suite includes the state-store benchmark.
