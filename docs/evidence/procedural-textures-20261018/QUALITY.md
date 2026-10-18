# Procedural texture quality 2026-10-18

Laplacian HF energy (higher = crunchier microdetail).

| material | orig | first proc | current |
|---|---:|---:|---:|
| concrete-floor | 36.4 | 20.4 | 26.2 |
| sign-rust | 9.5 | 20.6 | 18.3 |
| panel-metal | 6.2 | 4.7 | 7.1 |
| service-floor | 37.6 | 13.0 | 27.0 |
| weathered-metal | 29.1 | 10.5 | 17.6 |

Generator: src/core/ProceduralTexture.cpp
Recipes: src/assets/materials/procedural/*.rmp
Contact sheet: before-after.png (PNG top, procedural bottom)
