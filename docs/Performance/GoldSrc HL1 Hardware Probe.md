---
tags: [performance, goldsrc, research]
aliases: [GoldSrc bench, HL1 hardware probe]
date: 2026-10-17
---

# GoldSrc (Half-Life 1) on this PC

Booted and measured **Steam Half-Life (app 70)** on the RTX 5060 Ti box — not HL2.

Evidence: `.build/goldsrc-bench-20261017/goldsrc-hostspeeds.log`  
Compare: [[Performance/Preview 7 Breakdown]] · [[AI/GoldSrc AI]]

## Setup

| | |
| --- | --- |
| Game | Half-Life (GoldSrc / 25th-anniversary OpenGL path) |
| Map | `c1a0` (train intro) |
| Resolution | 1920×1080 windowed, `-novid` |
| Probe | engine `host_speeds 1` → `qconsole.log` (~5k samples, ents≈202) |
| Caps | Display stayed **vsync-locked ~60 FPS** (`gl_vsync 0` did not unlock presents) |

Demo `record`/`timedemo` was blocked (“machine that allows creation of files in valve”), so no official timedemo FPS line. Numbers below are live `host_speeds` during play.

## Measured (play, ents > 50)

| Metric | Median | Notes |
| --- | ---: | --- |
| Display FPS | **60** | Vsync / composed present |
| `gfx` ms | **9** | Renderer work only |
| `host` ms | **7** | Includes frame pacing / wait under vsync |
| `sv` / `cl` ms | **0** | Idle train ride |

**Renderer headroom (gfx alone):** 1000/9 ≈ **~110 FPS** if the frame were only gfx and uncapped.  
That is an estimate, not a measured uncapped present rate.

## Vs RawMetal Exclusion (preview.7)

| | GoldSrc HL1 `c1a0` | RawMetal Exclusion lab |
| --- | --- | --- |
| Resolution | 1080p | 640×360 lab window |
| Content | Light intro, ~200 ents | Combat yard, dense AI queries |
| Display / mean FPS | ~60 (vsync) | **~202** median |
| Hot cost | gfx ~**9 ms** | scene ~**2.5 ms** + sim ~**1.5 ms** |

So: we are **not** “at HL1 performance” in the sense of matching GoldSrc’s **cost model**. HL1 paints a full 1080p train ride for ~9 ms gfx with almost no sim. RawMetal’s Exclusion lab is already faster *as a frame rate number* at much lower resolution, but still spends real CPU on **walk/fits lattices and actor emit** — the thing GoldSrc avoided with nodes + sparse traces ([[AI/GoldSrc AI]]).

## Limits

- One map, one resolution, vsync on — not a max-FPS GoldSrc crown.
- Anniversary build; filesystem blocked demo recording.
- PresentMon CSV capture failed in this session (no elevated ETW write).
- HL2 was **not** the target of this probe.
