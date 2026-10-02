# v0.6.5 — Performance and Frame Pacing

Stationary pipework and its brackets now reuse a world-space Vulkan vertex
buffer. Camera turns no longer rebuild cylinders and repeat their lighting
work. Full lighting is baked once, correcting pipes that previously became
dark when the per-frame ray budget ran out.

The collision engine now selects nearby fixture, prop and terminal candidates
from a spatial grid before running the original exact intersection tests.
This reduces the cost of movement queries and lighting traces, including the
volumetric traces required by sloped water beds. Moving lift controls remain
eligible everywhere, and chunk reload/unload invalidates the grid.

The arm rig performs one final skinning pass after IK, fingers and recoil.
The removed intermediate skin pass produces identical final vertices and
hand transforms in the regression checks.

Vulkan surface capabilities are queried when the window changes, rather than
twice every frame. Resize, minimize/restore and suboptimal swapchain images
retain the hardware renderer. Renderer ownership also avoids a large native
startup stack shared by inspection branches.

Resolution, 4x MSAA, HDR, bloom, parallax, material textures and normal maps
remain at their existing quality. No maps or save-format changes are included.
Windows 10/11 x64 requirements from v0.6.4 remain in place; install the x64
Visual C++ runtime and a Vulkan-capable graphics driver.

Across three local runs at native 1080p, Cable Vaults p95 frame time fell from
58.4 to 7.3 ms; Waste Handling fell from 36.9 to 9.9 ms. Ashfall average FPS
was unchanged, with a 5.8% p95 regression. Cold lighting bakes still cost time.

See [performance and image validation](https://github.com/MAJWCF1234/RawMetal/blob/main/docs/performance-quality.md) for measured
results, comparisons and limits. These measurements are local tests on an
RTX 5060 Ti, not a GTX 1660 benchmark or a guarantee for other systems.
