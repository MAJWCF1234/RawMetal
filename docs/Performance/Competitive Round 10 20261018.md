---
tags: [performance, exclusion, validation]
date: 2026-10-18
---

# Round 10: exact SIMD animation, 314 FPS

The SIMD-only candidate wins the final three-way competition at **314.2 FPS**
versus preview.8's **308.4 FPS** in the same batch. **330 FPS remains unmet.**
Promoted unchanged to the root executable and shipped as **v0.7.2-preview.9**.
All **25 final executable regression checks passed**.

## Final competition

Ryzen 7 5800X / RTX 5060 Ti. Unchanged native hidden 640x360 Vulkan window,
linear HDR, 4x MSAA, live muted device mixer, fixed 60 Hz simulation,
30 warmup + 3,600 measured frames per run. Deep instrumentation disabled.
No builds, regression tests or other game benchmarks ran alongside this batch.
Run order: B1, S1, Q1, S2, Q2, B2, Q3, B3, S3.

| Build | Run 1 FPS | Run 2 FPS | Run 3 FPS | Median FPS | Median 1% low |
| --- | ---: | ---: | ---: | ---: | ---: |
| B: installed preview.8 | 308.43 | 295.02 | 312.84 | 308.4 | 187.8 |
| S: SIMD only, winner | 314.23 | 314.15 | 306.40 | **314.2** | **199.9** |
| Q: SIMD + platform index + larger state hints | 300.14 | 314.93 | 313.53 | 313.5 | 194.0 |

The modest **1.86%** median FPS gain repeats the direction of the preceding
SIMD-only batch (299.2 to 301.6 FPS). Timing varied between runs; this is not a
locked minimum, nor a claim about every resolution or map. The 1% low is
1,000 divided by the mean of each run's slowest 36 frames, then the three-run
median. It improves **6.48%**. Median p99 frame time improves from **5.1082 to
4.6368 ms**. Worst observed frames across each family's runs: B **10.6577 ms**,
S **10.399 ms**, Q **9.2345 ms**. No measured frame exceeded 50 ms.

All nine runs match every per-frame player-health value and the AI collision,
route, sensing and planning fingerprints. Final health is 99158 throughout.
State-lookup hints are advisory; their own hit statistics are not part of the
behavior fingerprint. All scenes use the unchanged custom-map fixture with
SHA-256 `C64C1BC8F6C738945F41CFA9586DA7FC612254C4FC8CF9DF03FA7BA5E392BBA2`.

## Adopted change

`Mesh::sampleCreatureClip` decodes eight signed coordinates with SSE2 at once.
It preserves int32 subtraction before float conversion and the original
multiply/add/metre-conversion order. Remaining coordinates use the original
scalar arithmetic. It changes neither samples, playback rate, waist blending,
hand adjustment, lighting nor AI cadence. SSE2 is part of the existing x64
baseline; no AVX, newer GPU feature or new runtime dependency is required.

The regression compares every coordinate against scalar decoding for every
clip across all six cleanup roles at nine phases, including clamped endpoints
and interpolation. Existing layer-composition, hand, finger and aim checks
remain enabled. No animation bank or packed asset changed.

## Rejected experiments

- Direct indexed pose reads instead of triangle publication: 305.2 to 304.2
  FPS in its own six-run batch. Pixel references passed; average FPS did not.
- Eager attachment-group preparation plus invariant viewmodel rotations,
  combined with SIMD: 294.9 to 286.1 FPS. All GPU references passed, but the
  runtime regression disqualified it.
- Platform-only collision indexing and larger AI lookup hints, combined with
  SIMD: correctness checks passed, but Q did not beat the simpler S build in
  the final shared batch. Those additions were removed from shipping source.

Each comparison above belongs to its own interleaved batch. Absolute FPS
between batches must not be treated as a direct gain or regression.

## Binary and evidence

Winner SHA-256:
`75774028EE7F71F6F1C94775ABDDA08CE83A50723F4936013DEDF9249735B812`.
Size **27,236,352 bytes / 25.975 MiB**, **7 KiB smaller** than preview.8.
The 32 MiB ceiling retains **6,318,080 bytes / 6.025 MiB** of headroom.
The pack verifier confirms all **230 resources unchanged**, including exact
RGBA images and byte-identical models, animations and audio.

Evidence is under `.build/competitive-round10-20261018/`:

- `final/results.json`, `final/analyze.py`, nine CSV/text pairs and all three
  competing binaries identify the final comparison.
- `competition/`, `simd/competition/`, `batched/competition/` retain the earlier
  timed batches. `queries/source/` and the experiment snapshots retain rejected
  source for audit without leaving it in shipping code.
- `simd/profile/` contains a separate deep-instrumented diagnostic run. Its
  timings are not competition scores. It identifies actor preparation, AI
  collision work, viewmodel rendering and pose waiting as remaining costs.
- `release-checks/` records final executable regression results.

Each candidate was instrumented, trained on sustained Exclusion and broader
Ashfall gameplay with startup counters reset, then linked with speed-first PGO.
Training timings are not FPS results. Ordinary build configuration is restored
to PGO OFF and an isolated output directory after the competition.

## Final validation and promotion

All 25 checks passed on the exact winner: AI queries/decisions, nav graph,
guard pursuit/roles/patients/schedules/policy/actions/cover/danger, Engineer
drones, creature/civilian schedules, campaign maps, saves, stealth, Exclusion,
cleanup splitting, terrain queries, GPU actors, Vulkan, visibility,
presentation resize and full smoke. GPU actor references cover 56 views and
54 phase-cache comparisons with zero changed pixels between reference and
optimized paths.

Fresh 12-view Exclusion captures were taken twice with preview.8 and once with
the winner. The winner matches the second baseline capture exactly in every
view. Baseline captures themselves vary in views 1, 5 and 7 (132, 1 and 572
pixels); the winner introduces no differences beyond that existing variation.
The yard image was also inspected directly. Raw comparison is retained in
`captures/comparison.json`.

The root executable, release asset and winner use the SHA-256 above. Previous
root is retained as the local round-ten baseline for rollback. Rejected source
changes are removed; only the exact SIMD decoder and its scalar reference test
are adopted. The release does not claim 330 FPS or a fixed minimum frame rate.
