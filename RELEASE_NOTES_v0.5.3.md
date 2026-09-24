# RawMetal v0.5.3 - Atmospheric renderer update

This release improves the Vulkan scene's depth and material response while keeping the original low-resolution texture art. Indoor geometry now has restrained contact shading, more consistent local lighting, and subtler fog shafts. Gloss response, filmic color handling, and a small bright-pixel bloom add material depth without blurring the HUD.

Static geometry caching, improved culling validation, and a first-frame lighting fix keep the scene stable as chunks load. Terrain performance was protected by limiting geometry occlusion probes to indoor worlds.

Validation: Release build, smoke test, Vulkan renderer test, shading inspection, six-view culling/reference comparison, and native Win32 Vulkan swapchain timing passed. The executable is 18,607,104 bytes. The 300-frame native swapchain run averaged 102.45 FPS (9.76 ms per frame) on an RTX 5060 Ti; results vary by hardware and scene.

Download either RawMetal.exe or RawMetal.zip. The ZIP contains the same single executable, with all game assets embedded.
