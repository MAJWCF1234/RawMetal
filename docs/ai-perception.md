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
position and confirmed entity across brief occlusion; an unconfirmed glimpse
does not become an alert after the target hides. A confirmed target is recognised
immediately when seen again, but interrupted weapon aim still starts afresh.
Finishing investigation/search clears confirmation, so a later encounter needs
the normal recognition delay. Guard
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
restore aim/recovery/reload and rounds. Enabled living armed guards hostile to the
player count in the physical chunk's contacts and enemy-clear gates; allied guards
and civilians/escorts do not.

`--guard-combat-test` covers attack timing, interruption, blocking, ammunition,
saves, contacts/gates, thin solids, glass, both door types and real neighbor shots.
`--guard-combat-inspection` captures the actual Ash export from the project root.
Faction targeting and bounded squad tactics are implemented below; stealth
takedowns remain future work.

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

## Factions, squads and recovery

`ACTOR_FACTION|map|actor|faction|squad` accepts faction 0 (default), 1 (Player),
2 (Facility), 3 (Creature), 4 (Raider) or 5 (Neutral). Omitted/default factions
resolve guards to Raider, other Worker actors to Facility, Wasp/Huntsman actors
to Creature and other visuals to Neutral. Player and Facility are allied; Creature
and Raider are hostile to each other and to those allies. Neutral actors are not
automatic targets. The last identified personal attacker can still provoke
retaliation, including an actual creature attack from behind.

Guards choose living entities through these relationships instead of always
attacking the player. Target references retain entity kind, level and index;
memory stores the last observed coordinates rather than revealing a hidden
target's live position. A dead, disabled or unloaded target cannot be fired upon.
The shared first-hit query protects intervening allies and neutral bystanders.

A nonempty squad name groups guards of the same faction. Fresh personal sightings
can inform a teammate within 18 m; a report supplies last-known information, not
the receiver's own visibility. At most two eligible members hold firing leases.
Leases last `max(1600, aimMs + 500)` milliseconds, allowing the maximum 3000 ms
authored aim to complete, and rotate after firing or expiry. Dead, disabled,
moving, recovering or reloading members release their opportunities. A solo guard
does not require a lease.

Squad roles attempt one flank per target encounter, reserve separate destinations
and stop there before aiming. Searches retain the eight-candidate/four-search
frame bounds. A flank abandons its destination after five seconds or if its walk
becomes blocked, permitting normal combat instead of permanent suppression.
A patrol point similarly advances after five seconds without meaningful progress;
successful detours reset that clock and ordinary pauses remain intact.

At close range, armed guards can bash through a 300 ms interruptible windup for
12 damage, followed by 900 ms recovery. Bash checks the live target and first
obstruction, spends no ammunition and never shares a frame with gunfire.
Frontal bare-handed guarding reduces bash damage; it does not block bullets.

Empty tubes, injuries within the last 1500 ms or health at/below 35% cause a
bounded search for reachable cover that blocks both head and body from the
remembered threat. Guards hold protected positions during reload instead of
chasing back into exposure. Once transient defense ends, unseen low-health guards
still reach the normal investigation/search deadline and forget their target.
Movement still requires physical hull/support/door
clearance. Threat references, clocks, leases, flank/recovery state and ammunition
use the existing save store with bounds checks; save format 13 is unchanged.

`--guard-policy-test` covers these decisions, 60/120 Hz timing, interrupted saves,
three-second squad aiming and corrupt state rejection. `--guard-defense-test`
covers bounded low-health memory, protected reloads and contradictory saved flags.
The opt-in southern
experiment in [the Ash custom playground](ashfall-playground.md) uses the same
authoring and systems. See [the policy verification report](guard-policy-pass-20261011.md).
Full GoldSrc AI parity remains unproven; these bounded behaviors do not establish
equivalent content coverage, tactics or long-session reliability.

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
