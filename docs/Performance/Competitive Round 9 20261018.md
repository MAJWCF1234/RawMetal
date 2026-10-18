---
tags: [performance, exclusion, validation, bugfix]
date: 2026-10-18
---

# Round 9: 304 FPS with animation and retreat fixes

The final candidate wins at **304.3 FPS**. **Promoted 2026-10-18** to
`N:/rawmetal/RawMetal.exe` as **v0.7.2-preview.8**. Fresh interleaved promote
check vs the prior root (live-walk-memo / round seven): median **242.1 → 306.3 FPS**.
All pre-existing workspace edits are preserved in source.

## Final competition

Ryzen 7 5800X / RTX 5060 Ti, the same native hidden **640x360** Vulkan HDR /
4x MSAA lab, fixed 60 Hz simulation, 30 warmup + 3,600 measured frames per run.
Live muted device mixer, deep instrumentation disabled. No builds or regression
tests ran alongside the timed batch. Order: B1, C1, C2, B2, B3, C3.

The baseline here is the already corrected, speed-first PGO build that won the
preceding nine-run competition at 294.3 FPS versus the round-eight candidate's
264.2 FPS in that batch. The final micro-pass compares that build with identical
clip-sample reuse and row rectangle fills; it is not a comparison against frozen
animation or reduced behavior.

| Run | Baseline FPS | Candidate FPS |
| --- | ---: | ---: |
| 1 | 295.05 | 303.93 |
| 2 | 299.23 | 306.35 |
| 3 | 296.58 | 304.32 |

| Three-run median | Baseline | Candidate |
| --- | ---: | ---: |
| Mean frame time | 3.372 ms | **3.286 ms** |
| Average FPS | 296.6 | **304.3** |
| 1% low FPS | 182.1 | **195.1** |
| P99 frame time | 5.068 ms | 4.839 ms |
| Scene mean | 1.783 ms | 1.762 ms |

Final pass: **+2.61% average FPS**. Each of the three candidate runs averaged
over 300 FPS. A run's 1% low is 1000 divided by the mean of its slowest 36 frame
times, then medians are taken across runs. The worst individual frame across
the three runs was 8.053 ms for candidate versus 9.263 ms for baseline.
This meets the sustained lab average target; it is not a locked 300 FPS claim
for every frame, resolution or campaign scene.

All 3,600 per-frame health values and aggregate AI/route/sensing/planning
fingerprints match across all six runs. Final health is 99,158 in every run.
Timings are CPU wall measurements including presentation, not GPU timestamps.

