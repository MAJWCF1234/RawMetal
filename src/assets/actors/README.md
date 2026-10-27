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

# Research staff provenance

Five civilian models share the Worker's pipeline and clips. `staff/` holds
their embedded derivatives (resources 693-713); the source FBX files are not
copied into the repository.

| Visual | Files | Model source |
| --- | --- | --- |
| Scientist | `scientist.*` | `O:/retro official/Characters/Characters_psx/Models/Male/Character_25_Doctor.fbx` |
| SeniorScientist | `senior-scientist.*` | `O:/retro official/Characters/Characters_psx/Models/Male/Character_26_Doctor.fbx` |
| FemaleScientist | `female-scientist.*` | `O:/retro official/Characters/Characters_psx/Models/Female/Character_23_Female_Doctor.fbx` |
| FemaleDoctor | `female-doctor.*` | `O:/retro official/Characters/Characters_psx/Models/Female/Character_24_Female_Doctor.fbx` |
| LabTech | `lab-tech.*` | `O:/retro official/Characters/Radiation Workers – Retro PSX Character Pack/light hazmat suit.fbx` |

Each `.obj` is the rest-pose assembly exported by RawMetalClutterImport with
`--assembly --skin-rest`; it retains every original triangle (926, 910, 1,208,
1,033 and 6,024) and the original material UVs. The lab coat PNGs are unchanged
copies of `Characters_psx/Textures/<model>.png`. `lab-tech-body.png` and
`lab-tech-mask.png` are unchanged copies of the light suit's `body radition guy
(1).png` and `mask radition (1).png`. The light suit's `guantelets radiation.png`
and `shoes radiation guy.png` are byte-identical to the heavy suit's, so the
lab tech wears the Worker's glove and shoe resources (280, 281). Runtime
material names map `mano` to gloves, `pies` to shoes, `mask` to the respirator
and anything else to body. No generated substitute textures were introduced.

RawMetalCreatureBake retargets the same animation library to each model with
the Worker's arguments: Idle_Loop, Walk_Loop, Punch_Cross, Hit_Chest, Death01
and Crouch_Idle_Loop, 16 vertex-pose samples per clip, with the `.anchors`
sidecar for RightHand and RightForeArm. Rerunning the pipeline on the heavy
suit reproduces `freight-worker.obj`, `worker-animation.bin` and its anchors
byte for byte, which is the check that these bakes were made the same way.

The Characters_psx models are authored about 4.5 units tall; the renderer
scales every actor to its track height, so they need no rescale. The arm
attachment joints in Scene3D's `kCivilianRigs` are the FBX RightArm/LeftArm,
ForeArm and Hand positions that RawMetalClutterImport's listing prints for the
same loaded pose.

`RawMetal.exe --civilian-inspection` renders the Worker and each staff model
through the game's actor path (every clip, death, and armed) into
`diagnostics/civilian-models/`; the creature-animation test checks every bake
for six clips, grounded feet, anchors and its rest height.
