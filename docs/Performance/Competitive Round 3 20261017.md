---
tags: [performance, exclusion, validation]
date: 2026-10-17
---

# Competitive round 3: 180 FPS target

**The 180 FPS average target is met in the Exclusion benchmark.** The combined
candidate averages **184.0–188.4 FPS** across three runs, with a **186.5 FPS**
median versus the fresh published baseline's **160.6 FPS**. This is an average
throughput result; 1% lows remain **107.7 FPS**, rather than a 180 FPS floor.

Before this competition, the round-two winner was promoted to the root EXE and
published as [v0.7.2-preview.5 — Performance hotfix](https://github.com/MAJWCF1234/RawMetal/releases/tag/v0.7.2-preview.5).
Its source is on `main` at `fb954980be93cfce0f103810d0233ae891d3c4ab`.
The uploaded EXE digest matches the root copy; the superseded preview.4 release
was removed while retaining source history. The round-three combined winner is now promoted to the root copy and published
as [preview.6](https://github.com/MAJWCF1234/RawMetal/releases/tag/v0.7.2-preview.6).
Its tested binary is unchanged; preview.5 is superseded.

## Competition

RTX 5060 Ti, native hidden **640 × 360** window, Vulkan linear HDR / 4× MSAA,
live muted audio mixer. Each `--lab-performance` replay has 30 warmup plus
3,600 measured frames with fixed 60 Hz simulation. Deep per-query timers are
disabled. Timings are CPU wall times including presentation, not GPU timestamps
or proof of the same rate at every resolution/map.

| Three-run median | Published baseline | Prefix optimization | Combined winner |
| --- | ---: | ---: | ---: |
| Average FPS | 160.58 | 184.58 | **186.46** |
| 1% low FPS | 94.62 | 107.82 | **107.69** |
| Mean frame time, ms | 6.227 | 5.418 | **5.363** |
| P99 frame time, ms | 9.516 | 8.427 | **8.443** |
| P99 minus P1, ms | 5.780 | 5.289 | **5.276** |
| Simulation mean, ms | 2.266 | 1.621 | **1.560** |

Combined versus published: average FPS **+16.1%**, 1% lows **+13.8%**,
mean simulation **31.1% cheaper**, P99 **11.3% lower**, and frame-time spread
**8.7% narrower**. Prefix-only also clears the target. The combined candidate
has a small additional mean gain; its low-FPS metrics are essentially tied
with prefix-only, so that incremental gain should not be overstated.

| Run | Average FPS | 1% low FPS | P99 ms | P99 minus P1 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Canonical 1 | 157.95 | 77.94 | 10.512 | 6.695 | 21.066 |
| Canonical 2 | 160.58 | 96.84 | 9.379 | 5.501 | 17.843 |
| Canonical 3 | 162.77 | 94.62 | 9.516 | 5.780 | 19.698 |
| Prefix 1 | 181.50 | 107.82 | 8.659 | 5.462 | 13.143 |
| Prefix 2 | 184.58 | 106.19 | 8.427 | 5.289 | 16.633 |
| Prefix 3 | 185.05 | 111.94 | 8.417 | 5.202 | 10.362 |
| Combined 1 | 186.46 | 113.25 | 8.232 | 5.138 | 11.057 |
| Combined 2 | 184.02 | 106.14 | 8.467 | 5.276 | 15.509 |
| Combined 3 | 188.36 | 107.69 | 8.443 | 5.305 | 12.617 |

Order: canonical 1, prefix 1, combined 1, combined 2, prefix 2, canonical 2,
canonical 3, prefix 3, combined 3. Builds/validation ran outside timed runs.
All outliers are retained. Average FPS is reciprocal mean frame time; 1% low
is reciprocal mean of each run's slowest 36 frames. Summary metrics are medians
of per-run metrics. Compare within this batch, not against older host conditions.

**Hitches and the full gap remain unfinished.** The winner still has a 15.509 ms
worst frame. The reciprocal-P1 fast-end versus 1%-low FPS difference widens
**167.3 → 211.0 FPS**, because fast frames improve proportionally more. The
frame-time spread narrows, but these are distinct metrics; this is not proof
of uniform pacing or elimination of older, larger intermittent stalls.

## Exact internal changes

Previously `actorAiState` constructed decimal strings and concatenated a full
`npc/level/actor/timer/field` string on every call before hashing it. It now
retains the pure prefix hash in a bounded 128-slot thread-local table, checking
the full numeric identity on every hit. Misses use stack-based decimal formatting;
field bytes still feed the original unsigned 32-bit FNV sequence. The result
retains the same internal-key high bit and matches the legacy format exactly.

`Game::state` and `setState` now use 1,024 bounded thread-local index hints into
the existing sorted state vector. Every hint must satisfy the current sorted
neighbour bounds. A stale index after insert/erase/load or a different Game
falls back to the original `lower_bound`. Only an index is retained: every state
value is read live, with no saved pointers or cached timer/health values.
The existing uncached query mode uses the original search for reference tests.

The combined replay replaces about **20.34 million / 24.98 million** searches
with validated hints (~81%). Tables are per thread. The save format, state IDs,
state-vector ordering and update order remain unchanged. No AI decision rates,
physics samples, animation frames, geometry, shaders, lighting, render scale,
maps or sound assets were reduced. The optimization is in the normal game path,
not a special benchmark-only behavior.

## Output and behavior validation

- Legacy-format checks cover numeric limits, long/empty fields, embedded NULs,
  non-ASCII bytes, prefix collisions and **10,000 randomized key cases**.
- **30,000 state-index checks** match `lower_bound` through insert/erase,
  missing keys, hash-slot collisions, vector swaps and clears.
- Cached/uncached AI states and **full save data** match throughout three
  replays and restoration, including the new state-index path.
- All nine competition replays retain **14,029,424** collision queries /
  **1,790,181** hits; **23,829** route queries / **16,696** hits; sensing
  **11,831** runs; planning **9,493** runs; final health **99,158**.
- **24 final regression/profile commands passed**, including full
  `--smoke-test --vulkan`, terrain/navigation, decision clock, guards and their
  schedules/danger/patients/cover/roles/tables, Engineer drones, Exclusion,
  stealth, streaming, saves, civilian/creature schedules, lifts and resize.
- The **28-view** visibility/cache suite has zero changed pixels; all **56**
  GPU actor comparisons pass the existing geometry/material/lighting thresholds.
- Two additional capture commands compare the published EXE and winner across
  **12 authored Exclusion/Receiving views: every pixel is identical**.
- Asset verification: **all 230 resources match source exactly**.

These are tested replay/capture guarantees, not an exhaustive bug-free claim.
No newer GPU feature requirement was added; 2015 hardware was not physically tested.

## Artifacts

`.build/competitive-round3-20261017/` contains `canonical.exe`, `prefix.exe`,
`combined.exe`, all raw timing CSVs, the analysis script, build/check logs,
captures and `published-candidate-pixels.json`. Default build output was restored
to the root after the experiment without replacing its canonical EXE.

| EXE | Bytes | SHA-256 |
| --- | ---: | --- |
| Published/root | 26,896,896 | `35c12d64d450e86271eba3365fe4eb24ec4c9fe1c85a354046299d33dfc2534d` |
| Prefix | 26,903,040 | `58984867963906688241a1b6e3943d9fea72c8fc146000f30a9b98ba92bc3300` |
| Combined winner | 26,939,904 | `1d4c6fd9da0d471d9fb1924ef0230606ec22cb493b443577f4782b92a27536e9` |

Winner: **25.69 MiB**, only **42 KiB** above the canonical EXE, with **6.31 MiB**
remaining below the unchanged **32 MiB** ceiling. Release builds passed with the
existing MSVC warnings; no compiler flags or asset encodings changed this round.
