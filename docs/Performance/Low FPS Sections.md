---
tags: [performance, ai, exclusion]
date: 2026-10-16
---

# Low FPS sections

Exclusion's repeatable slow sections combine route/collision sweeps in the AI
with CPU preparation and submission of animated actor geometry. The implemented
changes retain exact route probes and store actor triangles once for Vulkan
instancing. Route selection, movement, weapons, animation and sensing cadence
are preserved. Large isolated render/presentation stalls remain under review.

## Diagnosis

`--lab-profile` runs the full 3,600-frame replay with deep scopes enabled.
Its extra timer overhead makes its FPS unsuitable for the release comparison.
Baseline profile, mean versus mean within the slowest 36 total frames:

| CPU wall scope | Overall ms | Slowest 1% ms |
| --- | ---: | ---: |
| Simulation | 3.244 | 5.947 |
| Walk segment checks | 1.586 | 3.299 |
| Fits queries | 1.280 | 2.608 |
| Graph path planning | 0.414 | 1.000 |
| Scene preparation | 3.751 | 5.963 |
| Actors | 1.877 | 2.840 |
| Actor mesh / attachment / triangle emission | 1.364 | 2.023 |
| Skin sampling | 0.133 | 0.218 |
| Viewmodel worker wait | 0.204 | 0.066 |

Scopes nest; do not add them. Skin sampling and worker waits are smaller than
the full actor mesh path. Static geometry builds and atlas upload counters stay
flat after warmup, including the slow windows; repeated static baking or texture
upload is not the sustained yard problem. These are wall measurements, not GPU
timestamps or proof that every individual hitch has the same cause.

An opaque-wall actor-occlusion trial passed all 28 visibility views with zero
changed pixels, but culled **0 of 9,787** tested actors in this yard. Mean frame
time was 8.470 ms baseline versus 8.531 ms trial. The trial was removed. Its
artifacts remain under `visibility/`, `baseline-1/` and `candidate-1/` for audit.

## Retained change

`WalkQueryCache` wraps only planning sweeps inside `guardGraphWaypoint`:

- 2,048 direct-mapped entries, with exact float-bit keys for both endpoints,
  starting feet and hull height. Full keys are compared after hashing.
- Returns the exact destination support height on success. Failed probes retain
  the caller's output unchanged. Nonfinite inputs and seam-adjacent sweeps bypass
  the memo.
- Before each route request, compares an exact mutable collision snapshot:
  actual tiles, door geometry/opening position, cargo and cab lift elevation,
  cab phase/exit-timer threshold, terminal elevations, platform positions and
  geometry residency. Ordinary NPCs are not solids in these World queries.
- Authored-definition owner lifetime and World identity scope the memo. Changed
  state clears it; there is no timeout that permits stale doors or platforms.
  Snapshot equality itself does not use a hash.
- Actual movement still checks collision every simulation tick. Seam permissions
  and neighbouring chunks remain live. BFS order, path validation order, planning
  budgets and threat/task scheduling are unchanged.
- `--uncached-ai-queries` also disables this retained cache. No save format or
  additional GPU/CPU feature requirement is introduced. Entry storage is 64 KiB,
  with a small exact snapshot alongside it.

The yard makes 23,829 eligible planning probes, reusing 16,696 (**70.1%**).
Collision queries fall **20,573,027 → 17,361,806** (**15.6%**). Every replay still
has 21,600 guard updates, 11,831 full sensing runs and 9,493 planning deadlines,
and finishes at health 99,158. This change removes repeated collision work;
it does not reduce the number of guard decisions.

## Actor triangle stream

The follow-up timer now samples direct GPU actor submission as well as the CPU
reference. Before this change it missed that GPU path. A diagnostic replay
estimated about **1.18 ms of a 1.61 ms actor mesh scope** inside triangle
submission. These sampled values guide diagnosis, not the FPS comparison.

A three-vertex append trial gave only a modest profile signal and did not
establish an improvement in lows. The retained implementation instead uses one
Vulkan instance for each complete triangle:

- Three full-precision positions, corner light values and UV pairs, with one
  shared face/material record: **152 bytes instead of 312** per triangle, a
  **51.3% reduction** in this dynamic actor stream. No vertex quantization.
- The vertex shader selects the same three corners using `gl_VertexIndex` and
  uses the existing actor lighting/aim data. Geometry, animation, UVs and
  material values are preserved.
- Each frame slot owns a separate actor buffer. Its fence protects buffer reuse.
  Stable material ordering and the existing depth-clear boundaries remain;
  ordinary, cached and instanced draws select their own buffer and pipeline.
  Transparent draws retain their deferred ordering.
- Uses Vulkan 1.0 instanced vertex inputs and 11 attributes, within the core
  minimum of 16. No compute, geometry shader or new device extension is needed.
  The CPU actor reference remains available.

## Final combined comparison

Baseline SHA-256:
`8d017a826d2945dcb170ae834313c39e0d707dbd7a112f0232ac4476a24f43c2`.
Candidate SHA-256:
`941f2c81480fd4ede734c28409dcf7264fa1f9cb7d50fe17f11941c46f11c385`.
This candidate is the root executable, not a discarded trial.

Same map and settings, RTX 5060 Ti, Vulkan HDR / 4x MSAA, native hidden
primary-monitor window, 30 warmup then 3,600 measured frames at fixed 60 Hz,
live muted mixer, deep timers off. The 640x360 report header describes the
logical HUD; the window covers the primary monitor (current desktop mode
1920x1080). Times are CPU wall, including presentation, not GPU timestamps.
No builds or tests ran alongside timing. Order: baseline 1, candidate 1,
candidate 2, baseline 2, baseline 3, candidate 3. All frames and outliers retained.

