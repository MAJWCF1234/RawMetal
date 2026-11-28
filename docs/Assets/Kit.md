# Kit

Kit is optional character 3. Claire remains default character 0. The installed runtime assets are under src/assets/library/player and are embedded in the game and Builder Play engine.

- Body: 6,838 triangles; 40 animation clips and existing stance/weapon layers.
- First person: Kit's original skin, nails and personal-set sleeves, fitted to the existing four weapon rigs. Each has 4,428 vertices and 8,000 triangles.
- Materials: KitBody, KitSuit, KitHair, KitLashes, KitHands and KitNails. The two close-up hand textures are 512x512.
- Source project and original inputs: source-assets/characters/Kit (local licensed assets, intentionally excluded from public Git).
- Editable model: source-assets/characters/Kit/assets/Kit.blend.
- Portable model: source-assets/characters/Kit/assets/Kit - No Jewelry.glb.
- Build report: source-assets/characters/Kit/kit-build-result.json.

All Kit work files have moved out of Downloads. Current editable models pack their used textures and use relative texture paths.

The source converter now fits actual joint heads, preserves finger ownership, and uses the existing forearm twist skinning field. It excludes the original jewelry material before reducing the viewmodel surface. The full-body jacket uses barycentric UV projection from the original suit surface.

Validation includes the kit and kit-viewmodels feature-lab tubes, deterministic repeats, native settings/menu/weapon/lighting checks and the complete packaged-resource verifier. Physical headset and live remote multiplayer testing remain separate.

## Shared first-person animations

The four installed `kit-*.fbx` files retain Kit's fitted geometry, bind transforms, UVs, materials and skin weights, but contain no duplicate animation objects. `Mesh::shareActionsFrom` retains the existing stock rig's immutable animation scene and maps its node transforms by name. The fists/pistol rig uses resource 111; shotgun 2313; MAC-11 3359; rifle 3393. Kit's own hands and sleeves remain in use.

Compressed first-person resources dropped from 4,676,052 to 1,262,760 bytes (73.0% smaller). Kit's total added packed resources dropped from 6,574,220 to 3,160,928 bytes (51.9% smaller). These are incremental asset sizes; Leon's 960,288 bytes also exclude the stock first-person rigs shared by all existing characters.

Animated originals and rollback sources are preserved locally under `source-assets/characters/Kit/before-shared-actions`. Editable Blender files retain their source animations. Convert future runtime exports with:

```powershell
python tools/assets/strip_fbx_animation.py animated-kit-arms.fbx src/assets/library/player/kit-arms.fbx
python -m unittest discover -s tools/assets -p test_strip_fbx_animation.py
cmake --build .build/vs2022 --config Release --target RawMetalKitSharedActionsTest
.build/vs2022/Release/RawMetalKitSharedActionsTest.exe src source-assets/characters/Kit/before-shared-actions
```

The native parity test samples all 50 actions at 33 phases (1,650 poses), checks exact topology/UV/material equality, procedural grip poses, missing-action behavior, invalid attachment rejection and animation-scene lifetime. Maximum observed vertex difference against the accepted re-exported animations was 0.160818 mm. Hardware lab captures cover fists, pistol, shotgun, MAC-11 and rifle, with a deterministic repeat and before/after image comparison. This optimization does not change Claire's default selection.
