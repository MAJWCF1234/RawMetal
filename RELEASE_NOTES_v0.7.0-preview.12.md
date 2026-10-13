## Summary

- Cleanup locomotion removes donor planar root travel and closes loop endpoints.
- Baked pelvis/spine and calibrated arm joints keep the upper and lower layers joined while walking and aiming.
- Arm ownership includes the hands even below the waist; repaired fingers receive rigid aim and attachment transforms.
- Positive aim pitch raises the torso around its posed spine. Natural idle uses the full loop.
- Final arm attachment starts from the composed bake and follows the gameplay grip/muzzle, including pistol, reload and Engineer terminal work.
- NPC kneeling clearance is separate from the player's crouch hull.
- Downloaded the Pistol/Handgun pack and Pro Rifle special motions. Raw donors stay local; runtime bakes are embedded.
- Rifle firing uses the baked upper fire clip. Medic treatment keeps its UAL2 animation; the carrying palm holds the kit while the opposite hand treats.

## Validation

- `tools/BakeCleanupMixamo.py` re-bake
- `RawMetal.exe --cleanup-split-test` (also in `--smoke-test`)
- `RawMetal.exe --cleanup-layer-inspection`: all six roles, signed aim, gait, crouch, fire, reload and support views; final finger, reach and barrel measurements
- Release build under OoT 32 MiB cart ceiling

Verified: 103 split checks, 180 native pose/view checks, beam/hitscan/gameplay tests
and live Engineer deployment/maintenance inspection. Release executable:
25,604,608 bytes; all 222 embedded resources verified lossless.

Full smoke still fails the existing authored-save byte-exact check. The existing
Medic detour/healing and four Vulkan radiance-filtering checks also remain
unresolved. See the [repair report](docs/cleanup-animation-repair-20261013.md).
