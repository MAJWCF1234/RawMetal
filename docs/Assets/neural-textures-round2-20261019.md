# Neural texture screening, second pass

2026-10-19. Follow-up to [preview.11](neural-textures-expanded-20261018.md).
Shipped as **v0.7.2-preview.12**; see the [patch statistics](../Performance/preview12-stats-20261019.md).
Baseline root: 25,468,416 bytes, SHA-256
`7C96C14B434CA63F3D71BD2A96EA72C947A47871327BE3A923BF53D470F10D42`.

The candidate is **24,173,568 bytes / 23.05 MiB**, saving **1,294,848 bytes /
1.235 MiB** against preview.11 and leaving **9,380,864 bytes / 8.946 MiB**
under the unchanged 32 MiB ceiling. Compared with procedural preview.10, the
combined reduction is **2,260,480 bytes / 2.156 MiB**. These are whole executable
sizes, not PNG-only estimates. Selected payloads save 1,298,144 packed bytes;
small linker/resource-layout changes account for the remainder.

Candidate SHA-256:
`54E771DF0DBE6F3F08584B01B6BC4CC5D9E1A6BE01E341D51245BE9E7DC7509B`.

## Screening and selection

Screened 30 prop/environment textures at twofold reduction, then separately
screened three large nonhuman creature atlases. Selected eight props/materials
and the Scissors atlas: **nine additions, sixteen reduced textures total**.
The other 24 trials retain their original resources. Human characters, hands,
weapons, normal maps, emission maps and interface assets are unchanged.

| Resource | Texture | Runtime dimensions | Stored dimensions | PSNR | Block luminance SSIM |
|---|---|---|---|---:|---:|
| 117 | Scissors creature | 1024 × 1024 | 512 × 512 | 37.73 dB | 0.969 |
| 181 | Generator | 384 × 347 | 192 × 174 | 33.53 dB | 0.867 |
| 182 | Facility metal 4 | 192 × 192 | 96 × 96 | 42.31 dB | 0.942 |
| 183 | Facility metal 3 | 192 × 192 | 96 × 96 | 38.76 dB | 0.887 |
| 184 | Facility metal 6 | 192 × 192 | 96 × 96 | 36.64 dB | 0.867 |
| 192 | PC casing atlas | 348 × 307 | 174 × 154 | 34.54 dB | 0.915 |
| 261 | Transformer box | 311 × 426 | 156 × 213 | 32.16 dB | 0.840 |
| 268 | Service van | 538 × 284 | 269 × 142 | 34.94 dB | 0.879 |
| 269 | Van metal | 192 × 192 | 96 × 96 | 40.61 dB | 0.928 |

Original/reconstruction sheets and Scissors detail crops were inspected.
Selections preserve the large surface patterns, vents, seams and object
identity; fine grain is softer. This is lossy reconstruction. Metrics alone
were not the acceptance rule: large flat areas can mask loss of important
small details. The standalone keyboard, wall-box label, gate warning markings,
forklift atlases and fan grille stay original. Most 256-pixel walls/floors and
the carpet lose too much fine structure. Spider and Wasp also stay original.

All nine native outputs match the Python reference within one 8-bit channel
level (0–9 differing channels per image). Nine repeated native decodes per
texture are deterministic. The shared model, native decoder and file format
are unchanged; no additional inference or allocation is introduced per frame.
Original PNGs remain development inputs. The checked-in encoding plan records
the sixteen active source images and factors.

## Rejected offline refinement experiment

Tested optimizing the stored reduced pixels through the fixed decoder for 96
steps on gate, generator, transformer and van. Quality improved only about
0.1–0.3 dB, SSIM was nearly unchanged and one result regressed. Packets grew
roughly 20–35%. No refined packets or refinement code are shipped. The local
prototype and patch remain under `.build/neural-round2/` for investigation;
screening metrics are retained with the evidence.

## Evidence and reproduction

The exact candidate passed full Vulkan smoke, the reconstruction counter test,
28 visibility views, twelve Exclusion captures and 23 freight-detail captures.
Visibility/reference comparisons report zero changed pixels. The existing
native decoder suite passes 750 checks; all 231 embedded resources verify
against source or expected native reconstruction. Weapon/arms/Spider/Wasp
model previews remain pixel-identical to preview.11. Scissors and freight
before/after captures were inspected, including the terminal front and van.

The counter reports **17 startup reconstructions, zero more during 240
gameplay frames, and zero more during atlas scale changes**. Water has two
renderer variants, so sixteen distinct textures produce seventeen loads.

Eight 3,600-frame FPS runs used the same trained PGO database, unchanged
640 × 360 Vulkan settings and ABBA followed by reversed order. Both batches:

| Run | Mean frame time | FPS | p99 frame time |
|---|---:|---:|---:|
| Preview.11 A | 3.6623 ms | 273.05 | 9.3324 ms |
| Candidate A | 3.1635 ms | 316.10 | 4.6486 ms |
| Candidate B | 3.2156 ms | 310.99 | 4.7623 ms |
| Preview.11 B | 3.3238 ms | 300.86 | 4.8456 ms |
| Candidate C | 3.4243 ms | 292.03 | 4.9841 ms |
| Preview.11 C | 3.2318 ms | 309.43 | 4.8292 ms |
| Preview.11 D | 3.0815 ms | 324.51 | 4.4422 ms |
| Candidate D | 3.1179 ms | 320.72 | 4.4956 ms |

All runs are retained. Aggregate FPS is 300.77 versus 309.57, but both builds
vary substantially across runs and the order reverses the apparent result.
**This does not establish an FPS gain or a consistent performance change.**
The verified outcome is smaller storage with no new per-frame reconstruction
work; clean, low-variance benchmarking remains necessary for a speed claim.

Promoted the exact tested candidate to `N:\rawmetal\RawMetal.exe`, after
checking that the previous root had not changed. Root matches the candidate
SHA-256 above. Rollback: `.build/neural-round2/preview11-baseline.exe`.
Ordinary CMake configuration is restored to PGO OFF with isolated output.

[Scissors texture comparison](../evidence/neural-textures-round2-20261019/scissors-original-vs-neural.png),
[model previews](../evidence/neural-textures-round2-20261019/model-before-vs-after.png),
[freight before/after](../evidence/neural-textures-round2-20261019/freight-before-vs-after.png).

Evidence: `docs/evidence/neural-textures-round2-20261019/`.

```powershell
py -3 tools/encode_neural_textures.py --plan tools/neural_texture_plan.json --out .build/neural-reproduction
.build/vs2022/Release/RawMetalNeuralTextureTest.exe
```

Runtime generation remains a load-time operation followed by cached sampling.
All source texture dimensions and UV layouts are preserved. Build and FPS
comparisons use the same trained PGO database as preview.11; no source changes
to renderer, AI, model loading or native reconstruction are required here.
