# Ashfall custom-map playground

This is the project's replaceable, evolving engine playground. Each feature pass
can change its encounters and fixtures. It is not canonical campaign content.
The built-in Ashfall world and the main campaign have not been changed by this
guard pass.

The actual custom-map files are `custom maps/RawMetal-Horror-OpenWorld.json`
(editor source) and `custom maps/RawMetal-Horror-OpenWorld.txt` (runtime export).
Before this pass both files were copied to
`diagnostics/guard-pass-20261010/playground-backup/`.

## Open the current encounter

Launch `RawMetal.exe` from the project directory, choose **Custom Maps**, then
**RawMetal // Ashfall Guard Lab**. The separate **ASHFALL EXCLUSION ZONE** entry
is the older built-in world. Restart this custom map for a clean encounter;
loading an older save restores that saved encounter's state.

The ruined 24 m square remains the same layout, with all 24 rows verified to
contain 24 cells. The two former Huntsman test spawns were removed so they
cannot interrupt guard observation.
All new objects use existing project geometry or assets; the unavailable
`O:\retro official` drive is not needed.

Visual inspection exposed a shared engine bug in authored outdoor grid maps.
This file uses the original wasteland skybox and outdoor flag with a flat grid
and no terrain mesh. The renderer skipped its ordinary floor, while grid-wall
height defaulted to zero, leaving the guard and cover surrounded by sky. The
engine correction supplies grid floors when no terrain mesh exists and uses
the authored 2.8 m height for these outdoor walls. Outdoor maps keep their open
sky; indoor ceilings and the built-in Ashfall terrain retain their own paths.
The playground theme, geometry rows, floor elevation and patrol remain unchanged.

## Guard pass, 10 October 2026

One Worker visual rig, 1.6 m tall and with 70 health, uses engine-owned Guard AI.
It patrols this clear rectangle on the same flat floor:

| Point | X | Y | Feet Z | Pause |
| --- | ---: | ---: | ---: | ---: |
| 1 | 19.5 | 7.5 | 0 | 900 ms |
| 2 | 21.5 | 7.5 | 0 | 600 ms |
| 3 | 21.5 | 12.5 | 0 | 1,200 ms |
| 4 | 19.5 | 12.5 | 0 | 800 ms |

The player starts at `(16.7, 10.1, 0)` on the other side of a 1.85 m tall cover
panel. The cover ends at Y=8.5 and Y=11.5, leaving routes around both ends.
A bottle and power supply lie within reach of the starting position. Blueprint
labels mark the guard and its patrol points without creating duplicate actors.

Try these observations:

1. Wait behind cover. The guard should walk, pause at the points, and continue
   without spotting the player through the panel.
2. Peek around either end when the guard is facing you. A brief glimpse should
   build recognition; continued exposure should produce an alert. Full
   recognition takes 600 ms with the normal standing visibility conditions.
3. Break sight and move silently to another position. The guard should
   investigate its last observed position, search briefly, and return to patrol.
4. Look down at the bottle or power supply, press **E** to lift it, and use
   primary fire to throw it around the end of the cover into the aisle. The impact should
   attract investigation to the object's actual position rather than reveal
   the hidden player. Crouch with **C** or **Ctrl** to reduce footstep noise.
5. Save during investigation, then load. Position, patrol progress and awareness
   should restore. Killing the guard should use the shared actor injury/death
   path and remain dead when restored.

The authored sight range is 11 m, the sight-cone dot threshold is 0.65, and the
search duration is 4 seconds. The named states are `ash_guard_alert`,
`ash_guard_damage`, and `ash_guard_dead`. The October 11 pass equips this guard
through the shared `ACTOR_WEAPON` record. The guard counts as one hostile contact
and uses 18 damage, a six-round tube, 650 ms aiming, 1000 ms recovery and a 9 m
attack range. It reloads for 1800 ms when empty. Recognition and raising the gun
precede the aim delay, so the first shot has a readable warning.

Stay hidden to observe patrols; sustained exposure now starts actual combat.
Break sight or place cover/a character between the muzzle and your body to cancel
the shot. Save during aiming, recovery or reload to test restoration. The existing
Worker suit is prototype art; it now holds the existing shotgun with two attached
hands. This experiment does not implement faction tactics, stealth takedowns or
human-shield controls.

The cover-repositioning pass adds a 1.3 m tall metal panel at
`X20.6–20.8, Y7.1–7.9`, directly across the first patrol leg. The normal navigator
must also detour around it while patrolling. Stand near `(21.8, 7.5, 0)` to expose
your head while the panel blocks the guard's barrel/body shot. After recognizing
you, the guard should sidestep to a clear firing position, keep looking at you,
stop, and complete its aiming delay before damaging you. Moving or loading a save
must never allow a shot through the panel. The original taller hiding panel,
throwables and four patrol points remain part of the same temporary encounter.

Run `--guard-tactics-inspection` from the project directory for three fixed-camera
Vulkan captures of the actual exported encounter: covered, repositioning and
clear firing. These are observed after normal gameplay updates, with the real
health change retained and only HUD/hands and damage flash hidden for inspection.

## Keep authoring and runtime in sync

Geometry, player spawn, cover and throwable assets use supported editor objects.
The guard's `TIMED_SEQUENCE`, `ACTOR`, keys, health, AI, guard configuration and
patrol points and weapon configuration are stored as top-level JSON `runtimeRecords`, then appended by
the generic exporter into the runtime campaign block. These are shared engine
records, not map-specific code. The visual labels are only labels.

When replacing this experiment, edit the blueprint objects and `runtimeRecords`
together, export a fresh TXT, and test the exported custom map. Keep the same
file pair for the evolving playground. A successful JSON parse or export does
not by itself prove the encounter's runtime behavior.
