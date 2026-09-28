# RawMetal v0.5.9.7 - Service Map Rendering

Maps 6-9 were too dim between fixtures, and the Vulkan scene drew with one
sample per pixel. This release raises service-map ambient values from 0.18-0.26
to 0.48-0.55 while retaining fixture visibility shadows, adds mipmapped
surface normals to the concrete pressure floor, and enables 4x MSAA (2x or 1x
fallback according to hardware support). The resolved scene is antialiased
while the low-resolution texture art and full-resolution HUD keep their
authored sharpness.

The fixed-camera before/after capture compares the same nine views at
640 x 360. In the scene crop without the header and HUD, normalized mean
absolute RGB difference ranges from 1.61% to 9.49% across the views, averaging
4.52%. The largest shift is Cable Trench, which shows the broader fill light;
the smallest shifts are views dominated by dark machinery. This quantifies the
image change, not visual parity with Half-Life 2 or any other game.

See the [same-camera before/after panel](https://github.com/MAJWCF1234/RawMetal/releases/download/v0.5.9.7/service-maps-before-after.png).

## Validation

- Release build and embedded-resource verification passed; the EXE is
  18,649,600 bytes.
- `--vulkan-test` passed on an NVIDIA GeForce RTX 5060 Ti with 4x MSAA.
- `--smoke-test --vulkan` completed the full gameplay and capture suite.
- `--campaign-inspection --vulkan` produced all nine fixed-camera frames.
- The native 1920 x 1080 swapchain benchmark passed. Maps 6-9 averaged
  106.5, 112.6, 127.8, and 124.2 FPS respectively; p95 was 16.81, 12.68,
  10.53, and 11.00 ms. These results depend on hardware and workload.

This is a focused renderer update. RawMetal still does not have GPU shadow
maps, reflection probes, or temporal antialiasing, so this release does not
claim parity with games built around those systems.
