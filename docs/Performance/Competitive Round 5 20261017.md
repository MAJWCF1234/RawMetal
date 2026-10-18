---
tags: [performance, exclusion, validation]
date: 2026-10-17
---

# Competitive round 5: idle actor emit reuse

Cross-frame **body+gun triangle emit reuse** for rooted idle/crouch/death.
Standing aimers skip mesh emit when every float that moves skin or attachment
matches; walk and busy weapon actions stay exact. Phase is omitted only for
that sticky case so idle fidget does not bust the cache every frame.

**Candidate wins** the paired Exclusion lab against published preview.7.

## Identity

| | |
| --- | --- |
| Candidate | `.build/actor-emit-cache-20261017/candidate.exe` |
| SHA256 | `5C0FDBD84C2837203C7CDFB056B2AAEBBD53FA96A58A423291C71D718682DC7D` |
| Size | 27,791,872 bytes (under 32 MiB) |
| Baseline | Root preview.7 `661b557e…` / 27,405,312 bytes |

Promoted to `N:/rawmetal/RawMetal.exe` after the win. Bypass:
`--uncached-actor-emit`.

## Final paired competition

RTX 5060 Ti, `--lab-performance`, Vulkan HDR / 4× MSAA, 30 warmup + 3600
measured frames, fixed 60 Hz. Order: B1, C1, C2, B2, B3, C3.

| Run | Mean ms | Avg FPS | Scene ms |
| --- | ---: | ---: | ---: |
| B1 | 5.141 | 194.5 | 2.608 |
| C1 | 4.808 | 208.0 | 2.407 |
| C2 | 4.680 | 213.7 | 2.356 |
| B2 | 4.896 | 204.2 | 2.485 |
| B3 | 5.070 | 197.3 | 2.587 |
| C3 | 4.849 | 206.2 | 2.429 |

| Three-run median | Preview.7 | Emit reuse |
| --- | ---: | ---: |
| Mean frame time, ms | 5.070 | **4.808** |
| Average FPS | 197.3 | **208.0** |
| Scene preparation, ms | 2.587 | **2.407** |

Versus its paired baseline: **−0.261 ms** mean (−5.2%), **+10.7 FPS**
(+5.4%), scene **−0.180 ms**. Not the full ~0.7 ms scene cut hoped for in a
dense firefight (sticky hits standing aimers; walkers still emit live).

## Fingerprints (every run)

`final_health=99158`, `ai_queries=14029424`, route `23829/16696`.

## Checks

- `--gpu-actor-test`: all views PASS; idle emit reuse `hits 0→1`, `changed_pixels=0`
- Same candidate with `--uncached-actor-emit`: mean 4.897 ms (single run), still
  above the cached median — cache contributes; host noise remains.

## What advances

- Exact float-bit identity over pose, weapon, visual, inspection stage
- Sticky rooted idle/crouch/death omits phase only when weapon is calm
- World-space `ActorTriangle` replay with fresh `beginActor` lighting
- Capsule of 128 emit slots; `--uncached-actor-emit` disables

Evidence: `.build/actor-emit-cache-20261017/`.