| Run | Mean ms | Average FPS | 1% low FPS | Fast-end FPS | P99 ms | Worst ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Baseline 1 | 9.647 | 103.65 | 43.15 | 178.95 | 17.904 | 62.831 |
| Candidate 1 | 8.330 | 120.05 | 62.52 | 190.98 | 14.329 | 22.026 |
| Baseline 2 | 10.263 | 97.44 | 42.22 | 171.54 | 19.539 | 51.313 |
| Candidate 2 | 8.823 | 113.34 | 58.79 | 178.48 | 14.799 | 24.973 |
| Baseline 3 | 9.686 | 103.25 | 52.41 | 179.95 | 16.681 | 33.129 |
| Candidate 3 | 8.783 | 113.85 | 59.28 | 186.41 | 13.864 | 36.682 |

Average FPS = 1000 / mean frame time. 1% low = 1000 / mean of slowest 36 frames.
Fast end = 1000 / P1 frame time.

| Median across three runs | Baseline | Candidate |
| --- | ---: | ---: |
| Average FPS | 103.246 | 113.854 |
| 1% low FPS | 43.150 | 59.282 |
| Fast-end FPS | 178.955 | 186.407 |
| Mean simulation ms | 3.096 | 2.765 |
| Mean scene ms | 4.033 | 3.689 |
| Mean submit ms | 0.822 | 0.561 |
| Simulation within slowest 1% ms | 7.387 | 5.869 |
| Scene within slowest 1% ms | 8.966 | 6.570 |
| Submit within slowest 1% ms | 1.878 | 0.960 |
| P99 frame time ms | 17.904 | 14.329 |
| P99 minus P1 frame time ms | 12.316 | 9.092 |
| Fast-end FPS minus 1%-low FPS | 135.805 | 127.125 |

Median **1% lows rise 37.4%**, average FPS **10.3%**. P99-minus-P1 frame-time
spread narrows **26.2%**; the absolute FPS gap narrows **6.4%**. Every candidate's
1% low exceeds its corresponding baseline in this batch. Submission wall time
falls **31.8%** overall and **48.9%** within the slowest 1% of frames.

The result is an improvement in this workload, not a guaranteed frame-rate
floor. Candidate 3 still has a **36.682 ms** hitch, including **22.937 ms** scene
preparation. The earlier route-only batch also recorded an **84.698 ms** frame
with **33.453 ms** in presentation and substantial renderer time outside the
existing scene/submit/present scopes. GPU begin/fence handling and HUD work are
outside those scopes. The root cause of those isolated stalls is not fully
established; do not attribute every hitch to AI or claim hitch elimination.

## Earlier route-only trials

These preceded the actor stream change. They explain why the route cache alone
was not accepted as closing the low/high gap:

| Three-run median batch | Average FPS before/after | 1% low FPS before/after |
| --- | ---: | ---: |
| Initial route trial | 97.23 / 106.89 | 43.95 / 52.23 |
| Final route-only retest | 109.09 / 118.19 | 64.38 / 63.76 |

The retest made AI cheaper but did not reproduce the initial low-FPS gain.
All raw runs remain in `route-*` and `final-*`, with `route-comparison.json` and
`final-comparison.json`. Absolute FPS varies with host conditions; comparisons
use fresh baselines within each batch, not the best historical number.

## Validation

- Release: **230 embedded resources verified exact**, including models,
  animation, textures and audio. No animation bank or visual asset changed.
- Navigation: **39 checks**, including door open/close, moving cargo/platforms,
  destroyed cover, hull height, exact nonzero support output, world replacement,
  live seams, lift phase/elevation and the crashed lift exit threshold.
- Exact-query regression: Exclusion, Reactor and Receiving cached/uncached
  replays match AI state every tick and full save bytes at 30-frame intervals,
  including save/restore.
- All **19 targeted suites** pass: nav graph, query/decision clocks, Guard AI,
  Friendly AI, schedule, danger, patient, cover, roles, table, Engineer drones,
  Exclusion, stealth, streaming, saves, civilian/creature schedules and lift.
  Results: `final-checks/results.txt`. Game code is unchanged after these checks.
- **56 GPU actor reference views**, **429,728 triangles**: geometry, materials,
  lighting, treatment, support and aiming checks pass. Reference differences
  stay within the existing strict tolerance; inspected the six-role contact
  sheet at `instanced-checks/role-reference.png`.
- **28 visibility views**: every view has **0 changed pixels** versus unculled
  reference. Vulkan material/depth/clipping test passes without fallback.
- Final full `--smoke-test --vulkan`: **exit 0**, 113.45 seconds. Native
  presentation-resize test: **exit 0**. Results: `instanced-smoke/result.txt`.

Root `RawMetal.exe`: **26,784,256 bytes (25.54 MiB)**, leaving **6,770,176 bytes
(6.46 MiB)** below 32 MiB. This whole round adds 96,256 bytes. Shader target and
hardware feature baseline remain Vulkan 1.0; physical 2015-machine testing is
not established by this desktop run. No commit or push performed.

Evidence: `.build/low-fps-20261016/`. Final six runs: `combined-baseline-*` and
`combined-candidate-*`; complete metrics: `combined-comparison.json`;
reproduction: `py -3 .build/low-fps-20261016/analyze.py combined`.

Remaining targets: CPU actor preparation and investigation of isolated scene /
presentation stalls with finer timing. The implementation does not establish
500 FPS capability or eliminate the low/high gap.
