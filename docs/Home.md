---
tags: [moc]
aliases: [Documentation Hub, Index]
---

# RawMetal

| | |
| --- | --- |
| Performance | [[Assets/neural-textures-expanded-20261018\|Performance/bloat patch / preview.11]] · [[Performance/Exclusion Zone\|Exclusion Zone]] |
| AI | [[AI/GoldSrc AI\|GoldSrc]] · [[AI/ai-perception\|Perception]] |
| Design | [[Design/PROJECT_NOTES\|Project notes]] |
| Systems | [[Systems/Index\|Systems]] |
| Content | [[Content/Index\|Content]] |
| Assets | [[Assets/Index\|Assets]] · [[Assets/neural-textures-expanded-20261018\|Neural textures / preview.11]] |
| Archive | [[Archive/Index\|Archive]] |

Shipped **v0.7.2-preview.11** (2026-10-18): seven cached neural texture
reconstructions; EXE **24.29 MiB**, **943 KiB smaller than preview.10**, with
**7.71 MiB** free. Matched lab comparison **315.28 vs 318.69 FPS** (1.07% slower
for the size reduction). Decoder/resource checks, Vulkan smoke and 28 visibility
views passed — [[Assets/neural-textures-expanded-20261018]]. Prior lab round: **308.4 to 314.2 FPS**,
1% lows **187.8 to 199.9**; 330 FPS remains unmet —
[[Performance/Competitive Round 10 20261018]].

Ultrakill bar (2026-10-16 night): median lab ~**110 FPS** vs ~**500 FPS** Unity play on the same GPU — [[Performance/Ultrakill Bar]]. Prior route/instance batch **103 → 114 FPS** → [[Performance/Low FPS Sections]]. Hub → [[Performance/Exclusion Zone]].
