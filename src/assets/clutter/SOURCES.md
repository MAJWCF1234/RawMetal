# Physical clutter

Purchased originals in O:/retro official remain unchanged.

Five types now come from PSX Bunkers v1.8.8.zip, matching the machinery and log computers: trash_1, mre_1, power_supply_1, pcb_2, floppy_disc_2. Each uses its named FBX from PSX Bunkers/Models/FBX and matching PNG from PSX Bunkers/Textures. Runtime OBJ files preserve original triangles and UVs; textures are unchanged. FBX reference copies remain in source/.

The glass bottle remains the purchased Street Furniture asset: Bottles/Bottles.fbx, node Cylinder.001, and Bottles.png. The Bunkers pack has no bottle replacement. Older junk imports remain in source but are no longer embedded, except this bottle.

Build the importer with cmake --build .build --config Release --target RawMetalClutterImport. Pass a source FBX, node name and output OBJ. It retains original geometry; runtime applies uniform size scaling.

Physics uses lightweight box bounds with pitch, roll, yaw and angular velocity. Rendered vertices and collision bounds use the same rotation. Gravity/contact torque tips unstable objects, impacts impart tumble, friction slows motion, and resting bodies sleep without returning upright. Bottles can remain on their sides. Glass, metal and soft clutter retain positional landing sounds. Walking into clutter nudges it; E lifts/drops it and left click punts it for one 5-damage hit. Rotation and rest state persist in chunk snapshots.

This is small-prop collision against the static world, not a general rigid-body stacking solver. Tests cover tipped bottle settling at 60/120 Hz, no floor penetration, upper-catwalk landing, impact audio and exact punt damage.
