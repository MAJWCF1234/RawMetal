---
tags: [pass, performance]
aliases: [static-prop-cache-20261016]
date: 2026-10-16
---

# Static crate/prop cache

Tile `C`/`B` and authored props bake into the static VBO (cache rev 4).

**Result:** Exclusion **9.1 → 8.4 ms**; scene **3.59 → 3.05**.

Hub: [[Performance/Exclusion Zone]] · Evidence: `.build/static-props-20261016/`
