---
tags: [performance, exclusion, lab]
aliases:
  - Exclusion Zone Performance
  - Exclusion performance
  - lab-performance
  - nav-walk-pitch-20261016
  - static-prop-cache-20261016
  - ai-think-lod-20261016
  - ai-query-performance-20261016
  - gpu-actor-performance-20261016
  - lab-runtime-performance-20261015
  - exclusion-lab-performance-20261015
  - visibility-performance-20261015
date: 2026-10-17
---

# Exclusion Zone

Canonical note for Exclusion yard CPU cost. Dated stubs: [[Performance/Archive]].

**Harness:** `RawMetal.exe --lab-performance` — 30 warmup + 3600 measured frames, fixed 60 Hz replay, live muted mixer, AI query cache on unless `--uncached-ai-queries`. RTX 5060 Ti, Vulkan HDR / 4× MSAA. Times are **CPU wall** (include present), not GPU timestamps.

**Product shape:** ~26 MiB stylized build, handful of NPCs. ~84 FPS was unacceptable for that profile; GoldSrc had stronger AI cheaper because of a different **cost model** ([[AI/GoldSrc AI|GoldSrc AI]]), not because AI is inherently expensive.

**Ultrakill bar (2026-10-16 night):** same GPU, full Unity title ~500 FPS with denser content — fresh three-run Exclusion median ~**9.08 ms** (~**110 FPS**), submit+present ~0.8 ms. Full write-up: [[Performance/Ultrakill Bar]].

## Current numbers

Fresh original-versus-retained-panel competition (three runs each): median
average FPS **140.2 → 163.6**, 1% lows **92.2 → 103.6**, P99 **10.108 → 9.039 ms**.
The frame-time spread remains essentially flat at **5.23 ms**; consistency and
the 500 FPS target remain unfinished. Full runs, exact-pixel checks and limits:
[[Performance/Competitive Round 20261017|Competitive round, October 17]].

### Previous measured round

Exact retained route probes plus instanced actor triangles: fresh three-run
medians improve average FPS **103.2 → 113.9**, 1% lows **43.1 → 59.3**,
and P99-minus-P1 frame-time spread **12.316 → 9.093 ms** (**26.2% narrower**).
Submission wall time falls **31.8%**. The final candidate still has a 36.7 ms
hitch; the gap remains open. See [[Performance/Low FPS Sections|Low FPS sections]]
for all runs, the route-only retest that did not raise lows, reference checks
and limitations. Compare within each batch; host conditions vary substantially.

### Earlier measurements

Best sustained combat-yard mean after static props (calm-patrol LOD barely applies in this firefight probe):

| Metric | Value |
| --- | ---: |
| Total mean | **8.40 ms** (~**119 FPS**) |
| Simulation mean | 3.27 ms |
| Scene mean | 3.05 ms |
| Submit + present | ~0.8 ms |
| Eligible `fits`/`support` queries / 60 s | ~30.7M |

After calm-patrol think LOD (combat yard; noise-flat totals):

| Metric | Value |
| --- | ---: |
| Total mean | 8.85 ms |
| Simulation mean | 3.23 ms |
| Queries / 60 s | ~30.0M |

GPU submit+present CPU wall time stays ~1 ms in these older measurements.
That is not a GPU execution timestamp and does not by itself establish GPU load.

## Cost shape

| Bucket | Share | What it is |
| --- | --- | --- |
| Simulation | ~40% | Friendly AI; walk/intent; collision probes |
| Scene | ~35–40% | Actor mesh/attachment + rooms; static world mostly cached |
| Submit / present / audio | ~20% | Vulkan + mixer |

Deep timers (earlier short replay): **walk / fits / support** under AI; **actor_mesh** under scene. Skin alone is tiny (~0.08 ms) — do not chase GPU skinning first.

## Pass timeline

### 2026-10-15 — foundation

