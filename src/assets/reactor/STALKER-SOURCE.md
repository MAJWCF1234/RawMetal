# Reactor Stalker

Replaces the v0.3.4 antlered Warden and its ranged attack. Save enemy kind 3 is retained so existing saves load the new melee creature.

- Model: `E:/retro official/Characters/Characters_psx/Models/Killers/Character_Monster_03.fbx`, mesh `Character_Monster_03`, 1,386 triangles, with its matching texture under `Characters_psx/Textures`.
- Motion: Quaternius Universal Animation Library Standard, `Unity/UAL1_Standard.fbx`: Idle_Loop, Walk_Loop, Punch_Cross, Hit_Chest and Death01. The supplied animation library's License.txt declares CC0 1.0.
- `src/tools/bake_creature.cpp` maps 22 source bones onto the supplied Mixamo skeleton, aligns rest-pose shoulder axes, scales pelvis height motion, evaluates skeletal skinning, and exports 16 samples per clip with shared-vertex indexing and millimetre quantization. Grounded clips are floor-aligned after retargeting to avoid proportion-related sinking. Runtime linear interpolation preserves articulated motion without per-enemy FBX evaluation.
- `RawMetalClutterImport --skin-rest` exports the evaluated skinned rest mesh in the same triangle order. Resource 244 validates topology and every shared-vertex index before playback.

Behavior: approaches and mildly flanks at close range; commits to a 0.55-second melee windup, then recovers. Damage requires distance at most 1.2 units, a forward-facing cone, overlapping body heights and clear geometry. No projectile, beam, ranged damage, targeting stripe or glowing charge remains.

Run `--stalker-test` to verify clip deformation and melee behavior and render five poses from each clip inside the reactor.
