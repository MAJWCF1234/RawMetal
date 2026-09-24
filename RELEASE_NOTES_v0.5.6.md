# RawMetal v0.5.6 - Camera depth and focus

The native Vulkan presentation pass now uses the scene depth buffer to add distance-aware lens softness based on the reference camera's 2.10 cm focal length and f/4.8 aperture. The circle of confusion is calibrated to RawMetal's world scale, so distant rooms visibly soften by up to seven screen pixels. Nearby geometry, the weapon, and the HUD stay sharp. Depth-aware sampling prevents foreground silhouettes from bleeding across distant walls.

The existing ACES-style tone mapping remains in the scene shader and is applied only once. The full Release build, Vulkan smoke and renderer tests, performance test, and Win32 swapchain startup check passed. RawMetal.exe remains below the 19 MB release limit.

Run RawMetal.exe directly or extract RawMetal.zip; the ZIP contains the same executable with embedded assets.
