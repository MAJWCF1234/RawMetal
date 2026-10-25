# Surface sets validation — 2026-10-25

Built in the feature worktree with `Build.cmd` (Release/MSVC/Vulkan). The build
verified 1,036 packaged resources and 61 neural reconstructions. Executable:
48,557,056 bytes.

| Check | Result |
| --- | --- |
| `RawMetal.exe --each-test` | 27 passed, 0 failed; exit 0 |
| `RawMetal.exe --smoke-test` | Exit 0; copied custom pack loaded all 20 maps |
| `RawMetal.exe --vulkan-test` | Exit 0; hardware remained active |
| `py -3 tools/level-editor/test_runtime_records.py` | 7 passed |

The campaign-maps test validates all ten surface sets, all 25 imported finish
resources, complete imported normal/mip chains, all nine new structure materials,
nonzero crystal/lab emission, every parser enum value, defaults and invalid inputs.
Legacy textures retain their existing optional normal maps.

Test-only rooms generate 40 PPM captures: ten surfaces plus the legacy default
and nine structure materials, on both CPU and Vulkan. No campaign builder was
changed for the final implementation. No source textures or rendered images
were viewed; these are numerical checks rather than visual quality approval.

| Frame statistic (8-bit luminance) | CPU, 20 frames | Vulkan, 20 frames |
| --- | --- | --- |
| Mean luminance range | 15.8699–150.115 | 40.0943–104.542 |
| Standard deviation range | 10.1269–85.7064 | 12.2831–48.3362 |
| Pixels with luminance <1, total | 12,693 | 0 |
| Pixels with luminance >254, total | 5,406 | 0 |

All per-frame brightness/variation/clipping checks passed. Every surface and
material produced a distinct frame within each backend. GPU surface frames
reused their static geometry on subsequent renders. Changing finishes at the
same Game address exercises cache invalidation. Removing crystal emission alone
reduced 122,872 color channels by approximately 6.13 million channel levels.

Vulkan atlas encoding covered 732 atlases and 55,715,502 mip channels; fast and
reference encoders produced the same byte hash, 14879316486862657634. The smoke
normal-map checks passed for the existing materials and all imported finishes.

Reproduce the frame checks with `RawMetal.exe --each-test=campaign-maps`.
Measurements are written to `surface-sets-test.txt`; captures are written to
`.build/surface-evidence/`. Authoring fields and material IDs are documented in
[`MAP_SYSTEM.md`](../MAP_SYSTEM.md).
