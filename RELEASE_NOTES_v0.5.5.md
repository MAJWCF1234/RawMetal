# RawMetal v0.5.5 - Explosive barrel fire

Shooting a chemical barrel now produces a visible, world-space fire burst with rising flames and smoke. The effect appears at the destroyed barrel and is occluded by nearby geometry. Its flame and smoke masks are generated in the executable, so no external assets are required.

The barrel still destroys its tile, damages nearby enemies, shakes the camera, and plays its impact sound. A combat regression check confirms that shooting a barrel removes it and starts the explosion effect.

Release build, Vulkan smoke test, Vulkan renderer test, and three-phase visual inspection passed. Download RawMetal.exe directly or extract RawMetal.zip; the ZIP contains the same executable and embedded assets.
