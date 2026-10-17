---
tags: [performance, exclusion, validation]
date: 2026-10-17
---

# Competitive round 4: 200 FPS target

**The 200 FPS average target is met in the controlled Exclusion lab.** The final
candidate records **199.35, 202.51 and 202.29 FPS**, a **202.29 FPS** median versus
its paired published reference's **187.77 FPS**. This is an average throughput
result, not a 200 FPS floor. Median 1% lows improve **112.49 -> 125.55 FPS**.

Before this competition, the round-three winner was promoted unchanged to
`N:/rawmetal/RawMetal.exe`, pushed to `main` at
`a6ae05f13cf8beb04189ea6f0de9504204da073f`, and published as
[preview.6 performance hotfix](https://github.com/MAJWCF1234/RawMetal/releases/tag/v0.7.2-preview.6).
GitHub's uploaded asset SHA256 and byte size match the tested root binary.
The superseded preview.5 release was deleted after verification; its source tag
and history remain. The root EXE and `main` remain this published hotfix.

This next candidate is separate on `codex/performance-round4`; the tested local
binary is `.build/competitive-round4-20261017/winner.exe`. Executables are
intentionally ignored by Git; shipping EXEs are GitHub release assets.

## Final paired competition

RTX 5060 Ti, native hidden **640 x 360** window, Vulkan linear HDR / **4x MSAA**,
live muted device mixer. Each `--lab-performance` replay has 30 simulated warmup
plus **3,600 measured frames**, fixed 60 Hz simulation and combat/camera replay.
Deep instrumentation is disabled. These are CPU wall timings through render and
presentation, not GPU timestamps or a guaranteed rate at other resolutions/maps.

| Three-run median | Published preview.6 | Corrected compact stream | Stream + inlining winner |
| --- | ---: | ---: | ---: |
| Average FPS | 187.77 | 197.20 | 202.29 |
| 1% low FPS | 112.49 | 118.63 | 125.55 |
| Mean frame time, ms | 5.326 | 5.071 | 4.943 |
| P99 frame time, ms | 8.277 | 7.705 | 7.491 |
| P99 minus P1, ms | 5.158 | 4.605 | 4.443 |
| Simulation mean, ms | 1.552 | 1.546 | 1.583 |
| Scene preparation mean, ms | 2.838 | 2.694 | 2.537 |

Versus its paired published reference, the winner gains **7.7% average FPS**,
**11.6% 1% lows**, and reduces P99 **9.5%** and spread
**13.9%**. Mean frame time drops **0.382 ms**.
The last inlining step adds **2.6%** average throughput over
the compact stream in that same batch. It costs **454.5 KiB** of native code.

Simulation is slightly more expensive in this batch, **1.552 -> 1.583 ms**;
this is rendering/compiler savings, not a claim of cheaper AI. No AI code,
update rate, movement sampling, animation rate or game rule is changed.
The per-run reciprocal-P1 fast-end minus 1%-low FPS gap changes
**206.7 -> 202.5 FPS**. Neither higher averages nor narrower millisecond
spread establish uniform pacing or a minimum frame rate. The final contender's
worst retained frame is **11.467 ms**.

## What advances

- Keep every actor position, UV and per-corner light at full float precision.
  Vulkan derives edge vectors, UV differences and the outward vector from those
  original values, with `precise` expressions in the original CPU operation order.
  Records shrink **152 -> 92 bytes** (39.5%); no quantization or reduced animation.
- Store the original actor center in three unused W channels of its lighting
  record. The original **400-byte** lighting ABI and ordinary world shader remain
  intact. No additional per-actor record or descriptor is needed.
- Resolve immutable material settings once per actor/material, lazily on its first
  submitted face, preserving material creation and draw order. These records do
  not survive a draw or retain stale texture generations.
- Write each fully assigned triangle directly into the retained submission vector
  instead of constructing and copying another temporary record.
- MSVC Release uses **/Ob3** inlining with existing optimization/link folding,
  unchanged floating-point rules and unchanged baseline ISA. Debug is unchanged;
  cold import sources retain /O1. The hard executable-size verifier still runs.
- Vulkan **1.0**, eight vertex attributes and existing storage buffers; no new
  optional hardware feature. CPU actor preparation and near-far-limit fallback
  remain available. A physical 2015 GPU was not available for driver testing.

Meshes, fingers, waist/body animation, weapon alignment, materials, lighting,
floor rejection, transparency, depth ranges and all game/save rules retain their
existing inputs. No new assets, animation banks, maps or AI kernel work is added.

## Pixel investigation and correction

The first compact stream passed functional checks, but direct cross-build captures
found a stable **132-pixel** background difference in view 1, plus view 7 variation.
It did not advance on timing alone. Repeating both binaries showed existing
same-binary capture variation in views 4-7, including **521 pixels** in the
published build's view 7. Those differences were retained, not silently excluded.

Restoring the original lighting ABI and reusing unused W channels removes the
stable difference. The corrected stream matches a repeated published capture in
**all twelve encounter views, zero changed pixels**. Its ordinary world vertex
shader is byte-identical to the shader actually embedded in the published EXE:
**15,620 bytes**, SHA256 `d94674e9fb4c3b5ff51cfe97525fbaa8fc78cdf5d76271e4ee3678c06cc7a1d7`.
The final winner contains those exact same shader bytes; this is verified by
searching both executables, not only comparing GLSL source.

**Promoted 2026-10-17:** fresh interleaved root-vs-winner labs (3+3) median
**189.8 → 205.9 FPS** (+8.5%); smoke/exclusion/friendly/stealth PASS. Winner
copied to `N:/rawmetal/RawMetal.exe` as **v0.7.2-preview.7**; experiment folder
removed after promote. SHA256 unchanged from competition `inline`/`winner.exe`.

## Experiments and all measured runs

All **36 timed replays** have identical AI query/route counts, sensing/planning
poll/run/wake counts and final health. Terrain/state query totals can differ with
simulation/worker query distribution and are not treated as gameplay fingerprints.
Raw CSV/text and all outliers are retained, including the rejected combined
candidate's **19.324 ms** spike. No builds or tests run alongside timed benchmarks.

Five balanced batches:

1. canonical 1, cache 1, combined 1, combined 2, cache 2, canonical 2,
   canonical 3, cache 3, combined 3.
2. reference 1, stream 1, stream 2, reference 2, reference 3, stream 3.
3. stream-control 1, aligned 1, aligned 2, stream-control 2,
   stream-control 3, aligned 3.
4. final-reference 1, abi 1, abi 2, final-reference 2, final-reference 3, abi 3.
5. canonical-last 1, abi-control 1, inline 1, inline 2, abi-control 2,
   canonical-last 2, canonical-last 3, abi-control 3, inline 3.

All canonical/reference variants are byte-identical preview.6 copies.
`stream-control` repeats `stream`; `abi-control` repeats the corrected `abi`.
Compare within a batch. Host conditions drift; metrics from different batches
are not pooled to create the headline. Average FPS is reciprocal mean frame time;
1% low is reciprocal mean of each run's slowest 36 frames. Summary values are
medians of per-run metrics; P1/P99 use sorted indices 36 and 3564.

The exact field cache/larger state hints lose and are removed. Combining those
with submission changes also loses; that batch does not isolate material-hoisting
alone. The 96-byte aligned stream ties its 92-byte control **194.83 vs 194.89 FPS**,
not a demonstrated extra win. The original compact stream is superseded by the
shader-preserving correction. Inlining is retained only in the final candidate.

| Variant / run | Average FPS | 1% low FPS | P99 ms | P99 minus P1 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| canonical 1 | 187.80 | 114.22 | 8.277 | 5.112 | 10.064 |
| canonical 2 | 190.05 | 116.48 | 7.986 | 4.844 | 10.614 |
| canonical 3 | 184.26 | 114.02 | 8.106 | 4.965 | 9.790 |
| cache 1 | 185.32 | 116.39 | 8.235 | 5.080 | 9.381 |
| cache 2 | 184.69 | 109.65 | 8.560 | 5.420 | 12.190 |
| cache 3 | 184.28 | 108.43 | 8.565 | 5.382 | 11.966 |
| combined 1 | 184.95 | 109.81 | 8.457 | 5.298 | 11.041 |
| combined 2 | 187.14 | 102.32 | 8.393 | 5.173 | 19.324 |
| combined 3 | 184.60 | 108.03 | 8.453 | 5.199 | 10.921 |
| reference 1 | 183.44 | 105.77 | 8.597 | 5.396 | 12.241 |
| reference 2 | 185.83 | 112.86 | 8.254 | 5.073 | 13.736 |
| reference 3 | 183.50 | 104.95 | 8.558 | 5.371 | 13.514 |
| stream 1 | 191.79 | 112.08 | 8.187 | 5.095 | 10.694 |
| stream 2 | 196.25 | 121.93 | 7.641 | 4.612 | 10.637 |
| stream 3 | 191.56 | 118.28 | 7.934 | 4.815 | 10.503 |
| stream-control 1 | 196.44 | 120.18 | 7.738 | 4.683 | 11.696 |
| stream-control 2 | 194.83 | 121.07 | 7.782 | 4.670 | 10.114 |
| stream-control 3 | 191.55 | 118.15 | 7.902 | 4.762 | 11.695 |
| aligned 1 | 195.72 | 121.67 | 7.718 | 4.626 | 10.548 |
| aligned 2 | 194.89 | 114.01 | 7.938 | 4.893 | 11.213 |
| aligned 3 | 193.72 | 123.23 | 7.671 | 4.565 | 10.537 |
| final-reference 1 | 186.12 | 116.55 | 8.080 | 4.825 | 11.692 |
| final-reference 2 | 188.29 | 116.51 | 8.171 | 5.022 | 9.963 |
| final-reference 3 | 187.32 | 113.41 | 8.257 | 5.162 | 10.575 |
| abi 1 | 196.79 | 121.72 | 7.713 | 4.616 | 8.947 |
| abi 2 | 197.59 | 123.51 | 7.459 | 4.381 | 11.887 |
| abi 3 | 193.75 | 112.74 | 8.215 | 5.099 | 10.882 |
| canonical-last 1 | 188.87 | 112.49 | 8.277 | 5.158 | 10.686 |
| canonical-last 2 | 187.77 | 117.06 | 8.107 | 5.018 | 9.276 |
| canonical-last 3 | 184.67 | 111.47 | 8.374 | 5.226 | 10.969 |
| abi-control 1 | 197.47 | 124.29 | 7.588 | 4.463 | 9.294 |
| abi-control 2 | 194.00 | 115.22 | 7.912 | 4.775 | 10.363 |
| abi-control 3 | 197.20 | 118.63 | 7.705 | 4.605 | 10.623 |
| inline 1 | 199.35 | 125.55 | 7.491 | 4.443 | 9.372 |
| inline 2 | 202.51 | 118.34 | 7.772 | 4.679 | 11.467 |
| inline 3 | 202.29 | 127.24 | 7.404 | 4.350 | 9.818 |

## Executable identity and evidence

| Binary | Bytes | SHA256 |
| --- | ---: | --- |
| canonical | 26,939,904 | `1d4c6fd9da0d471d9fb1924ef0230606ec22cb493b443577f4782b92a27536e9` |
| cache | 27,176,960 | `54ad80c45c9768faf3ffc7b1e1b1b4fe8c38079f9280db72ac8380422715a17f` |
| combined | 27,177,984 | `7ee944dca36ed19c182379fbb21b7d8f8cc4a3aff27f099e0aa9cbe3175ddc1c` |
| stream | 26,939,904 | `2b5a239c5868e90201ea3ce78d2c479a5aa54c7bbe50233eaf3459827a2acae3` |
| aligned | 26,939,904 | `d4faeaabfe7fc343f4a8c32e86036ad9306438a63c0022437f4ab13fe2ffc75c` |
| abi | 26,939,904 | `4c5d596f0512f26a350bbbed21d2f2b187ec0354425cff7136d2398dda8684e6` |
| inline | 27,405,312 | `661b557e7d73275327d2ed81edca4d2f6c9b9a9d9bff1ec0a40c3d02c7d69d34` |

The final winner is **26.14 MiB**, leaving **5.86 MiB** below the unchanged
32 MiB ceiling. All **230 packaged resources** pass the lossless build verifier.
No asset data is sacrificed for native-code inlining.

Raw evidence: `.build/competitive-round4-20261017/`, including per-run CSV/text,
`analysis.json`, `evidence.json`, candidate patches, build logs, shader byte proof,
pixel comparisons and validation reports. Rejected and duplicate executables are
removed after recording their identities; patches and measurement evidence remain.
The root canonical EXE is not overwritten by experimental builds.

The target is met only for this defined benchmark. Main-route resolution scaling,
other hardware/drivers, rare hitches and all possible game states remain outside
this finite validation. No 200 FPS floor or universal bug-free claim is made.
