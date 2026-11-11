# DepthWorks promotional assets

Made 2026-10-26 for the Chapter 1 playtest (v0.14.0); redone 2026-10-28 for the
1.0 itch.io page (renders from the v1.0.2 build). All frames are the game's own
render (960x540, clean shots: no hands, no HUD), scaled 2x without smoothing,
captured with the states a player has on first arriving in each area.

## 1.0 itch.io set (spoiler-free; upload these)

Every still and the trailer share one grade (`.build/promo-1.0/look.py`):
crushed blacks, teal-black shadows / sodium-amber highlights, lamp bloom, a
slight red fringe, vignette, grain and faint scanlines. The wordmark is
Bahnschrift Bold Condensed, tracked out, with a stencil cut and a phosphor glow
in the terminal green of the game's mark (59,178,103); CHAPTER 1 in Consolas.

| File | Size | What it shows |
|---|---|---|
| trailer-720p.mp4 | 1280x720, 30 fps, 45.0 s, H.264 + AAC 48 kHz, 81 MB | the trailer (for YouTube; link it as the page video) |
| trailer-contact-sheet.png | 1920x1704 | one frame every 2 s of the trailer, for review |
| loop.gif | 640x268, 15 fps, 5.3 s seamless loop, 5.0 MB | the dark switch hall, slow push, small wordmark; top of the description |
| cover-630x500.png | 630x500 | itch.io cover: a guard's silhouette in his own torchlight, wordmark over the dark |
| cover-check-315x250.png | 315x250 | the cover at half size, the readability check |
| banner-1920x540.png | 1920x540 | page banner: wordmark + CHAPTER 1 over the unlit switch hall's sodium lamp |
| background-1920x1080.png | 1920x1080 | page background: the cooling tower shell sunk to near black, darkest under the page column |
| shot-01.png | 1920x1080 | Switch hall: walkways, one lamp at the far end (chunk 37) |
| shot-02.png | 1920x1080 | Sump Works: the flooded gallery under the walkway (chunk 49) |
| shot-03.png | 1920x1080 | Cooling tower shell: catwalks under the red lining (chunk 51) |
| shot-04.png | 1920x1080 | The yard: a torch and the man holding it (chunk 58) |
| shot-05.png | 1920x1080 | Hiding in a locker: the world through the vent holes (chunk 57) |
| depthworks-logo.png | 2400x729, transparent | the wordmark with the rule and CHAPTER 1 |
| depthworks-logo-bare.png | 2400 wide, transparent | the wordmark alone |
| itch-copy.txt | text | title, tagline, description, controls, needs, page colours |
| review-board.png | 1920x3524 | every asset above on one sheet |

**Recommended upload set:** cover-630x500.png, banner-1920x540.png,
background-1920x1080.png, screenshots shot-01 .. shot-05 in that order,
loop.gif in the description, the trailer on YouTube.

Trailer music: `music_clinic.mp3` (game library, 0:00-0:33), cut dead at the
black before the title; impacts are the game's `lift-crash.wav` pitched down,
with `lift-motor`, `locker-shut`, `detect-sting` and a low `machine.wav` drone.
Render lists and scripts: `.build/promo-1.0/` (`trailer_shots.py`,
`trailer2_shots.py`, `compose_trailer.py`, `make_stills.py`, `look.py`).

## Chapter 2 set (`chapter-2/`, spoiler-free)

Made 2026-11-11 from the v1.3.0 build with Chapter 2 released. Same grade and
wordmark as the 1.0 set (`look.wordmark(label='CHAPTER 2')`). Nothing from the
chapter's ending (chunks 108-109) or its last speaker is shown.

| File | Size | What it shows |
|---|---|---|
| cover-630x500.png | 630x500 | the breach crystals under the wordmark, CHAPTER 2 · ENERGY RESEARCH |
| cover-check-315x250.png | 315x250 | the cover at half size |
| banner-1920x540.png | 1920x540 | wordmark + CHAPTER 2 over the breach's far wall |
| depthworks-logo-chapter2.png | 2400 wide, transparent | the wordmark with CHAPTER 2 |
| shot-01.png | 1920x1080 | Intake theatre: the welcome film (chunk 68) |
| shot-02.png | 1920x1080 | Screening hall: the gates to the laboratories (chunk 69) |
| shot-03.png | 1920x1080 | Suit room: the hazmat rack (chunk 69) |
| shot-04.png | 1920x1080 | Calibration pan: standing water under the gantry (chunk 84) |
| shot-05.png | 1920x1080 | The 11 kV plant bus, arcing on the floor (chunk 92) |
| shot-06.png | 1920x1080 | Test bay: the tower rig over its pit (chunk 86) |
| shot-07.png | 1920x1080 | Containment breach: the growth and its crystals (chunk 102) |
| shot-08.png | 1920x1080 | Containment breach, the far wall (chunk 102) |
| itch-copy.txt | text | tagline, devlog headline and the description's Chapter 2 section |
| review-board.png | 1920x3220 | everything above on one sheet |

Cameras: `chapter-2/shots.txt` (`--map-shots` list; run with `--clean-shots`
in `.build/promo-ch2`); grading: `chapter-2/make_ch2.py`, run from
`.build/promo-ch2` (it imports `.build/promo-1.0/look.py`). No Chapter 2
trailer or loop GIF yet.

## 0.14 playtest set (kept for reference)

Raw engine captures, ungraded. 02 (names the Cleanup Force) and 08 (the
chapter's ending) give story away: do not upload them to the store page.

| File | Size | What it shows |
|---|---|---|
| 01-emergency-generation.png | 1920x1080 | Emergency Generation: the generator sets |
| 02-load-control.png | 1920x1080 | Load Control: a Cleanup Force guard among the consoles |
| 03-turbine-rotor.png | 1920x1080 | Machine Hall, crane end: the turbine rotor |
| 04-sump-works.png | 1920x1080 | Sump Works: the flooded gallery under its lamps |
| 05-heat-exchange.png | 1920x1080 | Heat Exchange: the exchanger vessels |
| 06-security-yard.png | 1920x1080 | Security Perimeter yard: a soldier and his torch |
| 07-tram-4.png | 1920x1080 | Personnel Transit: Tram 4 held at Platform B |
| 08-great-door.png | 1920x1080 | Energy Research forecourt: the great door |
| 09-locker-watch.png | 1920x1080 | Hiding in a locker: a Cleanup man makes you out through the holes (v0.14.6) |
| 10-hauled-out.png | 1920x1080 | Pulled out of the locker at gunpoint (v0.14.6) |
| legacy-0.14/cover-630x500.png | 630x500 | the old cover (yard soldier, PLAYTEST 0.14.0) |
| legacy-0.14/banner-1920x540.png | 1920x540 | the old banner (the rotor, PLAYTEST 0.14.0) |
| legacy-0.14/background-1920x1080.png | 1920x1080 | the old background (the forecourt: shows the ending) |

To make more: `python tools/campaign/spot_views.py RawMetal.exe RUNDIR FIRST LAST`
renders six views per chunk into RUNDIR/.build/shot/sv_N_k.ppm; or write a
`--map-shots` list (`name level x y feet yaw pitch [set states | wait N | reach N]`,
one line per frame) and run `DepthWorks.exe --map-shots=LIST --clean-shots`.
Grade the frames with `look.grade` so they match the set.
