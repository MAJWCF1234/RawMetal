# Shared guard AI and the custom Ash playground

This slice adds guard patrol and investigation to the existing shared NPC system.
The actual Custom Maps playground, `RawMetal-Horror-OpenWorld.json` and its TXT
export, now contains one guard, four patrol points, hiding cover and two throwable
objects. The user designated this file pair as replaceable experimental content.
No campaign maps or new assets were added.

Guards walk and pause at authored points, build recognition while the player
remains visible, investigate actual sound/last-seen positions, search for a bounded
time and resume their patrol. They use the same hull/support checks, ordinary-door
permissions, four-search frame budget, injury/death and saved actor identity as
civilians and escorts. Perception runs after this frame's movement, throws and
shots. Guard state and partial recognition persist in save format 13.

This is awareness/navigation, not armed combat or faction tactics. The prototype
uses the existing 1.6 m Worker visual rig while the external asset library is
offline. It does not restore the quarantined custom human subsystem.

## Defects repaired during inspection

- Authored outdoor grid maps without terrain omitted floor geometry and collapsed
  cell walls to zero height. The renderer now emits their grid floor, and outdoor
  walls use the authored height when no built-in wall height exists. Sky remains
  open; terrain worlds keep their existing terrain mesh.
- Ordinary doors in loaded neighboring chunks now animate for NPCs. Transfer
  leaves retain the streamer's existing synchronization and do not advance twice.
- NPC steering checks vertical overlap before stopping against the player, so
  actors on a separate floor do not stop under the player's XY position.
- Saved guard height, footstep accumulator and search bearing are bounded before
  use; malformed INT_MIN/INT_MAX state cannot overflow the new calculations.
- The editor preserves bounded authored runtime records on load/save/export.
  Guard records no longer disappear after editing the playground's geometry.

## Validation

Release build passed. `RawMetal.exe` is **23,527,936 bytes**, below the strict
28,000,000-byte cap. All 185 embedded resources match their source losslessly.

`--guard-ai-test`: **50 checks passed**. Coverage includes 60/120 Hz patrol and
recognition, interrupted saves, wall/cone occlusion, real Game-update footsteps,
shots and crouched throws, hidden-player memory, locked/neighboring doors, shared
alert flags, death, search limits, navigation budget and malformed authoring/saves.

Existing physics/AI, world isolation, mechanisms, campaign maps, saves, controls,
offline VR interaction, audio and streaming suites passed. Vulkan smoke passed.
Offline VR checks do not establish headset testing.

The actual exported playground was loaded by `--guard-inspection`; it completed
all four patrol points over 30 simulated seconds without seeing through the cover.
Its patrol, alert and impact-investigation states passed. Captures use Vulkan on
an NVIDIA GeForce RTX 5060 Ti with linear HDR and 4x MSAA, and were visually read.
The inspection camera now validates full hull clearance before capturing.

Seven focused editor tests and the existing map-payload regression passed against
clean Git HEAD plus only this feature patch. That clean exporter preserves all
11 playground runtime records and reproduces the TXT payload with zero warnings.
Unrelated local editor changes were not needed and are not part of this slice.

Pass evidence was local diagnostics (build, guard tests, suite outputs, and
patrol/alert/investigate captures). Initial captures exposed the absent
ground/walls; camera correction after walls became solid makes those images
diagnostic rather than a controlled pixel score or rendering-quality percentage.

## Playtesting

Start the rebuilt project EXE, choose **Custom Maps**, then **RawMetal // Ashfall
Guard Lab**. Restart the map for a fresh encounter. The built-in Ashfall campaign
entry is a separate world. See [playground instructions](ashfall-playground.md)
and [shared AI contracts](ai-perception.md) for controls and authoring details.