| Winning artifact | Value |
| --- | --- |
| Executable | `.build/competitive-round9-20261018/winner.exe` → `RawMetal.exe` |
| SHA256 | `E708B9F1E44E015B24CF5DB1D4C14FAE3F2A25CD0ADD6ED919450973F0AA5FD2` |
| Size | 27,243,520 bytes (**25.98 MiB**) |
| Headroom below 32 MiB | 6,310,912 bytes (**6.02 MiB**) |
| Exclusion fixture SHA256 | `C64C1BC8F6C738945F41CFA9586DA7FC612254C4FC8CF9DF03FA7BA5E392BBA2` |
| Release | [v0.7.2-preview.8](https://github.com/MAJWCF1234/RawMetal/releases/tag/v0.7.2-preview.8) |

### Fresh promote check (vs prior root)

Interleaved B1/C1/C2/B2/B3/C3 against round-seven root `EDAF720D…`:

| Three-run median | Prior root | Round 9 winner |
| --- | ---: | ---: |
| Mean frame time | 4.130 ms | **3.265 ms** |
| Average FPS | 242.1 | **306.3** |

Evidence: `.build/competitive-round9-20261018/promote-check/`.

Raw six-run reports/CSVs and computed `results.json` are under
`.build/competitive-round9-20261018/refine/final/`. The prior nine-run policy
competition is under `.build/competitive-round9-20261018/final/`.

## Bugs fixed

- **Receiving evacuation:** hypothetical seam starts could be inside the pressure
  door's side wall. A sweep that correctly skipped the already occupied start
  for live movement consequently accepted an impossible planner endpoint. Portal
  selection now checks the complete candidate footprint first. The original
  failure trace stops the Engineer at approximately `(14.637, 23.179)` while
  targeting `(14.637, 24.35)`, outside the doorway. The corrected campaign test
  proves five deaths, one living Engineer physically in Receiving 03, and the
  sealed retreat surviving save/load. No teleport or invulnerability was added.
- **Frozen idle/crouch/death geometry:** every animation phase now participates
  in retained actor geometry identity. Dormant-floor clipping and emission scale
  are also included. Moving-phase cached renders are compared with uncached
  renders across all six roles.
- **Nested world route queries:** active walk memos now require the exact queried
  World owner. A nested query into a second map cannot inherit an outer map's
  clear/blocked answer. A regression uses an open room and a walled room with
  identical query coordinates and checks restoration of the outer scope.

## Work removed while preserving output

- NPC key memoization stores pure IDs and copies complete short field names.
  It checks prefix and full bytes on hits, including mutated text at the same
  address. Long fields retain the original hash path. Existing save IDs remain
  identical; no live Game state is retained. Sorted-state index hints are larger
  and still validate both current neighbours on every use.
- Zero-weight torso/head animation work is skipped. Each role is checked against
  the original full float composition at every tested vertex, phase and aim.
- Identical upper/lower clip and phase combinations sample once, retaining the
  same float composition and joint metadata. Reference coverage includes idle,
  walk, crouch, fire and aim combinations.
- First-person arms retain the library's bone evaluation, blend offsets and
  position skinning functions, while skipping normal/topology generation that
  the renderer does not consume. Exact reference checks cover six firearm poses,
  fifteen idle/punch poses, UVs and hand-bone positions.
- Actor bodies reserve the existing GPU triangle stream once and write directly
  into it. There is no extra triangle staging copy. Triangle/material order,
  clipping, full-float fields and the 92-byte shader ABI are preserved. The old
  scalar path remains the test reference.
- Retained geometry is admitted only after its exact identity repeats. Moving
  poses still render every frame; their geometry is no longer copied into a
  never-hit cache on every animation frame. Existing allocations are reused.
  Stable poses become reusable after two emissions.
- UI rectangles clip once and fill contiguous rows. A scalar reference checks
  ten clipping, empty-dimension and large-extent cases. Visibility and panel
  reference images remain unchanged.

## Validation already completed

- AI query/legacy key-ID reference tests, including 1,000 same-address text
  mutations and the existing arbitrary-byte/limit/collision cases: PASS.
- Navigation graph tests including nested-world regression: PASS.
- Cleanup split/aim tests, including full-composition references: PASS.
- Campaign map test, including real Receiving evacuation and save/load: PASS.
- GPU actor suite: **56 scalar-versus-stream images, zero changed pixels**,
  plus **54 moving-phase cache comparisons**, all PASS. Receiving views now
  complete instead of aborting the suite.
- Vulkan materials, clipping, arm skinning and parallel/serial pose references:
  PASS on the combined stream/position-only build.
- Full visibility suite and the row-fill scalar reference: PASS on the refined
  source before PGO. Final optimized checks are recorded under `release-checks/`.
- Packaged resources: all **230 unchanged**. No animation banks, render-quality
  reductions, AI cadence reductions or CPU ISA requirements were introduced.

## Build and evidence

The round-eight PGO candidate is saved as
`.build/competitive-round9-20261018/baseline.exe`. Experiments and raw reports
are isolated under that directory; `before/` preserves touched source snapshots.
`fix-checks/`, `stream-checks/` and `admission-checks/` contain the corresponding
reference and regression reports.

PGO uses the same sources for instrumentation and optimization. Training covers
the sustained Exclusion lab and the broader campaign/terrain cases. The
Microsoft `pgosweep /onlyzero` operation clears startup counters after frames
begin, so the training profile focuses on gameplay. Ordinary and speed-first
PGO are separate candidate configurations, both subject to the 32 MiB verifier.
Ordinary rebuild configuration will be reset to PGO OFF after the competition.

References: [PGO counter reset](https://learn.microsoft.com/en-us/cpp/build/pgosweep?view=msvc-170),
[PGO optimization options](https://learn.microsoft.com/en-us/cpp/build/reference/useprofile?view=msvc-170).
