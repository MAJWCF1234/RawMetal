# Renderer architecture

Normal play uses Vulkan 1.0 on a hardware graphics queue. GLSL is compiled to
SPIR-V at build time and embedded in the executable. No runtime shader compiler,
loose shaders, vendor-specific extensions, or SDK installation is needed to play.
The executable delay-loads the Windows Vulkan loader and retains a software path
when initialization fails. `--software` explicitly selects that reference path.

The existing scene traversal supplies one triangle stream to either backend.
Vulkan performs perspective interpolation, rasterization, clipping, depth tests,
texture mip sampling, normal mapping, alpha cutouts, emission and additive effects.
Textures and mip chains are uploaded once at initialization. Opaque geometry is
batched by material, with additive effects after opaque geometry. World and weapon
depth ranges remain separate. CPU HUD/inventory rendering is preserved.

This first backend renders to an offscreen Vulkan image, waits on a fence, then
reads the image back for the existing Win32 presentation/HUD path. It is not yet a
native Vulkan swapchain presentation path, and it does not claim asynchronous
CPU/GPU frame overlap. Removing readback is a future optimization. The graphics
API is portable; the current shipping window, input and audio implementation is
still Windows-specific.

CPU optimizations retain full scene visibility:

- Conservative opaque-deck occlusion only rejects a complete projected bound
  covered by a solid rectangle. Shaft and stairwell openings remain visible.
- Receiver lighting caches do not invalidate on lift movement. Moving cab
  surfaces are handled separately from stationary geometry.
- Lamps are spatially indexed; shadow tracing is budgeted over successive frames.
- Collision/span queries use per-cell structure indices.
- Animated meshes cache triangulation, UVs, material assignments and boundary
  rings. Intermediate IK steps do not redundantly skin the mesh.
- One persistent worker evaluates the independent arm rig while the main thread
  prepares world geometry. GPU calls and the lighting caches stay on the main
  thread. Every frame joins the worker before consuming the posed mesh.

## Verification

Run tests from `diagnostics/`:

- `../RawMetal.exe --smoke-test`: software reference and gameplay regressions.
- `../RawMetal.exe --smoke-test --vulkan`: the same rendering scenes on hardware.
- `../RawMetal.exe --vulkan-test`: depth, cutout, emission, normal-map response,
  near clipping and weapon-depth isolation; exact parallel/serial pose images.
- `../RawMetal.exe --performance-test`: full 640x360 update/audio/render timings,
  followed by exact culled/unculled view comparisons. A measured frame over
  50 ms fails. Startup is separate; OS window presentation is outside this test.

For API validation, run with `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation`
and `VK_LAYER_VALIDATE_SYNC=1` in a development environment with that layer.
`RawMetal-renderer.txt` identifies the selected adapter or fallback reason.

Performance measurements are hardware- and load-dependent. Neither Vulkan nor
a benchmark pass can guarantee a minimum frame rate on every computer.
