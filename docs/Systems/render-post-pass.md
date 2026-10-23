---
tags: [renderer, systems]
date: 2026-10-23
---

# Post pass: occlusion, depth haze, bloom, grade

The first stage of the modern render upgrade. The goal is the "thickness"
Complementary and Bliss give Minecraft: the crisp PSX texels stay, and depth,
contact shadowing and graded light are added around them.

## Frame flow

1. **Scene pass (unchanged geometry).** HDR targets store linear radiance in
   RGB. Opaque fragments write *planar camera depth* into alpha. The additive
   and water pipelines blend alpha as `ZERO, ONE`, so alpha stays a blend
   factor and never overwrites depth. The clear alpha of 1000 means "far".
2. **Post pass** (`renderer/shaders/post.frag`). A fullscreen draw reads the
   resolved scene and writes the final linear image into `post`.
3. **Outputs.** Headless readback and the window composite both read `post`,
   so tests, inspections and the game show the same picture. The composite
   skips its own small bloom when post already ran.

It needs no extra attachments or multisampled depth sampling. It runs only on
HDR targets and outside VR, where stereo projection differs.

## Effects

- **Ambient occlusion:**
  - 12-sample golden-angle spiral in a 0.7 m world kernel;
  - view positions rebuilt from the projection (focal 1.3, aspect from the
    view state) and normals from the nearer neighbours;
  - squared response, so corners and contacts go dark while open walls stay
    clean;
  - brighter direct light keeps more of its value than bounce light does.
- **Depth haze:** aerial perspective toward the scene atmosphere colour
  (`setAtmosphere`), starting 2 m out, so large halls read as layered depth.
- **Bloom:** 24 taps in three rings at 5, 20 and 45 px, from highlights above
  0.85.
- **Grade:** saturation 1.08, a faint cool lift in shadows and a warm tint in
  highlights.

## Controls

- Console `r_post on|off|ao` (`ao` shows the occlusion buffer).
- Command line `--nopost` or `--post-ao` for A/B inspection renders.
- `--vulkan-test` measures the raw scene shader and pins post off.

## Cost

The Exclusion yard window benchmark measured 4.6-5.2 ms with post against
4.7 ms without, which is within run-to-run noise.

## Next stages

- Denoise the occlusion with a depth-aware blur.
- Per-pixel lighting with real-time shadow maps for key lamps.
- Hemispherical bounce light.
- Volumetric light that reuses the stored depth.
- Screen-space reflections on water and wet floors.
