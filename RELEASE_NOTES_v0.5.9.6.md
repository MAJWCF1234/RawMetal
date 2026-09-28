# RawMetal v0.5.9.6 - HDR Light Pipeline

This release improves lighting response across the Vulkan renderer and
rebalances maps 6-9 so local fixtures and visibility-tested shadows shape the
service spaces instead of broad ambient fill.

The Vulkan scene now renders into a blended, sampled RGBA16F target when the
device supports it, with RGBA32F as a secondary option and the existing LDR
target as a compatibility fallback. Surface light, emission, additive effects,
and transparent water remain linear through scene composition; bloom and a
single filmic tone-map/display-encoding pass run before the crisp HUD overlay.
The water regression check now verifies the expected linear-light alpha blend.

Service-map ambient values are tuned to 0.18-0.26 for Cable Vaults, Pump
Annex, Utility Junction, and Waste Handling. The fixture shadows, worn concrete,
grating, and hazard markings retain their authored material treatment. A fresh
nine-view inspection is available at
`diagnostics/render-upgrade/after/maps-contrast.png`.

## Validation

- Release build and embedded-resource verification passed; the EXE is 18,643,968
  bytes, below the 19,000,000-byte release limit.
- `--vulkan-test` passed on an NVIDIA GeForce RTX 5060 Ti, including linear HDR
  water blending, emission, normal mapping, GGX response, depth, and cached
  geometry checks.
- `--smoke-test --vulkan` passed.
- `--campaign-inspection` completed and generated all nine service-map views.

This is a targeted rendering and lighting update, not a claim of parity with
games using GPU shadow maps, reflection probes, or temporal reconstruction.
