# Kit

Kit is character 3. The installed runtime assets are under src/assets/library/player and are embedded in the game and Builder Play engine.

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
