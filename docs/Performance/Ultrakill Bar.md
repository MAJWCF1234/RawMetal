---
tags: [performance, exclusion, report]
aliases: [Ultrakill bar, Unity bar]
date: 2026-10-16
---

# Ultrakill bar — Exclusion yard report

Fresh three-run `--lab-performance` on the shipped **v0.7.2-preview.3** EXE, same hardware as the Ultrakill claim (RTX 5060 Ti). Framed against a stylized Unity title that runs ~**500 FPS** here with more polygons, denser action, and stronger AI — **while still paying Unity’s full engine tax**.

Evidence: `.build/ultrakill-bar-20261016/` (runs 1–3). Hub: [[Performance/Exclusion Zone]]. Cost model: [[AI/GoldSrc AI]].

## Verdict

**RawMetal is not GPU-bound and is not “too complex for the hardware.”** It is running the wrong **CPU cost model**. A custom ~26 MiB C++ cartridge should beat a full Unity game on this machine. Trailing Ultrakill at ~110 FPS average is a simulation/query architecture failure, not an engine-size problem.

| Bar | Frame budget | Implied FPS |
| --- | ---: | ---: |
| Ultrakill (play, this PC) | ~**2.0 ms** | ~**500** |
| RawMetal Exclusion lab (median of 3) | **9.08 ms** | ~**110** |
| Gap | **~4.5×** | |

## Fresh lab (2026-10-16 night)

Harness: 30 warmup + 3600 measured frames, fixed 60 Hz replay, live muted mixer, AI query cache on, deep timers off. Vulkan HDR / 4× MSAA. Times are **CPU wall** (include present).

| Run | Mean ms | Avg FPS | p50 ms | p99 ms | Worst ms | Sim mean | Scene mean | Submit+present |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 10.380 | 96.3 | 9.54 | 25.4 | **169.9** | 3.31 | 4.37 | 0.94 |
| 2 | 8.496 | 117.7 | 8.33 | 13.3 | 37.3 | 2.71 | 3.52 | 0.79 |
| 3 | 9.082 | 110.1 | 8.66 | 16.6 | 78.9 | 2.88 | 3.81 | 0.83 |
| **Median** | **9.082** | **110.1** | **8.66** | **16.6** | — | **2.88** | **3.81** | **0.83** |

Identical sim fingerprints every run: `ai_queries=17,361,806`, route `23,829` / hits `16,696` (70.1%), sensing/planning polls `21,600`, `final_health=99158`.

Submit + present ≈ **0.8 ms** of a **9 ms** frame (~9%). The GPU is waiting on the CPU.

## Where the frame goes (median run)

| Bucket | Mean ms | Share of 9.08 ms |
| --- | ---: | ---: |
| Scene | 3.81 | ~42% |
| Simulation | 2.88 | ~32% |
| Other / overhead in total | ~1.56 | ~17% |
| Submit + present | 0.83 | ~9% |
| Audio | 0.16 | ~2% |

Scene is CPU prep of actors/rooms (not “fill rate”). Simulation is still continuous solid probing for walk/fits — graph memo only covers **planning** sweeps ([[Performance/Low FPS Sections]]). Live movement still owns collision every tick.

## Why Unity at 500 FPS is the indictment

| | Ultrakill | RawMetal Exclusion |
| --- | --- | --- |
| Engine | Full Unity (GC, scene graph, editor runtime tax) | Custom C++ under **32 MiB** |
| Look | Stylized low-poly, dense action | Stylized, smaller cast |
| AI / combat | Dense, aggressive | Handful of NPCs |
| Observed | ~500 FPS | ~110 FPS lab average |
| Bound | Not this report | **CPU** (sim + scene prep) |

If Unity + heavier content can hold ~2 ms frames here, a thinner engine must be **wasting CPU on queries**, not “needing a bigger GPU.”

## Passes already banked (not enough)

From [[Performance/Exclusion Zone]] / [[Performance/Low FPS Sections]]:

- Hull-scale walk pitch, static prop VBO bake, calm patrol LOD
- Decision-clock stagger, exact route memo (70% planning hit), instanced actor triangles (152 B)

Those moved the needle (batch averages into the low hundreds of FPS). They do **not** close a **4.5×** Ultrakill gap. Cheap book is exhausted.

## What closes the gap

1. ~~**Nav as authority**~~ — [[Performance/Competitive Round 6 20261017|round six]]
2. ~~**Live walk-segment memo**~~ — [[Performance/Competitive Round 7 20261017|round seven]]
3. ~~**Cross-frame idle actor draw cache**~~ — [[Performance/Competitive Round 5 20261017|round five]]
4. ~~**PGO + animation/retreat correctness**~~ — [[Performance/Competitive Round 9 20261018|round nine]]
   promoted as **preview.8**: competition median **304.3 FPS** / promote-check
   **306.3 FPS** vs prior root. Midpoint **~300 FPS sustained lab average** is met.
5. Hitch hunt and path to the **~2 ms / ~500 FPS** play bar remain open

Target for “Ultrakill-class headroom” on this hardware: sustained lab mean
**≤ ~3 ms** (~300+ FPS) midpoint — **landed in round nine**; **≤ ~2 ms** to match
the cited play bar.

## Commands

```text
RawMetal.exe --lab-performance
RawMetal.exe --lab-profile
```
