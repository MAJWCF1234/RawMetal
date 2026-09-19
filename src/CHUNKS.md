# Map layers and door-controlled chunks

## Surface Lift extension

The fourth connected chunk follows Turbine Gantry at offset (54,72). Gantry's
southern seam is open after its existing control/combat interlock. The new chunk
contains seven full map layers at -9, -6, -3, 0, 3, 6 and 9 metres. The two bottom
maps form the reactor, connected by an east staircase and overlooking a common
containment vessel. The central lift room rises from 0 to 9, jams and falls to -9.
Its opposite emergency door opens on impact. The lower reactor exit ends the
currently authored route. See LIFT_DESIGN.md for the implementation and tests.

Door z offsets and negative-height spans support the stacked maps. Cab phase,
height and timing persist through geometry unload/reload. The reactor uses the
same span-aware enemy navigation as the gantry. Audio follows cab state, with
music/effects controls remaining independent. The historical notes below describe
the original three chunks; the new fourth chunk extends that route.

All authored map arrays and named layer instances live in `world/World.cpp`. `world/World.h` defines `MapRows`, `MapLayer`, `Staircase` and the public `layers()` / `spansAt()` queries. There is no map-specific Gantry.cpp.

- `FoundryGround`: map 1, ground layer.
- `PressureWorksGround`: map 2, ground layer.
- `TurbineGantryGround`: map 3, ground layer.
- `TurbineGantryUpperCatwalk`: map 3, separate upper layer at 3 m, with a 0.3 m deck thickness.

Each array has 24 rows of 24 characters. The upper array uses `=` for deck and `_` for no structure. Ground characters retain walls, crates, machinery and exits. The supplied gantry layout includes small authored corrections: orthogonal links across two diagonal gaps, a stair landing, a relocated barrel at the stair approach, and a sealed extraction vestibule.

The generic `World::buildLayers` reads layer elevation and thickness to produce deck runs, edge rails and supports. Stair instructions generate treads; the gantry flight rises 3 m over 15 steps. Rail openings follow stair connections. Rendering, player collision and enemy navigation consume these structures. `spansAt(x,y)` returns both the space below a bridge and the space above it. Support queries include the actor's current foot height, so walking underneath does not snap the actor onto the upper deck. The first two maps retain their existing stepped ground height profiles.

Turbine Gantry has six enemies. Its route leads from the northern entry through the machinery hall, up the southern stairs, across the connected upper catwalks to Gantry Control, then back down toward extraction. Both clearing the enemies and activating the upper control are required to open the final interlock. E dismisses a displayed log.

## Streaming and persistence

The three chunks occupy offsets (0,0), (18,24), and (36,48). Adjacent three-metre openings align across the boundaries. Opening a transfer bulkhead reconstructs the neighbor's geometry before the door reveals it. Both chunks render into the same depth buffer and player collision samples both sides of the seam. Player position is rebased at the boundary; health, inventory and momentum continue.

A closing door retains both chunks until its leaves are completely shut. Then inactive static geometry is released. Reopening reconstructs it from the layer arrays and restores door/control state. Enemies, pickups, loose clutter and kill progress remain in snapshots; clearing a chunk and returning does not respawn it. Shared models, textures and audio remain loaded. Enemy simulation advances only in the occupied chunk. Reconstruction is synchronous and small; this is not background disk streaming or cross-chunk combat.

Each map has six movable objects: purchased PSX Bunkers debris, a ration pack, power supply, circuit board and floppy disk, plus the Street Furniture glass bottle. E picks up or drops one; left click punts a held object. A projectile hit deals exactly 5 damage once, then disarms. Objects rotate on all three axes, fall, bounce and settle on ground or upper decks; bottles can rest on their sides. Rendered geometry and collision bounds share the same rotation. Ground friction and contact torque slow the motion before resting bodies sleep. Walking into nearby clutter wakes and nudges it. Material-specific spatial impact sounds scale with impact speed; settled objects stay quiet. These are lightweight box-based prop physics against the static world, not a general rigid-body stacking solver.

## Validation

`--smoke-test` exercises continuous travel through maps 1 and 2 into map 3, returning across a seam, persistent state after unload/reload, and chunk retention while the blast door closes. Gantry checks drive the real controller from entry to stairs and along the full catwalk route, test both walkable spans at the same XY, activate and dismiss the upper terminal, fall to the lower floor, and unlock extraction. Separate checks cover clutter damage/landing, enemy gravity, crate climbing and circling attacks.

Diagnostic views cover ground-level underpasses, stairs, upper crossings, the terminal, extraction and held clutter. Run tests from `N:/rawmetal/diagnostics`; the only published executable is `N:/rawmetal/RawMetal.exe`.


