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
date: 2026-10-16
---

# Exclusion Zone

Canonical note for Exclusion yard CPU cost. Dated stubs: [[Performance/Archive]].

**Harness:** `RawMetal.exe --lab-performance` — 30 warmup + 3600 measured frames, fixed 60 Hz replay, live muted mixer, AI query cache on unless `--uncached-ai-queries`. RTX 5060 Ti, Vulkan HDR / 4× MSAA. Times are **CPU wall** (include present), not GPU timestamps.

**Product shape:** ~26 MiB stylized build, handful of NPCs. ~84 FPS was unacceptable for that profile; GoldSrc had stronger AI cheaper because of a different **cost model** ([[AI/GoldSrc AI|GoldSrc AI]]), not because AI is inherently expensive.

## Current numbers

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

GPU submit+present stays ~1 ms — still **not GPU-bound**.

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

## Architecture verdict

Wrong: “AI is too complex.”  
Right: movement still does **continuous solid probing** instead of GoldSrc **node hop + few hull/LOS traces**.

| GoldSrc | RawMetal today |
| --- | --- |
| Node graph owns walkability | Graph exists; live walk still owns collision |
| One hull move / TraceHull | Hull-scale lattice (not nav authority) |
| Sparse LOS / think intervals | Calm patrol LOD only |
| BSP + PVS | Coarse frustum; static bake uncullled |
| Studio bone palette | CPU pose + per-vertex arm attach |

Capability gap (schedules/cover): [[AI/GoldSrc AI]]. Capability ≠ cost.

## Remaining work

1. **Nav as authority** — largest sim cut left  
2. **Shared multi-frame spatial cache** — invalidation trap  
3. **Broader AI think LOD** — beyond calm patrol  
4. **Cross-frame idle actor draw cache**  
5. **GPU attachment / bones** — skin alone is not the bill  
6. **Sim/render overlap**  
7. **BSP/PVS**

Cheap one-liners on Exclusion are largely exhausted after pitch + static props.

## Commands

```text
RawMetal.exe --lab-performance
RawMetal.exe --nav-graph-test
RawMetal.exe --friendly-ai-test
RawMetal.exe --exclusion-test
RawMetal.exe --ai-query-test
```

## Evidence

`.build/nav-pitch-20261016/` · `.build/static-props-20261016/` · `.build/ai-think-lod-20261016/` · `.build/ai-query-batch-20261016/` · `.build/gpu-actors-20261016/`
