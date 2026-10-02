# Freight worker provenance

Model source: `O:/retro official/Characters/Radiation Workers – Retro PSX Character Pack/heavy radiation suit.fbx`.
Animation source: `O:/retro official/Animations/Universal Animation Library[Standard]/Unity/UAL1_Standard.fbx`.

`source/freight-worker.fbx` retains the original model. `freight-worker.obj` is
the rest-pose assembly exported by RawMetalClutterImport with `--assembly
--skin-rest`. It retains all 4,572 triangles and original material UVs. Runtime
material names map `mano` to gloves, `pies` to shoes and the remaining material
to body.

The three PNG files are unchanged copies of the supplied body, gauntlet and shoe
textures. No generated substitute textures were introduced.

RawMetalCreatureBake retargets the animation library to the worker skeleton:
Idle_Loop, Walk_Loop, Punch_Cross, Hit_Chest, Death01 and Crouch_Idle_Loop. The
animation binary has six clips and 16 vertex-pose samples per clip. Its anchors
sidecar stores the matching RightHand and RightForeArm positions for pipe
attachment. The sixth clip is enabled by the bake tool's final argument.

Stage the source FBX files under ASCII-only paths before calling ufbx-based
tools; the narrow Windows loader cannot open the pack's en-dash directory name.
The source FBX is retained for future rebakes but is not embedded in the game.
