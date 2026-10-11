# Shared AI perception

Enemy creatures and idle friendly workers use `src/game/AIPerception.h` for
range, field of view and geometry-based visibility. Detection uses vertical
separation as well as horizontal distance.

Enemies investigate the strongest audible stimulus at its actual origin.
Supported stimuli are shots, footsteps and physics-object impacts. Silent
events are ignored. Gain scales effective hearing radius, so quiet crouched
footsteps have a shorter detection range than ordinary walking. Occlusion reduces shot range to 70% and quiet sound range
to 35%. Visible targets take priority over distractions; losing visibility
retains the existing last-known-position investigation/search behavior.
Sound events capture source X/Y and absolute height when emitted. Player footsteps
and landing use contact height; shots use the player view or tracked VR weapon.
Object impacts capture their corrected collision height. NPC, sequence-tick and
authored cue origins are separate from the player's position. A direct thrown-object
hit records its impact without disclosing the hidden thrower.

Hearing and playback share a static acoustic trace split at real chunk boundaries.
Exact cached intersections catch thin decks and structures; bounded samples check
doors and moving platforms. Investigation support comes from the source chunk and
source height. Playback uses 3D distance and interpolated-height occlusion while
retaining smooth stereo envelopes. Inaudible voices skip geometry tracing. This is
stereo positional audio, not an HRTF/binaural implementation.

Legacy events/cues without a height retain their fallback behavior. Ambient machine
origins still contain only X/Y and retain planar playback; wasp and water loops use
body-center and surface heights. New custom `SEQUENCE_CUE` records may append an
absolute-height field after the caption; old records remain valid.

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
1 (civilian), 2 (escort) and 3 (guard). A named enable state keeps the authored timeline
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
include the original chunk and actor identity and use a separate namespace from
authored flags, even after the actor moves to another chunk.
Living enabled workers block the player/enemy hull on their own floor; scripted
actors keep their previous collision behavior.
Escorts can follow through aligned 24 m chunks in both axes, including turns
and non-sequential map IDs. The engine keeps one runtime actor with a stable
authored identity, loads its current chunk and next portal when needed, and
draws it in its physical chunk. Save/load restores physical ownership, commands
and death without creating a second actor in the source map. Actors start
following after the player encounters their chunk; remote unseen workers do
not all start chasing the player.

Paired open edges or physically open matching north/south transfer doors allow
crossing. NPCs never force transfer doors. Actual movement samples support and
full hull clearance on both sides; closed edges, steep drops and mismatched
floors stop movement. Follow/Wait and player collision also work while a body
straddles an open seam. Unreachable portal searches use staggered retries.

This is ground navigation through cardinal aligned chunks. It does not add
arbitrary overlapping portal spaces or automatic lift operation. Enemy updates
and friendly combat targeting retain the active player chunk's simulation scope;
this pass does not add full offscreen enemy simulation across the whole chapter.

Full waypoint searches have a shared frame budget and staggered retry timers.
Routine steering uses a short local ground probe; every actual movement step
still checks collision, so budget delays do not permit walking through walls.

## Guard patrol and investigation

Guard mode uses the same runtime NPC registry, collision, door permissions,
navigation budget, animation and save state as civilian/escort actors. It adds
an authored patrol and pauses, a sight cone, recognition delay, actual sound-source
investigation, bounded search and return to patrol. Guards cannot receive player
Follow/Wait commands. They patrol within their home chunk and pause when that chunk
is unloaded; they do not request follower residency or chase through the chapter.

```
ACTOR_AI|0|0|3||1.4|2|6
ACTOR_GUARD|0|0|11|0.65|600|4000|guard_alert
ACTOR_PATROL|0|0|6.5|6.5|0|900
ACTOR_PATROL|0|0|9.5|6.5|0|600
```

Sight range is metres, sight cone is a forward dot threshold, recognition/search
are milliseconds, and patrol Z is absolute feet height. The optional alert flag
is set while a living enabled guard confirms a target. Multiple guards sharing
that flag combine their alert state. Losing sight preserves the last observed
position; a brief glimpse does not become an alert after the player hides. Guard
perception runs after footsteps, thrown objects and shots are emitted this frame.

