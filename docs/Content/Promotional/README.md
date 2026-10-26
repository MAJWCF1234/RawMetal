# DepthWorks promotional assets

Made 2026-10-26 for the Chapter 1 playtest (v0.14.0) and the itch.io page at 1.0.0.
All frames are the game's own render (960x540, scaled 2x without smoothing),
captured with the states a player has on first arriving in each area.

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
| cover-630x500.png | 630x500 | itch.io cover (the yard soldier, title in the plant's green) |
| banner-1920x540.png | 1920x540 | page or store header (the rotor) |
| background-1920x1080.png | 1920x1080 | itch.io page background (the forecourt by torchlight, darkened) |

Fonts: Bahnschrift for the title, Consolas for the subtitles, in the terminal
green of the game's DEPTHWORKS mark (59,178,103).

To make more: `python tools/campaign/spot_views.py RawMetal.exe RUNDIR FIRST LAST`
renders six views per chunk into RUNDIR/.build/shot/sv_N_k.ppm; pick frames
and scale them 2x with nearest-neighbour. The game summary for the page is the
v0.14.0 section of RELEASE_NOTES.md.
