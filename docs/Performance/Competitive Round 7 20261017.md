---
tags: [performance, exclusion, validation]
date: 2026-10-17
---

# Competitive round 7: live walk-segment memo

`WalkQueryCache` now covers **live locomotion** as well as planning. A
`WalkQueryCache::Scope` opens for each friendly AI update (and nested graph
planning). `friendlyWalkSegment` hits the same exact float-bit memo; seam
crossings stay live. Bypass remains `--uncached-ai-queries`.

**Candidate wins** against the round-six root (nav authority).

## Identity

| | |
| --- | --- |
| Candidate | `.build/live-walk-memo-20261017/candidate.exe` |
| SHA256 | `EDAF720D6021D99D5AA351D6CDB81AE23B7B0EB0597AC0B6B7516DE8EF0F34C9` |
| Size | 27,795,456 bytes (under 32 MiB) |
| Baseline | Round-six root `7E6E321F…` |

Promoted to `N:/rawmetal/RawMetal.exe` after the win.

## Final paired competition

RTX 5060 Ti, `--lab-performance`, Vulkan HDR / 4× MSAA, 30 warmup + 3600
measured frames. Order: B1, C1, C2, B2, B3, C3.

| Run | Mean ms | Avg FPS | Sim ms | Scene ms | AI queries | Route q/hits |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| B1 | 4.197 | 238.3 | 0.936 | 2.383 | 5,749,274 | 23,829 / 16,697 |
| C1 | 4.033 | 248.0 | 0.818 | 2.346 | 4,081,122 | 274,140 / 160,268 |
| C2 | 4.098 | 244.0 | 0.842 | 2.374 | 4,081,122 | 274,140 / 160,268 |
| B2 | 4.161 | 240.3 | 0.943 | 2.351 | 5,749,274 | 23,829 / 16,697 |
| B3 | 4.121 | 242.7 | 0.920 | 2.336 | 5,749,274 | 23,829 / 16,697 |
| C3 | 4.058 | 246.4 | 0.824 | 2.351 | 4,081,122 | 274,140 / 160,268 |

| Three-run median | Round 6 root | Live walk memo |
| --- | ---: | ---: |
| Mean frame time, ms | 4.161 | **4.058** |
| Average FPS | 240.3 | **246.4** |
| Simulation, ms | 0.936 | **0.824** |
| Scene, ms | 2.351 | 2.351 |
| AI queries / 60 s | 5.75M | **4.08M (−29%)** |
| Route memo | 23.8k / 16.7k | 274k / 160k (58.5%) |

Versus its paired baseline: **−0.103 ms** mean (−2.5%), **+6.1 FPS** (+2.5%),
simulation **−0.112 ms** (−11.9%). Scene noise-flat. Route stats now include live
locomotion probes (planning-only counts no longer apply).

## Fingerprints

`final_health=99158` every run. Query counts drop by design (exact reuse);
route volume rises because live sweeps join the memo.

## Checks

- `--nav-graph-test`, `--friendly-ai-test`, `--ai-decision-test`, `--exclusion-test`: PASS

## What advances

- `WalkQueryCache::Scope` + TLS active memo shared by planning and live walk
- `friendlyWalkSegment` / `friendlyWalkSegmentUncached` split; null `endFeet` still memoizes
- Nested scopes keep one collision snapshot for a full actor update

Gap to 300 FPS (~3.33 ms): still ~**0.73 ms**. Next: remaining non-walk probe
storms, deeper graph-only hops, hitch hunt, or scene emit widening.

Evidence: `.build/live-walk-memo-20261017/`.
