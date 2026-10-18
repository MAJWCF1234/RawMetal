---
tags: [performance, goldsrc, research]
aliases: [GoldSrc bench, HL1 hardware]
date: 2026-10-17
---

# GoldSrc on this hardware (booted)

RTX 5060 Ti · Steam Half-Life (app 70) · `c1a0` · **1920×1080** windowed · `-novid`.
Measured with engine `host_speeds 1` → `qconsole.log` (5170 samples, ents>50).

Evidence: `.build/goldsrc-bench-20261017/goldsrc-hostspeeds.log`.

## Result

| Metric | Value |
| --- | ---: |
| Display FPS (median / avg) | **60 / 59.8** |
| `gfx` ms (median) | **9** |
| `host` ms (median) | **7** |
| Entities | ~202 |

Display is **vsync-locked ~60**. Renderer work is only ~**9 ms** → uncapped headroom from gfx alone ≈ **~110 FPS** on this light train intro (not a combat yard).

Demo `record`/`timedemo` was blocked by Steam (“creation of files in valve”). PresentMon CSV capture did not stick without elevation. HL2 Source install here is heavily Workshop-modded — not used as a clean baseline.

## Vs RawMetal preview.7 Exclusion lab

| | RawMetal Exclusion | GoldSrc c1a0 |
| --- | --- | --- |
| Resolution | 640×360 lab | 1920×1080 |
| Content | Combat yard + dense AI | Train intro |
| Median FPS | **~202** | **60** (vsync) / ~**110** gfx-implied uncapped |
| Frame cost shape | sim ~1.5 + scene ~2.5 ms | gfx ~9 ms @ 1080p |

**Are we “at HL2/GoldSrc”?** Not as a like-for-like claim. RawMetal’s lab FPS is higher, but at much lower resolution with a totally different CPU tax (AI probes + actor prep). GoldSrc’s lesson on this PC is still the cost model: a full 1080p classic frame for ~9 ms gfx, while Exclusion still spends milliseconds on sim/scene at 360p.

Related: [[Performance/Preview 7 Breakdown]] · [[AI/GoldSrc AI]] · [[Performance/Ultrakill Bar]].
