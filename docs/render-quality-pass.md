# Rendering quality and performance pass

The art target is intentionally coarse source artwork with stable modern
presentation. This pass improves the existing renderer; it does not claim
parity with or superiority over other shipped games.

## Implemented

- GGX dielectric highlights with Schlick Fresnel, material smoothness and
  normal-derivative roughness filtering on architectural surfaces and water.
- Flat-normal architectural materials share the normal-mapped BRDF. Painted
  surfaces remain dielectrics; no invented metallic maps are used.
- Pump, compressor, pipe, and gate materials use restrained dielectric gloss.
  Their fixture-light directions are sampled once per object, keeping the
  improved highlights out of the per-triangle light-search hot path.
- Removed duplicate unshadowed lamp shading that mixed world/tangent spaces.
- Trilinear texture filtering and up to 8x anisotropy when supported.
- Vulkan scene color now renders to RGBA16F when the device supports blended,
  sampled half-float attachments (RGBA32F is the secondary option). Lighting,
  emission, additive effects and transparent water stay in linear HDR until
  the presentation composite applies bloom, filmic tone mapping and display
  encoding once. Unsupported hardware retains the prior LDR path.
- The scene target uses 4x MSAA when color and depth formats support it, 2x on
  narrower hardware, and 1x as a compatibility fallback. Resolve antialiases
  geometry edges while leaving texture art and the full-resolution HUD crisp.
- Service-map ambient values are now 0.48-0.55. The concrete pressure floor
  gets a subtle tileable normal derived from its albedo and a full trilinear
  mip chain.
- Removed forced gameplay depth blur and noisy depth-only AO. The composite
  keeps the HUD crisp and limits bloom to bright emitters.
- Matched mouse yaw/pitch angular sensitivity, +/-86 degree vertical look,
  and save validation/tests for both new limits.
- Retired static vertex buffers only after frame completion, synchronized shared
  scene attachments, fixed material-cache ownership and temporary paint reuse,
  and preferred device-local mapped vertex memory where available.

## Validation and limits

Run `--vulkan-test`, `--smoke-test --vulkan`, `--performance-test`,
`--performance-window` and `--campaign-inspection`.
The window benchmark reports actual presentation dimensions, 30 warmup frames,
120 measured frames and mean/p95/p99 latency. Headless timings include blocking
readback and omit the presentation composite: they are not windowed FPS.
On an RTX 5060 Ti at 1920 x 1080 with 4x MSAA, maps 6-9 averaged 106.5,
112.6, 127.8 and 124.2 FPS. Their p95 frame times were 16.81, 12.68, 10.53
and 11.00 ms. Results depend on hardware and window load. The 4x resolve has
a measurable cost, especially on Cable Vaults; it remains above 100 FPS on
this test system.

Fixed-camera pixel comparisons use the same nine views at 640 x 360. In the
scene crop with the header and HUD excluded, normalized mean absolute RGB
difference against the v0.5.9.6 build ranges from 1.61% to 9.49% of full
channel range (average 4.52%). This measures the image change; it does not
measure similarity or parity with another game's artwork. The before/after
panel is at `diagnostics/pixel-compare/service-maps-before-after.png`.

Inspection views frame the cable trench,
switchgear, pump machinery, observation console, bridge and compactor bypass.
The earlier `diagnostics/render-upgrade/after/maps-contrast.png` is still an
art review capture; the pixel comparison above uses controlled fixed cameras.

The baseline window benchmark failed with Vulkan device loss entering Ashfall
following Gantry; the integrated run completed all twelve scenes. The baseline
therefore cannot support a full-suite percentage speedup claim. Camera sweep
pitch also changed with the sensitivity correction.

Headless captures read back the HDR scene and apply the same tone curve, but do
not execute the presentation-only bloom and HUD composite. The renderer still
uses cached vertex lighting and CPU visibility rays rather than a GPU shadow
map pipeline, has no reflection probes, and has no temporal anti-aliasing.
These remain future engineering work.
