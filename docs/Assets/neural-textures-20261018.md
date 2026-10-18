# Reduced textures with shared neural reconstruction

Historical two-texture experiment. See the
[expanded pass](neural-textures-expanded-20261018.md) for the current selection
and whole-executable comparison.

Experimental source integration, 2026-10-18. The published preview.10/root
executable is unchanged; the trained candidate has not met performance acceptance.

The earlier comparison used bicubic enlargement, not AI. This implementation
actually evaluates a learned residual subpixel CNN when loading resources.
It uses our own training code and weights, with no downloaded model or runtime
framework. Low-resolution feature processing and pixel shuffle follow the
[ESPCN approach](https://arxiv.org/abs/1609.05158).

## Selected surfaces

Only two of twelve inspected surfaces are selected. Signs, normal maps,
characters, weapons, transparency and other high-frequency materials retain
their existing assets. Reconstruction is an approximation, not pixel-exact
recovery of the original image.

| Surface | Stored → runtime | PSNR | SSIM | Original packed image | Neural packed image |
|---|---|---:|---:|---:|---:|
| Panel metal (132) | 128² → 512² | 40.83 dB | 0.929 | 268,038 B | 12,822 B |
| Water (250) | 128² → 256² | 44.23 dB | 0.964 | 60,132 B | 12,480 B |

The shared model adds 2,196 packed bytes. Together these replace 328,170 bytes
with 27,498 bytes: **300,672 bytes / 91.6% saved** against the original packed
images. This is not an executable-size saving against preview.10's much
smaller, approximate RMP1 recipes. Restoring the other original images adds
space back compared with that procedural release.

## Runtime and training

`NTX1` holds a lossless WebP reduced image and a shared model resource ID.
`NTM1` contains 1,084 learned parameters in 2,204 bytes, quantized to int16
with per-layer scales. Three convolutions (1→8, 8→8, 8→4) predict luminance
residuals above a bilinear RGB base. Fourfold reconstruction runs the twofold
model twice. Native SSE requires no AVX2, RTX hardware, external DLL or Python.

The loader produces the same full-resolution RMT1/RGBA representation used by
ordinary textures. Reconstruction is load work; rendering samples the result.
A successful-reconstruction counter and `--neural-texture-runtime-test --vulkan`
check startup, 240 gameplay frames and atlas scale changes independently.
The instrumented Vulkan test passed: three startup reconstructions, **zero**
additional reconstructions across 240 frames, and **zero** across two atlas
scale changes. The counter measures completed load operations, not unique
texture IDs; three startup operations do not mean a third reduced asset.

Training uses seed 1337 and 4,000 joint x2/x4 steps. The right quarter plus an
8-pixel margin is excluded from gradient training. Panel metal, water and
painted wall are entirely excluded from gradient training, but participate in
checkpoint validation; these are validation results, not a blind test.
Quantized validation MSE is 0.00091955 versus bilinear's 0.00099857.

Native selected outputs differ from Python by at most one 8-bit channel level
(three channels in each image). Warm native reconstruction medians were about
19.1 ms for the panel and 3.9 ms for water. Those are load-time measurements,
not frame costs. The parser/unit suite passed 138 checks. The ordinary Release
candidate passed full Vulkan smoke and all 231 embedded-resource checks.

An initial FPS comparison mixed the trained-PGO canonical executable with a
PGO-OFF candidate (about 317 versus 267 FPS). That comparison was confounded;
it must not be attributed to per-frame neural reconstruction.

An exclusive ABBA comparison then used trained, speed-first PGO for the new
candidate too, at unchanged Vulkan settings, 30 warmup + 3,600 measured frames:

| Run | Average frame time | FPS | p99 frame time |
|---|---:|---:|---:|
| Canonical A | 3.1343 ms | 319.05 | 4.6515 ms |
| Neural A | 3.2436 ms | 308.30 | 4.9700 ms |
| Neural B | 3.1617 ms | 316.28 | 4.8561 ms |
| Canonical B | 3.1515 ms | 317.31 | 4.9042 ms |

The aggregate candidate frame time is about 1.9% higher, with variation between
runs. The candidate uses newly trained profiles, not the deleted canonical PGO
database, and source/resource changes are bundled. This is not an isolated
texture-cost experiment and does not prove equal performance. The counter
does prove no reconstruction work occurred during the tested gameplay frames.
**Root remains unchanged; no performance winner or new release is claimed.**

The trained candidate is `.build/neural-textures/optimized/RawMetal.exe`,
27,063,808 bytes (25.81 MiB), with 6,490,624 bytes below the ceiling.
SHA-256: `F722C923DC2B9770DFEEFE3E0ACDA56D67BD502128979C46D6D6ED76F618F5EC`.
Ordinary CMake configuration is restored to PGO OFF and isolated output.

## Cold storage

All retired recipe/OpenKTG work, including uncommitted evolver changes and
comparison artifacts, is preserved at:

`H:\Archives\RawMetal\texture-recipes-20261018`

The snapshot has 954 files, a SHA-256 manifest, a binary working-tree patch,
base commit and `RESTORE.txt`. Every snapshot copy was hash-verified before
inactive workspace files were moved into `retired-from-workspace/` there.
The old canonical executable is also retained. No Git reset was used.

Reproduce the neural experiments with `tools/train_texture_upscaler.py` and
the `RawMetalNeuralTextureTest` CMake target. Model training and source images
remain development inputs; only reduced packets, shared weights and native
inference ship in a candidate.