| Pass | What landed | Exclusion effect |
| --- | --- | --- |
| Terrain column | Stack-local density reuse | Short probe ~24.8 → ~19.2 ms |
| Lab runtime | Attachment reuse; `--lab-performance` | Sustained ~12.3 ms |
| Visibility | Coarse occlusion; no BSP/PVS | Partial |
| GPU actor lighting | Face light / projection in VS | Scene ~18% in paired test |
| AI query memo | Per-actor batch (1024 slots) | ~12.8 → ~11.9 ms; ~20% hits |

### 2026-10-16 — cost-model cuts

| Pass | What landed | Exclusion effect |
| --- | --- | --- |
| [[Performance/Archive/nav-walk-pitch-20261016\|Hull-scale walk]] | Indoor `0.08`→`0.28`, terrain→`0.16` | **11.9→9.1 ms**; queries **50.6M→30.7M** |
| [[Performance/Archive/static-prop-cache-20261016\|Static crates/props]] | Bake into static VBO | **9.1→8.4 ms**; scene **3.59→3.05** |
| Cross-actor memo / pitch `0.40` | Tried | **Reverted** |
| [[Performance/Archive/ai-think-lod-20261016\|Calm patrol LOD]] | Alternate-frame think when calm | Queries ↓; combat yard FPS noise-flat |
| [[Performance/AI Decision Clock\|Decision clock]] | Staggered threat selection/tactics; movement stays full rate | Simulation **−16.9%**; average FPS **+6.9%**, 1% lows **+4.2%** |
| [[Performance/Low FPS Sections\|Route memo + actor instances]] | Exact retained graph probes; 152-byte triangles instead of 312-byte expanded stream | Average FPS **+10.3%**; 1% lows **+37.4%** in final fresh batch |
| [[Performance/Archive/walk-start-skip-20261016\|Walk start skip]] | Don’t re-prove current footprint (match enemy roughSegment) | Queries **17.4M→14.0M** (−19%); same Exclusion replay health |

## Architecture verdict

Wrong: “AI is too complex.”  
Right: movement still does **continuous solid probing** instead of GoldSrc **node hop + few hull/LOS traces**.

| GoldSrc | RawMetal today |
| --- | --- |
| Node graph owns walkability | Graph exists; live walk still owns collision |
| One hull move / TraceHull | Hull-scale lattice (not nav authority) |
| Sparse LOS / think intervals | Staggered selection/tactics; known-target LOS and task execution live |
| BSP + PVS | Coarse frustum; static bake uncullled |
| Studio bone palette | CPU pose + per-vertex arm attach |

Capability gap (schedules/cover): [[AI/GoldSrc AI]]. Capability ≠ cost.

## Remaining work

1. ~~**Nav as authority**~~ — [[Performance/Competitive Round 6 20261017|round six]]
2. ~~**Live walk-segment memo**~~ — [[Performance/Competitive Round 7 20261017|round seven]]
3. ~~**PGO + animation/retreat**~~ — [[Performance/Competitive Round 9 20261018|round nine]] / preview.8 (~304 FPS lab)
4. ~~**Cross-frame idle actor draw cache**~~ — [[Performance/Competitive Round 5 20261017|round five]]
5. **Broader scheduled planning** — support jobs and creature sensing
6. **Path to ~2 ms / Ultrakill play bar** — hitch hunt, sim/render overlap, BSP/PVS

Cheap one-liners on Exclusion are largely exhausted after pitch + static props.

## Commands

```text
RawMetal.exe --lab-performance
RawMetal.exe --lab-profile
RawMetal.exe --nav-graph-test
RawMetal.exe --friendly-ai-test
RawMetal.exe --exclusion-test
RawMetal.exe --ai-query-test
```

## Evidence

`.build/low-fps-20261016/` · `.build/nav-pitch-20261016/` · `.build/static-props-20261016/` · `.build/ai-think-lod-20261016/` · `.build/ai-query-batch-20261016/` · `.build/gpu-actors-20261016/`
