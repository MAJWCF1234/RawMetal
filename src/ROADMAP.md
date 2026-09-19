# Next RawMetal passes

Completed the client movement/map pass: crouch-jumping and airborne ducking; ground acceleration, friction and air control; height-aware collision; foundry stairs and raised storage; four usable bulkheads; three shift logs; hearing, investigation and door-aware enemy pursuit; correctly mounted chemical diamonds; 640×360 rendering with local, interpolated lighting. Controller tests reach extraction through three bulkheads. Imported pickup models, audio, settings and the attached first-person rig remain covered by the smoke test.

1. **Combat feedback.** Add material-specific bullet impacts and enemy hit effects, improve hit reaction timing, and check aiming against visible geometry around cover. Verify that feedback corresponds to actual damage and cannot appear through walls.
2. **Shotgun reload.** Separate the loaded tube from reserve shells, add an interruptible shell-by-shell reload with matching handling sounds, and integrate hand animation without losing grip alignment. Move restart out of the R binding when reload takes it over. Verify idle, empty, interrupted and completed reloads from multiple angles.
3. **Enemy behavior and animation.** Build on hearing, investigation and pursuit with species-specific ranged attacks and encounter coordination. Audit additional supplied creatures for real rigs and usable clips before expanding the roster; avoid importing more static models with the same behavior.
4. **Level encounters and progression.** Build deliberate combat pockets, shortcuts and discoverable supply caches, then add checkpoints. Test complete playthroughs for navigation, ammunition availability, pacing and extraction.

Performance follow-up: the 640×360 CPU renderer measured roughly 19–40 ms per frame across three views on this machine after lighting optimization (render only). Profile and move triangle rasterization to a GPU backend before raising resolution or substantially increasing enemy counts; do not treat this benchmark as a locked gameplay frame rate.

Keep checking performance, audio overlap, pause/settings behavior and the first-person rig as each pass lands. Heightfield collision now supports stairs, raised floors and jumpable cover. Turbine Gantry now uses separate ground and upper map arrays with multiple walkable spans, stairs and bridges that can be walked beneath. Moving platforms remain future work.

