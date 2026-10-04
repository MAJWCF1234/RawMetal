# v0.7.0-preview.6 — Rendering and transfer performance

Diffuse textures now filter in linear light using sRGB images and complete,
area-weighted mip chains. Fine patterns retain their average brightness instead
of darkening as they recede. Original embedded assets remain lossless.

Normal-map mip levels and hardware interpolation preserve the actual mean normal
and its variance. GGX uses that variance to soften unresolved highlights. Tiny
valid UV islands keep shading, and skewed charts use an orthonormal lighting frame.

Ceiling light scattering follows room density and each lamp's intensity, clips
to the finite volume, and uses a smooth brightness shoulder. This reduces the
beige haze that masked depot and platform details while preserving light pools.

Vulkan batches immutable texture transfers through a reusable staging buffer.
CPU scene work reuses light samples and fixture material bindings. Native HDR,
4x MSAA, anisotropic filtering, original geometry and shadow rays are retained.
Dynamic wrist/terminal color updates refresh their full mip chains.

Fixed ceiling fixtures and supports now reuse geometry across frames. Exact lamp
state changes refresh cached lighting and emission; moving lights stay dynamic.
In the focused Warehouse cache/reference check, CPU scene preparation used 45%
less time with identical compared pixels. Exact mip encoding was 1.90x faster.
These are isolated measurements; overall FPS remains variable, and cold cache
construction still causes first-load hitches.

The campaign still contains 32 chunks. The executable limit is strictly below
28,000,000 bytes. PC and VR share these renderer changes; live headset and GTX
1660 performance require hardware testing.

Matched images, pixel comparisons, raw timings and release validation are in
[the evidence report](docs/render-quality-pass6.md).

The executable is **23,360,000 bytes** (23.36 MB). All 185 embedded resources
passed lossless verification. Vulkan material/cache regressions, campaign,
streaming, save, service-map, extension, offline VR and hardware smoke tests
passed. The release includes 66 matched before/after camera comparisons.
