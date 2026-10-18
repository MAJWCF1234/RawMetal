# Expanded reduced-texture pass

2026-10-18. Follow-up to the [initial neural texture experiment](neural-textures-20261018.md).
The goal is a whole executable smaller than the procedural release, not only
smaller payloads than original PNG images. The procedural root baseline is
26,434,048 bytes (25.21 MiB), SHA-256
`8AF412E7214016E025B7FEDEF937A618AD9B15368AE329362FF292FDC3A9970C`.

The final trained candidate is **25,468,416 bytes / 24.29 MiB**. It is **965,632
bytes / 943 KiB / 3.65% smaller** than that procedural baseline and leaves
**8,086,016 bytes / 7.71 MiB** beneath the 32 MiB ceiling. Relative to the
initial 27,063,808-byte neural candidate, it saves 1,595,392 bytes / 1.52 MiB.
These are measured whole PE executable sizes, including native code and model.

## Selected additional textures

All five use twofold reduction on each axis and the existing 2,204-byte shared
model. No extra weights, animation banks or runtime frameworks are added.

| Surface | Runtime dimensions | Stored dimensions | PSNR | Block luminance SSIM |
|---|---|---|---:|---:|
| Barrel | 510 × 475 | 255 × 238 | 38.27 dB | 0.927 |
| Environment crate | 619 × 730 | 310 × 365 | 36.42 dB | 0.896 |
| Rust backing | 512 × 512 | 256 × 256 | 38.59 dB | 0.928 |
| Pump | 512 × 603 | 256 × 302 | 34.76 dB | 0.920 |
| Packing crate | 700 × 800 | 350 × 400 | 34.84 dB | 0.868 |

The seven active packets plus shared weights occupy 533,360 packed bytes.
The five additions save 1,551,392 packed bytes against their original PNG
representations. This is separate from the whole-EXE comparison above, which
also accounts for replacing the prior procedural materials with originals.

Original/native texture sheets were inspected. The crate markings, pump
openings, barrel rims and rust shapes remain readable. Fine grain is softer;
this remains lossy reconstruction, not exact recovery. Fourfold reductions
were rejected. Compressor and electronics variants were also rejected for
loss of small details. Actors, weapon textures, lettering/sign panels, normal
maps and emissions are untouched.

The panel metal from the initial experiment is improved from fourfold to
twofold reduction (SSIM 0.956 instead of 0.929), spending about 43 KiB of the
new savings on fidelity. Water retains its twofold reduction. Seven active
textures share the same model.

## Odd-sized atlases

The encoder edge-pads right/bottom to a multiple of the reduction factor.
Native reconstruction crops that padding, preserving the authored width,
height and UV layout. The parser bounds dimensions to 1024 and checks the
exact ceil-divided source dimensions. No image resizing, inference or crop
work is added to the per-frame path.

The decoder suite passes 750 checks, including odd x2/x4 crop strides and
malformed sizes. Each selected prop was compared against the Python model:
maximum difference one 8-bit channel level, 2–8 channels per image. Repeated
native decodes were deterministic. Warm prop load-time medians were about
15–33 ms each; these are not frame timings.

## Reproduction

The first expanded build inherited profiles containing startup work. Its
exclusive ABBA batch measured about 305 versus 321 FPS, so it was rejected
for promotion. A fresh instrumented build trained sustained Exclusion plus
six Ashfall/campaign scenes, with counters reset after measured gameplay
started in each run. Training timings are not performance scores.

The final exclusive ABBA batch used unchanged Vulkan settings at 640 × 360,
linear HDR and 4× MSAA, 30 warmup frames and 3,600 measured frames per run.

| Run | Mean frame time | FPS | p99 frame time |
|---|---:|---:|---:|
| Procedural A | 3.1665 ms | 315.81 | 4.6275 ms |
| Neural A | 3.1954 ms | 312.95 | 4.7495 ms |
| Neural B | 3.1482 ms | 317.64 | 4.7300 ms |
| Procedural B | 3.1092 ms | 321.62 | 4.4851 ms |

Across 7,200 frames per build, procedural averages **318.69 FPS / 3.1378 ms**
and neural averages **315.28 FPS / 3.1718 ms**: **1.07% fewer FPS / 1.08% more
frame time**. Combined p99 is 4.5744 versus 4.7403 ms. This fits the user's
accepted approximate 1.9% speed-for-size trade. Profile/layout differences
remain bundled with texture changes; this does not isolate texture sampling
cost. Successful reconstruction counters confirm **eight startup operations
and zero additional operations across 240 gameplay frames or atlas scale
changes**. Eight operations load seven distinct textures; one is loaded twice.

Exact promoted root executable SHA-256:
`7C96C14B434CA63F3D71BD2A96EA72C947A47871327BE3A923BF53D470F10D42`.

## Final validation and promotion

The exact final build passed full Vulkan smoke, the runtime reconstruction
counter test, all 28 visibility views and Exclusion/service inspection runs.
Culling/reference comparisons reported zero changed pixels. Twelve Exclusion
captures also match the first expanded build exactly, confirming that the new
PGO profiles did not alter those rendered outputs. These are comparisons
between the two neural builds; texture changes versus procedural are intended.
The twelve Exclusion views, eight service views and original/native prop sheets
were inspected. All 231 embedded resources verify against source or expected
native neural output; the standalone decoder suite passes 750 checks.

Promoted to `N:\rawmetal\RawMetal.exe` after verifying the previous root had
not changed, all final checks passed and the new executable was smaller. Its
hash matches the tested candidate. Previous root is retained at
`.build/neural-props/procedural-baseline.exe` for rollback. Ordinary CMake
configuration is restored to PGO OFF with isolated development output.

See [procedural versus final captures](../evidence/neural-textures-expanded-20261018/procedural-vs-final.png)
and [original versus native prop textures](../evidence/neural-textures-expanded-20261018/original-vs-native.png).

`tools/neural_texture_plan.json` records all seven original sources and
reduction factors. Original PNGs remain development references. Run:

```powershell
py -3 tools/encode_neural_textures.py --plan tools/neural_texture_plan.json --out .build/neural-reproduction
cmake --build .build/vs2022 --config Release --target RawMetalNeuralTextureTest
.build/vs2022/Release/RawMetalNeuralTextureTest.exe
```

Evidence is in `docs/evidence/neural-textures-expanded-20261018/`. The new
source/model encoder runs offline; the executable uses only native inference
and embedded reduced packets at load time. Retired procedural work remains in
the hash-verified cold-storage archive documented in the initial report.
