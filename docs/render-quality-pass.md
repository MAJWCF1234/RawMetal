# Rendering quality and performance pass

The art target is intentionally coarse source artwork with stable modern
presentation. This pass improves the existing renderer; it does not claim
parity with or superiority over other shipped games.

## Implemented

- GGX dielectric highlights with Schlick Fresnel, material smoothness and
  normal-derivative roughness filtering on architectural surfaces and water.
- Flat-normal architectural materials share the normal-mapped BRDF. Painted
  surfaces remain dielectrics; no invented metallic maps are used.
- Removed duplicate unshadowed lamp shading that mixed world/tangent spaces.
- Trilinear texture filtering and up to 8x anisotropy when supported.
- Removed forced gameplay depth blur, noisy depth-only AO and double tone mapping.
  Bounded spatial edge AA and restrained symmetric bloom precede crisp HUD composition.
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
Before/after evidence is in `diagnostics/render-upgrade/`.

The baseline window benchmark failed with Vulkan device loss entering Ashfall
following Gantry; the integrated run completed all twelve scenes. The baseline
therefore cannot support a full-suite percentage speedup claim. Camera sweep
pitch also changed with the sensitivity correction.

Remaining renderer limits: LDR scene target and display-space bloom, cached
vertex lighting rather than a complete per-pixel light/shadow pipeline, no true
metallic workflow or reflection probes, and spatial AA rather than temporal
reconstruction. These are future engineering work, not features supplied by
this pass. Imported mesh props still use their existing lighting path.
