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
On an RTX 5060 Ti at 1920 x 1080, the final measured turn averages for
maps 6-9 were 123.8, 128.0, 135.4 and 135.9 FPS respectively. Map 6 p95
was 13.54 ms, so turn spikes remain visible. Results depend on hardware and
window load. The incomplete baseline means these results do not establish a
speedup percentage.

Ambient fill on maps 6-9 was reduced from 0.38-0.42 to 0.18-0.26 so fixture
lighting and cached visibility shadows define the room instead of a uniform
fill washing out the geometry. Inspection views frame the cable trench,
switchgear, pump machinery, observation console, bridge and compactor bypass.
The new nine-view capture is
`diagnostics/render-upgrade/after/maps-contrast.png`. It is for visual review;
camera framing and map dressing changed during this pass, so its pixel delta
against earlier montages is not a controlled renderer benchmark.

The baseline window benchmark failed with Vulkan device loss entering Ashfall
following Gantry; the integrated run completed all twelve scenes. The baseline
therefore cannot support a full-suite percentage speedup claim. Camera sweep
pitch also changed with the sensitivity correction.

Headless captures read back the HDR scene and apply the same tone curve, but do
not execute the presentation-only bloom and HUD composite. The renderer still
uses cached vertex lighting and CPU visibility rays rather than a GPU shadow
map pipeline, has no reflection probes, and has no temporal anti-aliasing.
These remain future engineering work.
