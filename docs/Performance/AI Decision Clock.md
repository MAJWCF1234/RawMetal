---
tags: [performance, ai, exclusion]
date: 2026-10-16
---

# AI decision clock

Guard target selection and tactical repositioning now run on staggered
simulation-time deadlines. Movement, collision, animation, task execution,
recognition, weapon timing and support work still advance every simulation tick.
This replaces the previous calm-patrol shortcut that skipped an entire actor
update and banked movement time for the next update.

## Scheduling and reaction safety

- Full threat selection: nominally 10 Hz during patrol and 20 Hz otherwise.
  Tracking the current target and checking its actual LOS still run every tick;
  occlusion does not reveal hidden movement or permit firing through cover.
- Tactical repositioning and lateral room probes: nominally 10 Hz. The actor
  follows the retained goal between decisions, with live movement collision.
- Damage, relevant nearby sounds, close hostiles, changed target/defense state,
  blocked movement, reached goals and lost reservations cause early work.
  Player and creature contact both have explicit immediate-reaction tests.
- Periodic deadlines retain their phase after an urgent wake. Actors do not all
  permanently synchronize after a loud event. Pause does not advance AI time.
- Clock phase, deferred tactical time and retained goals are saved. Old saves
  without these fields initialize immediately. Chunk migration clears retained
  plans; local collision/stall checks react to changed geometry each tick.
  No independent wall clock or OS worker thread was added;
  this is the scheduling stage, with all live state mutation on the simulation
  thread. Support planners and creature brains retain their existing cadence.
- `--full-rate-ai-decisions` keeps the full-rate reference mode, including after
  loading a save. It does not re-enable the old whole-actor patrol skipping.
- Impossible distant melee checks now reject distance/height before LOS. The
  same predicates still protect every contact attack.

## Comparison method

Saved the executable present at the start of the task as `baseline.exe`:
SHA-256 `f3c83ac21da9719f4286e1f9d131c601902fefda8d7aa9f253b7d318dcc9ea22`.
Compared it against the new executable with `--lab-performance` on the RTX 5060
Ti: Vulkan HDR, 4x MSAA, native hidden window presentation, 30 warmup plus 3,600
measured frames, fixed 60 Hz replay, live muted mixer, deep instrumentation off.
Identical authored map in each isolated working directory. No build or test
process ran during the timed replays. Times are CPU wall measurements, not GPU
timestamps. The printed 640x360 is the logical HUD size, not native resolution.

Order: baseline 1, candidate 1, candidate 2, baseline 2, baseline 3, candidate 3.
The earlier best 119 FPS result was not used as this comparison's baseline;
fresh runs capture the machine conditions during this task.

| Run | Mean ms | Average FPS | 1% low FPS | Fast end FPS | P99 ms | Worst ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Baseline 1 | 10.205 | 97.99 | 37.62 | 167.44 | 21.643 | 42.013 |
| Candidate 1 | 8.454 | 118.29 | 73.18 | 188.89 | 12.679 | 17.933 |
| Candidate 2 | 8.956 | 111.66 | 52.14 | 184.84 | 14.639 | 67.719 |
| Baseline 2 | 9.234 | 108.30 | 61.08 | 173.92 | 14.856 | 24.219 |
| Baseline 3 | 9.462 | 105.69 | 60.43 | 173.74 | 15.449 | 19.449 |
| Candidate 3 | 8.853 | 112.96 | 62.95 | 182.60 | 13.966 | 21.295 |

Average FPS is 1000 / mean frame time. The 1% low is 1000 / mean of the
slowest 36 frame times. Fast end is 1000 / P1 frame time. No frames were removed.

| Median across three runs | Baseline | Candidate |
| --- | ---: | ---: |
| Average FPS | 105.69 | 112.96 |
| 1% low FPS | 60.43 | 62.95 |
| Fast end FPS | 173.74 | 184.84 |
| Mean simulation ms | 3.415 | 2.839 |
| P95 frame time ms | 13.248 | 12.126 |
| P99 frame time ms | 15.449 | 13.966 |
| P99 minus P1 frame time ms | 9.693 | 8.490 |
| Median fast-end FPS minus median 1%-low FPS | 113.31 | 121.89 |

Mean simulation time is **16.88% lower**. Total mean frame time is **6.44% lower**,
average FPS **6.88% higher**, and 1% low FPS **4.17% higher**. The frame-time
spread narrows **12.42%**, while the absolute FPS gap widens slightly: the fast
end gains more than the slow end. This improves ordinary frame pacing but does
not solve hitches. Candidate 2's 67.719 ms outlier contains 53.484 ms of scene
work and 9.571 ms of simulation; its cause is not isolated. It is retained.

All six guards still receive 3,600 updates each (21,600 total). Each
candidate has 11,831 full selection passes (6,948 urgent wakes) and 9,493 planning
deadlines (7,067 urgent wakes); close combat raises the effective rates above
their nominal intervals. Eligible collision queries fall from 29,997,025 to
20,573,027. Baselines finish at health 99162; candidates at 99158. Trajectories
and combat outcomes can differ under scheduling; this is not a claim of
byte-identical behavior between the two versions.

Raw CSVs/reports, both executables and `analyze.py` are in
`.build/ai-decision-clock-20261016/`. Calculated metrics: `comparison.json`.

## Validation

- Release build: all 230 embedded resources verified exact.
- `--ai-decision-test`: deadline rates at 30/60/120/240 Hz, urgent phase
  preservation, continuous patrol movement, mechanical timing between sensing
  deadlines, immediate damage/player/creature reactions and save continuation.
- Guard AI (including combat, tactics, policy, defense, pursuit, actions and
  grenades), Friendly AI, schedule, danger, patient, cover, roles, table,
  nav graph, Engineer drones, Exclusion, stealth, streaming, saves, exact-query
  replay, civilian and creature schedule suites: all exit 0. Final logs:
  `contact-checks/results.txt`. Terrain reference test also exits 0; World
  geometry/rendering code was unchanged in this pass.
- Full `--smoke-test --vulkan`: exit 0, 101.16 seconds (`smoke/result.txt`).
- `git diff --check`: pass; existing line-ending notice only.

Root `RawMetal.exe`: **26,688,000 bytes** (25.45 MiB), leaving **6,866,432 bytes**
below 32 MiB. This round adds 22,016 bytes. No new GPU feature or CPU instruction
requirement, and no animation/visual asset changes. 2015-era requirements are
unchanged; a physical 2015-machine test remains outstanding.
SHA-256 `8d017a826d2945dcb170ae834313c39e0d707dbd7a112f0232ac4476a24f43c2`.
No commit or push performed.
