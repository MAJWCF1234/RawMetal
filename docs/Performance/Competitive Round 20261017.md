---
tags: [performance, exclusion, validation]
date: 2026-10-17
---

# Competitive round: retained terrain and HUD work

The retained-panel candidate wins the fresh Exclusion replay comparison:
**140.2 → 163.6 FPS**, with **1% lows 92.2 → 103.6 FPS**.
The winner is installed as the root `RawMetal.exe`. Existing worktree edits,
including the walk-start skip and actor GPU preparation, were preserved.

## Final competition

RTX 5060 Ti; native hidden window at **640 × 360**; Vulkan linear HDR / 4× MSAA.
Each run uses `--lab-performance`: 30 warmup frames, then 3,600 measured frames
of the same 60-second Exclusion combat replay, with the live muted audio mixer.
Deep per-query instrumentation is disabled. These are **CPU wall frame timings**
including native presentation, not GPU timestamps or a manual playtest at other
resolutions. Builds and regression tests ran outside the timed competitions.

Order: original, C, C, original, original, C. No timed-run outliers were removed.

| Build / run | Average FPS | 1% low FPS | P99 ms | P99 − P1 ms | Maximum ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| Original 1 | 139.69 | 91.95 | 10.223 | 5.224 | 13.107 |
| Original 2 | 140.93 | 93.42 | 10.108 | 5.238 | 12.917 |
| Original 3 | 140.21 | 92.15 | 10.100 | 5.233 | 12.732 |
| C 1 | 162.89 | 104.51 | 9.039 | 5.065 | 11.652 |
| C 2 | 163.57 | 103.56 | 9.066 | 5.235 | 13.963 |
| C 3 | 166.41 | 103.10 | 9.033 | 5.321 | 11.486 |

Median per-run results: average FPS **+16.7%**, 1% lows **+12.4%**, mean frame
time **7.132 → 6.114 ms**, P99 **10.108 → 9.039 ms**. A 1% low is the reciprocal
of the mean of each run's slowest 36 frames; average FPS is the reciprocal of
mean frame time. Percentiles use sorted frame indices.

**Consistency remains unfinished.** Median P99-minus-P1 is effectively flat:
**5.233 → 5.235 ms**. The FPS difference between the fast end (reciprocal P1)
and 1% lows actually widens **113 → 157 FPS**, because the fast end improves
more. The candidate's worst retained frame, **13.963 ms**, also exceeds this
batch's original maximum, **13.107 ms**. This is a throughput and low-FPS win,
not proof of hitch elimination or the 500 FPS target.

## Earlier elimination round

Separate three-run medians against the same original executable:

| Variant | Average FPS | 1% low FPS | P99 ms |
| --- | ---: | ---: | ---: |
| Original | 141.37 | 93.30 | 10.129 |
| A: exact terrain reuse | 142.28 | 90.97 | 10.165 |
| B: A + retained minimap occupancy | 142.30 | 94.87 | 10.007 |

A and B do not establish a convincing standalone throughput win. The panel
work was then added to B and compared in a fresh original/C round above.
Do not compare these numbers with older dated batches as if host conditions
were identical. One preliminary original replay overlapped the diagnostic
profile; it was retained separately and replaced before the valid three-run
summary. This exclusion was due to overlap, not its timing result.

## Changes kept

- Terrain surface queries reuse **exact float-bit** inputs and immutable terrain
  revision IDs in a bounded 4,096-entry thread-local table (96 KiB per thread).
  Copies can share results; rebuilding terrain changes the revision. Dynamic
  doors, props, platforms, collision fitting and support checks still execute.
  Explicit uncached reference scopes bypass this table too.
- Minimap occupancy retains the original `World::solid` results for its 576
  fixed sample points. An exact snapshot of tile, fixture, prop and terminal
  inputs invalidates it; contacts and player markers still update every frame.
