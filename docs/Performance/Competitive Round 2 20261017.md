---
tags: [performance, exclusion, validation]
date: 2026-10-17
---

# Competitive round 2: support and door broad phase

The first-round winner was promoted and published before this competition:
[v0.7.2-preview.4 - Performance hotfix](https://github.com/MAJWCF1234/RawMetal/releases/tag/v0.7.2-preview.4).
`main` release commit: `67dfb1ed2ac7daca83753f72759567808d8bc1e5`.
The GitHub asset's SHA-256 and size match the canonical root EXE. The superseded
preview.3 GitHub release was removed; its source tag/history were retained.

The root EXE remains that published first-round winner. Round-two development
and its next candidate are kept separately on `codex/performance-round2` and
in `.build/competitive-round2-20261017/`; this candidate is not the published hotfix.

## Result

Fresh three-run medians on the same Exclusion replay favor the broad-phase
candidate for lows, with a small average gain:

| Metric | Published canonical | Body cache, allocated on demand | Broad phase |
| --- | ---: | ---: | ---: |
| Average FPS | 161.74 | 163.08 | **163.51** |
| 1% low FPS | 95.41 | 102.25 | **105.14** |
| Mean frame time, ms | 6.183 | 6.132 | **6.116** |
| P99 frame time, ms | 9.471 | 9.219 | **8.997** |
| P99 minus P1, ms | 5.614 | 5.470 | **5.163** |
| Simulation mean, ms | 2.261 | 2.237 | **2.225** |
| EXE bytes | 26,896,896 | 26,898,432 | **26,896,896** |

Broad phase versus canonical: average FPS **+1.1%**, 1% lows **+10.2%**,
P99 **5.0% lower**, frame-time spread **8.0% narrower**. The average gain is
small relative to run variation; the stronger result is the low-FPS improvement.
All three broad-phase 1% lows exceed all three canonical 1% lows in this batch.
The fast-end-versus-low FPS gap also narrows **165.5 → 159.2 FPS** (3.8%):
the fast end is the median reciprocal P1 frame time, compared with median 1% low.
This small sample does not establish performance on every map or machine.

| Run | Average FPS | 1% low FPS | P99 ms | P99 minus P1 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Canonical 1 | 161.74 | 95.41 | 9.603 | 5.770 | 13.601 |
| Canonical 2 | 162.12 | 95.36 | 9.412 | 5.614 | 13.366 |
| Canonical 3 | 157.51 | 99.74 | 9.471 | 5.455 | 11.502 |
| Body cache 1 | 163.08 | 103.11 | 9.084 | 5.377 | 11.604 |
| Body cache 2 | 161.24 | 102.25 | 9.294 | 5.545 | 11.268 |
| Body cache 3 | 163.33 | 101.11 | 9.219 | 5.470 | 14.285 |
| Broad phase 1 | 163.51 | 105.14 | 8.997 | 5.163 | 11.359 |
| Broad phase 2 | 167.58 | 108.02 | 8.762 | 5.061 | 10.871 |
| Broad phase 3 | 161.76 | 102.05 | 9.177 | 5.394 | 13.430 |

No outliers were removed. Larger hitches from previous batches remain
unexplained; a 13.430 ms worst frame here is not proof they have been eliminated.
Do not combine this batch with the earlier 140-to-164 result: host conditions
changed, and the published winner is now the baseline.

## Candidates and method

The body-fit cache reused exact float-bit coordinates, feet, height and immutable
terrain revision IDs, while retaining live dynamic collision checks. A direct
8,192-entry TLS version grew the EXE to **27,160,064 bytes** and did not improve
the initial comparison (161.54 FPS). The on-demand version avoids that size
growth and reached the table above, but needs 256 KiB per participating thread
and did not beat broad phase. Neither body-cache version is kept in the next
candidate's source; its patch and raw results remain in the local experiment folder.

The retained broad-phase changes are small and use existing spatial indices:

- Door collision rejects points outside the exact XY footprint before asking
  for terrain height. Swinging-door geometry and live open amount remain exact.
- Support queries visit the existing ordered prop and terminal cell candidates,
  then perform the original height and overlap tests. Moving lift-control
  terminals retain the existing all-cell coverage and live height checks.

No scheduling rates, movement samples, animation banks, shaders, lighting or
render resolution were reduced. No additional runtime cache enters this candidate.

Hardware/settings: RTX 5060 Ti, native hidden **640 x 360** window, Vulkan linear
HDR / 4x MSAA, live muted mixer. Every `--lab-performance` run has 30 warmup plus
3,600 measured frames of a fixed 60 Hz combat replay. Deep per-query profiling
is disabled; timings are CPU wall times including presentation, not GPU timestamps.
Average FPS is reciprocal mean frame time; 1% low is reciprocal mean of the
slowest 36 frames. Results are medians of per-run metrics, not best runs.

Completed comparison order: canonical 1, direct-cache screen, allocated-cache 1,
broad phase 1, broad phase 2, allocated-cache 2, canonical 2, canonical 3,
allocated-cache 3, broad phase 3. A separate direct-cache screening replay is
also retained. Builds and validation ran outside timed comparisons. Broad phase
required clearing generated incremental-LTCG artifacts after MSVC C1001; a full
Release link then succeeded with the same compiler flags. This compares actual
executables; isolated attribution of every gain to source versus code layout
has not been established.

## Validation

Both cache prototypes and broad phase passed terrain and cached/uncached AI/save
reference checks before benchmarking. The retained broad-phase candidate passed
**21 regression commands** including full `--smoke-test --vulkan`, navigation,
guards, friendly actors, schedules/danger/patients/cover/roles, Engineer drones,
Exclusion, stealth, streaming, saves, creatures/civilians and lifts.

The 28-view visibility/cache reference suite reports zero changed pixels;
all 56 CPU/GPU actor comparisons pass. Two additional capture commands generated
the same 12 Exclusion encounter views with the published EXE and next candidate:
**every corresponding pixel is identical**, including the Receiving breach views.
`published-candidate-pixels.json` records the comparison.

All nine compared combat replays retain identical gameplay fingerprints:
14,029,424 collision queries / 1,790,181 hits; 23,829 route queries / 16,696 hits;
sensing 11,831 runs, planning 9,493 runs; final health 99,158. The asset packer
verified all 230 resources exactly. The default build output was restored to the
repository root after the experiment; the canonical root binary was never replaced.

Raw CSVs, logs, binary copies, the body-cache patch and `analyze.py` live in
`.build/competitive-round2-20261017/`. The root canonical hash remains:
`17059affba29ab4e694d7e0177cdee69c35829c86510d1ec3985813c04277c0a`.

The tested broad-phase executable is `broadphase.exe`, SHA-256:
`35c12d64d450e86271eba3365fe4eb24ec4c9fe1c85a354046299d33dfc2534d`.
It is **25.65 MiB**, with **6.35 MiB** under the unchanged 32 MiB ceiling.
