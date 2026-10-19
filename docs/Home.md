---
tags: [moc]
aliases: [Documentation Hub, Index]
---

# RawMetal

Shipped **v0.7.2-preview.13** (2026-10-19): approved spatial audio first pass,
geometry-driven wall muffling and point-source stereo correction. Root EXE
**23.05 MiB**, **8.95 MiB free**. See [[Performance/audio-spatial-pass1-20261019]].

Shipped **v0.7.2-preview.12** (2026-10-19): second neural texture pass, **16 reduced
textures**, root EXE **23.05 MiB** (another **1.24 MiB** below preview.11),
**8.95 MiB free**. Validation and comparisons:
[[Performance/preview12-stats-20261019]] and [[Assets/neural-textures-round2-20261019]]. FPS runs varied; no gain claimed.

| | |
| --- | --- |
| Performance | [[Performance/preview12-stats-20261019\|Preview.12 patch statistics]] · [[Performance/Exclusion Zone\|Exclusion Zone]] |
| AI | [[AI/GoldSrc AI\|GoldSrc]] · [[AI/ai-perception\|Perception]] |
| Design | [[Design/PROJECT_NOTES\|Project notes]] |
| Systems | [[Systems/Index\|Systems]] |
| Content | [[Content/Index\|Content]] |
| Assets | [[Assets/Index\|Assets]] · [[Assets/neural-textures-round2-20261019\|Neural textures / preview.12]] |
| Archive | [[Archive/Index\|Archive]] |

Shipped **v0.7.2-preview.11** (2026-10-18): seven cached neural texture
reconstructions; EXE **24.29 MiB**, **943 KiB smaller than preview.10**, with
**7.71 MiB** free. Matched lab comparison **315.28 vs 318.69 FPS** (1.07% slower
for the size reduction). Decoder/resource checks, Vulkan smoke and 28 visibility
views passed — [[Assets/neural-textures-expanded-20261018]]. Prior lab round: **308.4 to 314.2 FPS**,
1% lows **187.8 to 199.9**; 330 FPS remains unmet —
[[Performance/Competitive Round 10 20261018]].

Ultrakill bar (2026-10-16 night): median lab ~**110 FPS** vs ~**500 FPS** Unity play on the same GPU — [[Performance/Ultrakill Bar]]. Prior route/instance batch **103 → 114 FPS** → [[Performance/Low FPS Sections]]. Hub → [[Performance/Exclusion Zone]].
