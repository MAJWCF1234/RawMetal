---
tags: [performance, archive]
aliases: [walk-start-skip-20261016]
date: 2026-10-16
---

# Walk start-footprint skip

Hub: [[Performance/Exclusion Zone]] · Bar: [[Performance/Ultrakill Bar]]

**Trick:** `friendlyWalkSegment` no longer re-proves the actor’s current footprint. Enemy `roughSegment` already sampled from `i=1`; friendlies paid start+goal on every short step. Same destination fits / support / structure / doors — TraceHull-style, not a content cut.

**Evidence:** `.build/walk-start-skip-20261016/` — Exclusion AI queries **17,361,806 → 14,029,424** (−19.2%) at identical route memo hits. `--friendly-ai-test`, `--exclusion-test`, `--ai-decision-test`, `--nav-graph-test` (from repo root) PASS.
