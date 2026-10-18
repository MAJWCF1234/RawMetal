---
tags: [performance, exclusion, validation]
date: 2026-10-17
---

# Competitive round 6: nav as authority

Friendly walk probes use **TraceHull-scale footprint pitch** (indoor **0.95 m**,
terrain **0.55 m**) instead of the dense lattice (**0.28 / 0.16**). Structure
sweeps still own solids between samples; door centerlines keep the fine lattice
so thin leaves cannot slip between footprint sites. Graph build and enemy
`roughSegment` are unchanged. Bypass: `--lattice-walk-probes`.

**Candidate wins** against the round-five root (idle emit reuse).

## Identity

| | |
| --- | --- |
| Candidate | `.build/nav-authority-20261017/candidate.exe` |
| SHA256 | `7E6E321FD6586109268497CF2D1A910258C5317C5C80AB80B5CFA7CE33121931` |
| Size | 27,793,920 bytes (under 32 MiB) |
| Baseline | Round-five root `5C0FDBD8…` |

Promoted to `N:/rawmetal/RawMetal.exe` after the win.

## Final paired competition

RTX 5060 Ti, `--lab-performance`, Vulkan HDR / 4× MSAA, 30 warmup + 3600
measured frames. Order: B1, C1, C2, B2, B3, C3.

| Run | Mean ms | Avg FPS | Sim ms | Scene ms | AI queries |
| --- | ---: | ---: | ---: | ---: | ---: |
| B1 | 4.783 | 209.1 | 1.516 | 2.391 | 14,029,424 |
| C1 | 4.230 | 236.4 | 0.931 | 2.378 | 5,749,274 |
| C2 | 4.283 | 233.5 | 0.937 | 2.409 | 5,749,274 |
| B2 | 4.825 | 207.3 | 1.521 | 2.422 | 14,029,424 |
| B3 | 4.796 | 208.5 | 1.517 | 2.404 | 14,029,424 |
| C3 | 4.307 | 232.2 | 0.941 | 2.418 | 5,749,274 |

| Three-run median | Round 5 root | Nav authority |
| --- | ---: | ---: |
| Mean frame time, ms | 4.796 | **4.283** |
| Average FPS | 208.5 | **233.5** |
| Simulation, ms | 1.517 | **0.937** |
| Scene, ms | 2.404 | 2.409 |
| AI queries / 60 s | 14.03M | **5.75M (−59%)** |

Versus its paired baseline: **−0.512 ms** mean (−10.7%), **+25 FPS** (+12.0%),
simulation **−0.580 ms** (−38%). Scene noise-flat (render path unchanged).

## Fingerprints

`final_health=99158` every run. Query counts drop by design (sparser probes);
route memo hit counts scale with the new probe volume.

## Checks

- `--nav-graph-test`, `--friendly-ai-test`, `--ai-decision-test`, `--exclusion-test`: PASS

## What advances

- `aiWalkAuthorityPitch` + `Game::setNavAuthority` (default on)
- `friendlyWalkSegment` sparse footprint sites; fine door centerline between them
- `--lattice-walk-probes` restores the prior dense lattice for A/B

Gap to 300 FPS (~3.33 ms): was ~**0.95 ms** after this round. Follow-up:
[[Performance/Competitive Round 7 20261017|round seven]] live walk memo.

Evidence: `.build/nav-authority-20261017/`.
