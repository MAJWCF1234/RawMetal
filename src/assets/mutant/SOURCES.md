# Mutated human

Model and diffuse texture copied unchanged from `O:/retro official/mutatedhumanoid.zip`:
`mutatedhumanoid/MonsterPSX.fbx` and `MonsterPSX_Diffuse.png`.

`animation.bin` is generated with `RawMetalAuthoredCreatureBake MonsterPSX.fbx animation.bin`.
Uses the model's authored Idle_Watchful, Walk_Nervous, Attack_Lunge and WallSlam_Recover clips.
The fifth clip lays the recovery pose onto its back and grounds its bounds for death;
no death animation was supplied. All five clips contain 24 skinned samples.
The runtime interpolates baked vertices without evaluating FBX skeletons per frame.

Resources: 272 model, 273 diffuse, 274 baked animation. Native encounters: Warehouse C
(chunk 17) and Depot Trackside (chunk 27), exactly two creatures, 180 health each.
