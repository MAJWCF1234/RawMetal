# Freight and shared Vulkan rendering pass — 2026-10-01

This 0.6.2-dev build addresses the supplied freight critique and patches the
flat blue beams before distribution. The campaign still has 32 chunks. It does
not add a district-specific engine or require custom campaigns to select a
special render path.

## Shared renderer changes

Lighting lookup for mesh triangles now uses the world-space triangle center,
captured before camera transformation. Fixture and prop lighting samples six
positions outside their oriented bounds; previously the solid object center
could shadow itself. Physical exterior faces select the correct probe even
when an imported mesh has mixed triangle winding. World geometry still blocks
light. These fixes apply throughout the game, including custom campaigns.

Wood, concrete, computer and painted hardware textures now have distinct
normal-map and specular strengths. Vulkan normal-map mip uploads retain the
length lost when averaging normal directions in their existing alpha channel.
The fragment shader uses that variance to increase GGX roughness at distance,
reducing specular aliasing without another texture fetch or descriptor.
Light-shaft scattering has a lower cap to reduce the fog wash over geometry.
This is a refinement of the existing renderer, not a new deferred renderer,
ray tracing implementation or complete PBR asset conversion.

The first beam revision compressed a nearly black panel texture into a narrow
blue tint, making it look textureless. Material 12 now preserves the existing
pressureworks steel texture unchanged, with rust, scratches, metre-based UV
repeats, derived normals and restrained gloss. Close and wide Vulkan captures
were inspected after this correction. No new asset files were needed.

## Authored freight changes

Warehouse crossovers and wide side decks previously concealed the storage
height behind an entrance ceiling. Narrow side galleries and two staggered
bridges now expose the full rack height. Existing stair access, restricted
manifest and exit elevation remain traversable. Workstations sit outside the
central cargo lane. Lamps have cantilever brackets attached to the galleries.

Intake has a segmented gate surround, overhead hoist, mounted control panel,
bollards, threshold markings and two cargo lanes. The inspection pit has
track-height workshop decks, bearing girders, hazard lips and mounted service
lights. Its west wall leaves an opening so the chassis is visible on approach.
Platform benches sit along the margin rather than crossing the sightline.
The bore has regular structural ribs, service pipes and a limited collapsed
edge instead of a broad barrier across the route. Manifest office changes in
this pass are primarily the shared lighting correction; its layout still needs
further art development.

## Verification and comparison

Release build: 20,793,856 bytes, below the 22,000,000-byte limit. All 159 embedded
resources match their original pixels or bytes. Campaign traversal, shared
mechanisms, world isolation, streaming, save/load and Vulkan smoke checks passed.
The final beam-patched executable also passed `--vulkan-test`.

`--freight-art-inspection` captures twelve fixed views through actual Vulkan
without the HUD or viewmodel. Local evidence lives in
`diagnostics/freight-art-direction`: before/after PNGs, `comparison.png`,
`area-thumbnails.png` and `pixel-comparison.json`. The warehouse canyon differs
on 99.63% of pixels with mean absolute RGB difference 39.62/255; intake differs
on 99.55% with mean difference 25.05/255. These measure visible change, not
quality. Some legacy views change only slightly; this pass does not redesign
their layouts. Thumbnail inspection confirms separate intake, warehouse, pit,
office, platform and bore silhouettes, but player feedback remains necessary.

Performance comparison used the previous Freight Contents development archive
and the final corrected executable, run serially on the same RTX 5060 Ti:
1920x1080, 4x MSAA, uncapped native swapchain, 30 warmup frames and 120 measured
frames per scene. The twelve whole-game scenes delivered 118–158 FPS after the
change. The eight freight scenes delivered 128–159 FPS, with zero static map
rebuilds during motion. Individual scene timings fluctuate; this single run is
not evidence of a general speed improvement. Reports retain average, p95, p99
and maximum frame times in both before/after directories.
The ascent and Cable Vaults scenes were 6.3% and 5.6% slower in this run;
those remain performance follow-up targets despite the acceptable average FPS.

The local patch archive is `artifacts/RawMetal-0.6.2-dev-Vulkan-Art-Pass.zip`.
`RawMetal.exe` and `RawMetal.zip` contain this corrected development build.
The published v0.6.1 GitHub release remains separate.
