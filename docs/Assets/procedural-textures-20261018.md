---
tags: [assets, textures, openktg]
date: 2026-10-18
---

# Procedural textures — five materials (2026-10-18)

Replaces five photographic PNGs with **RMP1** recipes that expand to **RMT1**
RGBA at resource load via OpenKTG noise (`src/ThirdParty/openktg`) and
`src/core/ProceduralTexture.cpp`. Shipped as **v0.7.2-preview.10**.

## Materials

| resource | recipe | former PNG (archived) |
| ---: | --- | --- |
| 125 | `assets/materials/procedural/concrete-floor.rmp` | `assets/texture-old/concrete-floor.png` |
| 131 | `assets/materials/procedural/sign-rust.rmp` | `assets/texture-old/sign-rust.png` |
| 132 | `assets/materials/procedural/panel-metal.rmp` | `assets/texture-old/panel-metal.png` |
| 149 | `assets/materials/procedural/service-floor.rmp` | `assets/texture-old/service-floor.png` |
| 150 | `assets/materials/procedural/weathered-metal.rmp` | `assets/texture-old/weathered-metal.png` |

Kinds: 1 concrete, 2 sign rust, 3 panel metal, 4 service floor (3×3 tiles),
5 weathered metal. Originals are **not** referenced by `assets.rc`; keep
`texture-old/` for A/B restore.

## Package size

| | bytes |
| --- | ---: |
| Five PNG sources | 1,184,281 (~1.13 MiB) |
| Five RMP recipes | 160 |
| Source replacement | ~1.13 MiB |
| Root EXE vs preview.9 | 27,236,352 → 26,434,048 (−802,304 / −783 KiB) |

Runtime still holds full RGBA at the same dimensions after generation. Savings
are package/disk, not VRAM.

## Visual quality (lab)

Laplacian high-frequency energy vs the archived PNGs and the first procedural
previews:

| material | orig HF | first proc | current |
| --- | ---: | ---: | ---: |
| concrete-floor | 36.4 | 20.4 | 26.2 |
| sign-rust | 9.5 | 20.6 | 18.3 |
| panel-metal | 6.2 | 4.7 | 7.1 |
| service-floor | 37.6 | 13.0 | 27.0 |
| weathered-metal | 29.1 | 10.5 | 17.6 |

Contact sheet (PNG top / procedural bottom):
[before-after.png](../evidence/procedural-textures-20261018/before-after.png).
Generator notes: `src/ThirdParty/openktg/RAWMETAL.md`.

Accepted as **good enough for in-game look** after side-by-side review. Further
fidelity work can continue without restoring the PNGs to the resource table.

## Validation

Executable SHA-256:
`8AF412E7214016E025B7FEDEF937A618AD9B15368AE329362FF292FDC3A9970C`.
Size **26,434,048 bytes / 25.21 MiB**; **7.12 MiB** headroom under 32 MiB.
Pack verifier: **230** packaged resources; **5** procedural recipes verified.

`RawMetalProceduralTextureTest`: all five recipes deterministic, opaque,
non-flat, malformed recipes rejected.

Full release suite (**25/25 PASS**, each with `--smoke-test --vulkan`):
[release-checks.txt](../evidence/procedural-textures-20261018/release-checks.txt).
Unit test log:
[QUALITY.md](../evidence/procedural-textures-20261018/QUALITY.md) (unit-test log section).