Ordinary doors in resident neighboring chunks now animate for NPCs without the
player entering that chunk. Transfer doors retain streaming synchronization.
Remembered sound/target positions use global coordinates; patrol points remain
local to the actor's authored home. Save format 13 is unchanged. Partial recognition,
patrol pauses, search and injury/death restore through the existing state store.

Guards stay unarmed unless an `ACTOR_WEAPON` record equips them. The October 11
combat pass adds shared first-hit ballistics, aiming/recovery, a six-round tube,
reloads and attached weapon rendering. First-hit queries test world solids,
current doors and live characters; a guard suppresses fire when a character or
cover blocks its target. Player and tracked VR shots use the same explicit-origin
resolver without temporarily changing the player's view/body pose. Guard bullet
damage cannot be blocked by bare-handed melee guarding.

```
ACTOR_WEAPON|0|0|1|650|1000|9|18
```

Weapon 1 is the existing shotgun; the remaining fields are aiming milliseconds,
recovery milliseconds, range in metres and damage. Empty tubes reload for 1800 ms.
Recognition and weapon raising precede aiming. Fractional milliseconds carry
between frames so higher frame rates do not shorten the attack clocks. Saves
restore aim/recovery/reload and rounds. Armed guards count in the physical chunk's
hostile contacts and enemy-clear gates; civilians/escorts remain separate.

`--guard-combat-test` covers attack timing, interruption, blocking, ammunition,
saves, contacts/gates, thin solids, glass, both door types and real neighbor shots.
`--guard-combat-inspection` captures the actual Ash export from the project root.
Faction tactics and stealth takedowns remain future work.

Armed guards now choose a firing position when they can see the player's head
but cover or another body blocks the barrel. Candidate poses use the same
torso/grip/muzzle and first-hit checks as actual shots. Each search tests at most
eight nearby positions, shares the four-search navigation budget, and backs off
for 600–780 ms. Every movement step still checks support, rails, doors and hull
clearance. Guards keep facing their visible target while sidestepping, lower
their weapon during movement, then raise it and complete the authored aim delay
after stopping. If no direct sidestep
works, ordinary navigation approaches the remembered position around obstacles;
an impassable wall or locked door still blocks movement and shots.

Repositioning goals and retry timing save through the existing state store;
malformed values are rejected on load. `--guard-tactics-test` covers progress,
safety, 60/120 Hz updates, save continuation and crowded navigation. The actual
Ash export is inspected with `--guard-tactics-inspection` from the project root.
The [custom Ash playground](ashfall-playground.md) uses the existing Worker rig
to test these systems until the external asset library is available again.
`--guard-ai-test` covers the shared state machine, real-frame sound ordering,
save validation, locks, neighbor doors, navigation budget and authoring validation.
`--guard-inspection` loads that actual exported custom map from the project root,
checks its full four-point patrol behind cover and captures three states in Vulkan.

## State storage

Script/NPC state uses sorted storage and binary search instead of scanning all
flags on every query. Saves allow up to 8,192 state values while retaining the
existing smaller limits for other collections and the overall 2 MB file limit.
Older insertion-ordered state payloads are sorted on load; duplicate IDs are
rejected. The systems regression compares indexed and linear results and loads
a 4,095-flag unsorted payload.

`--hearing-test` covers actual crouched/standing movement, captured gun and impact
origins, stacked floors, thin decks, walls, neighboring chunks, actor ticks,
strongest-stimulus selection, lost sight, invalid inputs and cue compatibility.
`--audio-test` covers offline stereo playback, elevation, occlusion and source
retention alongside existing mixing/ambience checks.
`--physics-ai-test` covers real-source investigation, silent/occluded footsteps,
friendly reactions, walls, dead targets and authored animation preservation,
alongside stairs, terrain, stalking and contact combat. `--mechanisms-test` and
`--campaign-map-test` protect the worker encounter and saved actor timelines.
`--friendly-ai-test` covers following at 60/120 Hz, PC/VR commands, wall detours,
door permissions, fleeing, save restoration, animated death and enemy attacks
on friendly actors. It also covers streaming every frame, both directions across
seams, a two-axis turn, open/closed paired transfers, a player in the doorway,
invalid saves and migrated commands/corpses. The Vulkan smoke suite includes the
state-store benchmark.
