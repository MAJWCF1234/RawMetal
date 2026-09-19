# Supplied arm animation data

Inspected the copied arms_rig.glb and arms_rig.fbx from psx-first-person-arms-free-game-assets.zip. Both expose 18 animation clips. The FBX names have the ArmsRig prefix.

- finger_gun_broken, finger_gun_fire, finger_gun_fix, finger_gun_idle
- grab.L, grab.R
- guard_draw, guard_idle
- jab.L, jab.R
- knife_draw, knife_hit_01, knife_hit_02, knife_idle
- push.L, push.R
- relax, rest

The rig contains shoulder, upper_arm, forearm, hand, palm, thumb and finger bones, plus handIK and elbowIK control nodes. The archive also includes Blender source, UVs and skin/glove textures. It contains no README or shotgun-specific animation clip.

RawMetal reads the final finger rotations from grab.L and grab.R (30 transforms). It evaluates upper-arm/forearm IK at runtime, using gun-mounted trigger and support grip sockets. A damped spring changes the elbow pole direction for follow-through, with fixed segment lengths and hands constrained to the gun. It is an IK chain with secondary motion, not a freely dangling ragdoll.

The grab endpoints are closed fists, so firearm poses blend individual joints from the bind pose toward those endpoints. The support hand uses a cupped palm with more curl at the outer finger joints; the trigger index stays more open. Wrist orientation is derived from the source palm and knuckle directions: the support palm faces up around the fore-end and the trigger palm faces inward toward the stock. Each wrist has its own socket offset.

The complete assembly sits 0.13 metres closer to the camera and 0.035 metres lower than the previous placement. The source mesh has two open shoulder boundaries. The renderer finds their boundary loops in the skinned mesh and extends them into closed dark sleeves behind the camera, preventing exposed hollow arm ends during recoil and sway.

Gun socket coordinates are authored in the imported Remington FBX space in Scene3D.cpp. The same weapon transform drives the geometry, sockets and muzzle flash. The gun has separate Barrel, Base, Bolt, Crosshair, PicatinnyRail and ShotgunBullet mesh nodes and no animation clips. RawMetal adds procedural recoil, sway, bolt travel and shell ejection.

The motion check in --smoke-test measures wrist/socket separation across recoil, mouse movement, jumping and settling. attachment-test.txt records the maximum error.

The polish pass derives lag from angular velocity instead of per-frame mouse deltas. A damped rotational spring, shared assembly roll, stronger recoil and a separate vertical spring provide weight during fast turns, takeoff and landing. The grip diagnostic now exercises mouse deltas up to 80 pixels per frame without separating wrists from gun sockets.

At zero ammunition the shotgun and attached hands lower together; the arms then evaluate the supplied `guard_idle`, `jab.L` and `jab.R` skeletal clips directly. Alternating punches apply their hit at 220 ms, with range, facing, height and world occlusion checks. Right mouse raises the guard, cancels a pending punch and reduces frontal melee damage. The unarmed camera placement keeps shoulder openings behind the camera; boundary caps remain for external inspection. Restocking shells raises the shotgun again.

Developer inspection renders six angles in idle and recoil (rig-0-0.ppm through rig-1-5.ppm). Side and overhead inspection identified the source-to-camera handedness mismatch: source hand.R is at negative X. The arm conversion now mirrors X, including the inverse socket conversion, so the right arm operates the trigger and the left supports the fore-end. Geometry, hand orientation and grip targets are checked using these diagnostic views rather than the first-person camera alone.
