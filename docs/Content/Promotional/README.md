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
chapter's ending (chunks 108-109) or its last speaker is shown. Reshot the same
day after the defect pass (`docs/Design/chapter2-defects-20261111.md`): modelled
crystals and bedrock in the breach, suits on hangers, the test bay stairs lit.

| File | Size | What it shows |
|---|---|---|
| cover-630x500.png | 630x500 | the breach crystals and heaved bedrock under the wordmark, CHAPTER 2 · ENERGY RESEARCH |
| cover-check-315x250.png | 315x250 | the cover at half size |
| banner-1920x540.png | 1920x540 | wordmark + CHAPTER 2 over the breach's far wall |
| depthworks-logo-chapter2.png | 2400 wide, transparent | the wordmark with CHAPTER 2 |
| shot-01.png | 1920x1080 | Intake theatre: the welcome film (chunk 68) |
| shot-02.png | 1920x1080 | Screening hall: the gates to the laboratories (chunk 69) |
| shot-03.png | 1920x1080 | Suit room: the hazmat suits on their hangers (chunk 69) |
| shot-04.png | 1920x1080 | Calibration pan: standing water under the gantry (chunk 84) |
| shot-05.png | 1920x1080 | Test bay: the tower rig over its pit (chunk 86) |
| shot-06.png | 1920x1080 | The 11 kV plant bus, banks live behind the barrier (chunk 92) |
| shot-07.png | 1920x1080 | Containment breach: the ore grown up through the floor (chunk 102) |
| shot-08.png | 1920x1080 | Containment breach, the far wall: the bedrock heaved into the room (chunk 102) |
| itch-copy.txt | text | tagline, devlog headline and the description's Chapter 2 section |
| review-board.png | 1920x3220 | everything above on one sheet |

Cameras: `chapter-2/shots.txt` (`--map-shots` list; run with `--clean-shots`
in `.build/promo-ch2`); grading: `chapter-2/make_ch2.py`, run from
`.build/promo-ch2` (it imports `.build/promo-1.0/look.py`). No Chapter 2
trailer or loop GIF yet.

## Chapter 2 promo v2 (`chapter-2/v2/`, the reveal; Chapter 2 still sealed)

Made 2026-11-12 from the v1.4 build (Chapter 2 sealed, chunks loaded directly).
New shots, a new score edit and a new look; nothing is re-cut from the 1.0
trailer or the v1 Chapter 2 stills. Spoiler rule as before: nothing from chunks
108-109, the finale or its last speaker. End card: DEPTHWORKS, CHAPTER 2 ·
ENERGY RESEARCH · COMING SOON, and CHAPTER 1 — FREE ON ITCH.IO; the studio
credit MAJWCF1234 PRESENTS opens it.

**Look ("cold hold", `look14.py`).** A cold bleach-bypass instead of the 1.0
sodium grade: silvered, desaturated steel with crushed blacks, reds kept at full
strength (blood is the only warm thing in the frame), red-orange film halation
round the lamps, luminance grain, a slight gate weave, no scanlines, a 2.20:1
band. Cut effects: flash frames on hits, an analog tear (slipped bands, split
channels) on hard cuts, chromatic smear on fast motion (red now, green a frame
back, blue two), decaying camera shake on the pounces, blood on the lens and a
red-black spatter wipe built from the game's own decal sheets
(`src/assets/library/blood/*.png`, tinted), and a film burn out of the end card.
Captions are a new voice: a red file tag over one line, revealed from under a
sliding redaction bar, dropping out with a flicker. The wordmark is the 1.0 one
(`look.wordmark`).

**How the creature footage was shot.** A capture-only flag, `--film=LIST`
(src/main.cpp, default off): one live scene stepped at 60 Hz with the player held
at an anchor, each frame drawn clean (no HUD or hands) from its own camera into
`.build/film/NAME.ppm`, with `film-log.txt` giving every creature's position per
frame. The sim is deterministic with the anchor fixed, so several cameras can
roll on one take and a second pass can aim at the logged animal. R-117's kill in
Containment A (96) is the scene `--chimera-kill-frames` stages; its returns in
99, 101 and 103 and the Machine in 92 (`Game::machineScene(1)`) are the scenes
the creature tests use. Its film lighting ops (capture only): `dark` (the chunk a
true-dark zone, no fill, exposure never metered up), `lights S` (every lamp
scaled), `ambient A`, `lamp X Y Z POWER RANGE KELVIN [R G B]` (a bare key/rim
lamp with a gel).

**R-117 is never shown whole and evenly lit** (the fix pass after review). Every
take with it is shot in true dark with the room's lamps at 30%, and the kill in
96 has one red work lamp in the service-run door behind it, so it reads as a
shape with lit edges (spines, jaw, claws) against red. In the edit each creature
shot is graded down to a near-silhouette (blacks crushed, contrast up) and
cropped tight to a part (the head against the red door, the spine line, the jaw
in the dark); a whole-body view lasts only a few frames in motion.

