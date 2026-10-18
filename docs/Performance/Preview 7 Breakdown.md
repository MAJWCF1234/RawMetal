---
tags: [performance, exclusion, report]
aliases: [preview.7 breakdown, fresh breakdown]
date: 2026-10-17
---

# Preview.7 — fresh lab breakdown

Canonical root EXE is the round-four winner (v0.7.2-preview.7).

**Identity:** `N:/rawmetal/RawMetal.exe` — **27,405,312** bytes, SHA256
`661b557e7d73275327d2ed81edca4d2f6c9b9a9d9bff1ec0a40c3d02c7d69d34`. Matches GitHub
release asset. File **LastWriteTime 5:44:58 PM** is the original winner *build*
timestamp (Copy-Item preserves it); promote/publish was later. Not an older binary.

Evidence: `.build/preview7-breakdown-20261017/`. Related: [[Performance/Competitive Round 4 20261017]] · [[Performance/Ultrakill Bar]].

## Fresh three-run lab (now)

`--lab-performance`, RTX 5060 Ti, Vulkan HDR / 4× MSAA, CPU wall including present.

| Run | Mean ms | Avg FPS | 1% low | Sim | Scene | Submit+present | Worst ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 5.040 | 198.4 | 124.1 | 1.583 | 2.577 | 0.521 | 13.03 |
| 2 | 4.942 | 202.4 | 127.8 | 1.549 | 2.532 | 0.514 | 8.96 |
| 3 | 4.918 | 203.3 | 128.5 | 1.540 | 2.527 | 0.511 | 11.97 |
| **Median** | **4.942** | **202.4** | **127.8** | **1.549** | **2.532** | **0.514** | — |

Fingerprint every run: `ai_queries=14029424`, route `23829/16696`, `final_health=99158`.

## Frame share (median)

| Bucket | ms | Share |
| --- | ---: | ---: |
| Scene prep | 2.532 | ~51% |
| Simulation | 1.549 | ~31% |
| Other in total | ~0.85 | ~17% |
| Submit + present | 0.514 | ~10% |
| Audio (in other) | ~0.16 | — |

Still **CPU-bound**. GPU path ~0.5 ms. Scene is the largest remaining wall.

## Vs recent milestones

| Milestone | Median FPS | Mean ms | Notes |
| --- | ---: | ---: | --- |
| Ultrakill-bar night | ~110 | 9.08 | Pre round-2/3/4 stack |
| Walk-start skip (quiet) | ~133 | 7.51 | Queries 17.4M→14.0M |
| Preview.6 (promote check) | ~190 | 5.27 | Round-3 state-key winner |
| **Preview.7 (this batch)** | **~202** | **4.94** | Compact stream + Ob3 |
| Ultrakill play bar | ~500 | ~2.0 | Full Unity, denser content |

Gap to Ultrakill bar: still ~**2.5×** on frame time (4.94 vs ~2.0 ms).

## Verdict

Root **is** preview.7. Fresh labs reproduce the competition headline (~202 FPS median).
Next cost is still scene CPU emit / idle draw tricks and nav authority — not “wrong EXE.”