- Decorative HUD/menu panels retain their original shaded pixels, keyed by
  coordinates, dimensions, material flags and texture generation. Sixteen
  entries, each limited to 65,536 pixels, bound retained work. Larger panels
  use the original path. Clipped rows copy only visible pixels. Text, meters,
  prompts, shadows and panel fasteners retain their ordinary draw order.
- Coarse profiling now exposes HUD/post-scene work, frame-fence waiting,
  render setup, vertex-buffer allocation time and CPU/GPU vertex-buffer growth.
  `--uncached-terrain-surfaces`, `--uncached-minimap` and the lab's
  `--uncached-panels` provide reference paths.

The diagnostic A/B runs put overlay work near **1.07 ms**. In the final C
competition its median mean is **0.172 ms**. Fence waits average **0.003 ms**;
this does not measure total GPU execution. C still spends about **2.227 ms**
in simulation and **2.895 ms** preparing the scene. These dominate the next
optimization opportunity. No AI decision rates, animation samples, geometry,
lighting settings or render scale were reduced in this round.

## Validation and artifacts

- `--visibility-test`: **28 views**, zero changed pixels for both occlusion
  and cached-versus-live terrain/minimap/panel rendering. Also exact panel
  clipping/material/cache-hit comparisons across **24 layouts**.
- `--terrain-query-test`: layered terrain reference checks, exact float results,
  terrain copy/rebuild invalidation and explicit uncached bypass all passed.
- `--minimap-cache-test`: **13 checks** passed, including destroyed cover,
  a moving lift terminal, replacement geometry and eight campaign levels.
- `--ai-query-test`: cached and uncached AI state and full save data match
  through three replays and restoration.
- Navigation, decision clock, guards, friendly actors, guard schedules/danger/
  patients/cover/roles/tables, Engineer drones, Exclusion, stealth, streaming,
  saves, civilian/creature schedules and lifts: **18 targeted commands passed**.
- `--gpu-actor-test`: **56 views passed** the existing CPU/GPU geometry,
  material and lighting reference thresholds (429,728 hardware triangles).
- `--presentation-resize-test` and the final diagnostic `--lab-profile` passed.
  The profile preserves **14,029,424** collision queries, **23,829** route
  queries / **16,696** route hits, sensing **11,831** runs, planning **9,493**
  runs and final health **99,158**, matching the original combat replay.
- Full `--smoke-test --vulkan`: **passed, exit 0**, including save/script checks,
  model/animation validation, weapon attachment, audio and final rendering.

The final deep profile reused **4,470,224 / 9,282,967** terrain height queries;
the retained minimap rebuilt zero times over the 3,600 measured frames. Vertex
vectors grew on one measured frame and GPU vertex buffers grew twice. These
counters do not cover every allocator, and the small measured fence/allocation
costs do not identify the cause of older, much larger sporadic hitches.

Local raw data, logs, preserved binaries and analysis script:
`.build/competitive-20261017/`. `final-original-{1,2,3}` and
`candidate-c-{1,2,3}` are the final competition; `original-*`, `candidate-a-*`
and `candidate-b-*` are the earlier elimination round.

| Executable | Bytes | SHA-256 |
| --- | ---: | --- |
| Original | 26,784,256 | `712d0d2ffd1eee5cdf096a1f9ce4382ba673b9b36a9e5640fadd575df4189de3` |
| Winner C | 26,896,896 | `17059affba29ab4e694d7e0177cdee69c35829c86510d1ec3985813c04277c0a` |

Winner size: **25.65 MiB**, leaving **6.35 MiB** below the unchanged 32 MiB
ceiling. The asset packer verified all **230 resources** exactly against source.
The Release build passed with existing build warnings plus MSVC C4744 warnings
about inline atomic diagnostic toggles' type alignment metadata. Reference tests
exercise their enabled and disabled paths. No newer Vulkan feature requirement was added;
physical testing on a 2015 GPU was not performed.