| File | Size | What it shows |
|---|---|---|
| trailer-1080p.mp4 | 1920x1080, 30 fps, 59.5 s, H.264 High (CRF 18, tune grain) + AAC 320k 48 kHz, 398 MB | the reveal trailer; give YouTube this one |
| trailer-720p-web.mp4 | 1280x720, 30 fps, 59.5 s, H.264 CRF 23 + AAC 160k, 61 MB | the share copy |
| trailer-contact-sheet.png | 1920x1010 | one frame every 2 s |
| cue-sheet.txt | text | every sound cue: time, frame, level, and the cut it lands on |
| shot-01.png | 1920x1080 | Intake theatre: the empty rows under the welcome film (chunk 68) |
| shot-02.png | 1920x1080 | Containment approach: the red hall, a door left open on the dark (chunk 100) |
| shot-03.png | 1920x1080 | Energy Test Labs: the glass test chambers (chunk 93) |
| shot-04.png | 1920x1080 | Containment B: the rock and the growth through the wall, a body under it (chunk 98) |
| shot-05.png | 1920x1080 | R-117 in the dark behind the bars of the specimen cages, lit at the edges (chunk 99) |
| shot-06.png | 1920x1080 | R-117 a black shape against the red work lamp of the service-run door, Containment A (chunk 96) |
| shot-07.png | 1920x1080 | R-117 rearing over the post man in the dark, jaw open (chunk 96) |
| shot-08.png | 1920x1080 | Experimental Power: the cursed equipment, its lamp lit in the dark (chunk 92) |
| cover-630x500.png | 630x500 | R-117 a shape against the red door lamp, spines and claws catching it; wordmark, CHAPTER 2 · ENERGY RESEARCH, COMING SOON on the dark floor below |
| cover-check-315x250.png | 315x250 | the cover at half size |
| banner-1920x540.png | 1920x540 | wordmark + CHAPTER 2 over the breach's crystals (102) |
| youtube-thumbnail-1280x720.png | 1280x720 | R-117 rearing over the post man in the dark, jaw lit; big CHAPTER 2, "IT EATS WHERE IT KILLS" |
| depthworks-logo-chapter2-v2.png | 2400 wide, transparent | the wordmark with CHAPTER 2 |
| review-board.png | 1920x4310 | everything above on one sheet, with the contact sheet |
| itch-copy.txt / youtube-upload.txt | text | the reveal: tagline, devlog, description block, upload sheet |

**The edit** (30 fps; frame numbers in `compose_ch2v2.py`). 0:00 black,
MAJWCF1234 PRESENTS. 0:03 intake theatre (68), 0:06.5 mortuary (78), 0:10 test
labs (93), 0:13 red hall (100), 0:16 Containment B rock (98), 0:19 the breach
(102), 0:21.5 R-117 in the dark behind the bars (99), 0:24 the office in 101,
something moving low behind the chairs. 0:27 the drop: R-117 against the red
door of the service run (96), the post man's back, the pounce, the bite (head off
on a flash frame, blood on the lens), the feed (spines and head over the body),
it gets up; a blood wipe into the return in 103 (a shape coming out of the dark
at us, jaw first, shake, black); the cages again, the return in 101 (the head
over the chairs, then over the floor camera), the Machine waking on live
current (92: the lamp-eye and the arcs over its casing), the breach; then sixteen
half-pulse cuts (R-117 only in parts) and the last pounce at the camera. 0:49.97
hard stop to silence. 0:51 sting, title, CHAPTER 2 · ENERGY RESEARCH, 0:53.3
COMING SOON, 0:55 CHAPTER 1 — FREE ON ITCH.IO joins it; 0:56.2 the card burns out;
0:57 the Machine's lamp-eye alone in the dark, 0:58 the floor goes live and shows
it, cut to black at 0:58.6, out at 0:59.5.

**Music and sound** (purchased packs only; nothing sent anywhere). Bed: "What
Eyes Can't See" (`O:/retro official/Audio/Tracks/ost_what_eyes_cant_see_mx_1_full.ogg`),
0:17.65-1:07.62 of the track, faded in over 3 s, placed so its drop (0:44.65)
lands on the trailer's 0:27.0 and its 0.6965 s pulse runs under the cuts (every
post-drop cut is on a pulse or half-pulse), cut dead at 0:49.97. Under the cold
open: Pure Horror `drone_long_ph_1`. Act 1 room tone: `old_machine_mx_1` (the
projector), `retro_computer_calculating_mx_1`, `static_loop_1`, the PA line
`containment_breach_risk_smx_1`, `sci-fi_frequencies_1`, distant impacts. Hits:
Cinematic vol. 1 `sfx_impact_mx_1/4/5/6/9/13/16/19/20/21`, `sfx_swell_mx_1`
into the drop; body impacts `impact_body_ufx_1/2` on the pounce and bite;
creature: `monster_roar_2/3/5/6/7`, `monster_talking_1/3`, `dog_hybrid_mx_1`,
`unknown_entity_distant_mx_2`; the Machine: `electric_shock_mx_1` and the game's
own `machine.wav`. Title: `sfx_stinger_mx_2` + `sfx_gong_mx_1`; the button (the Machine's tease):
the game's `machine.wav`, Pure Horror `stinger_ph_1` on the lamp-eye, `electric_shock_mx_1`
when the floor goes live, `sfx_impact_mx_16` on the cut to black; end card `drone_ph_2`. Every hit is aligned by its
loudest sample to its frame (`cue-sheet.txt`). Mixed with numpy at 48 kHz
stereo, look-ahead limited: -14.3 LUFS integrated, -1.0 dBTP, LRA 12 LU; digital
silence from the stop to the title.

**Scripts** (in `.build/promo-1.4/`, copies here): `shots.py` (the environment
and return takes), `film96.py` (the kill in 96, seven cameras on one take),
`film-machine.txt` (the Machine take), `look14.py`, `compose_ch2v2.py` (frames),
`score_ch2v2.py` (the mix and the cue sheet), `contact_ch2v2.py`,
`make_ch2v2.py` (stills, cover, banner, thumbnail, board), `cues_ch2v2.py` (the
cue sheet). To rebuild everything from a new exe (e.g. after an engine change to
bodies or lighting): copy it into `.build/promo-1.4/` and run
`sh rerender_all.sh EXE` there: it reshoots every take (environments, returns,
the kill in 96, the Machine), recomposes the frames, remixes, re-encodes both
mp4s and remakes the contact sheet, cue sheet, stills, key art and board.

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
