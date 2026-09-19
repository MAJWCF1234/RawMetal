# Surface lift and reactor

## Existing game analysis

RawMetal is a native C++ game with a software 3D renderer, embedded purchased
assets and three previously connected chunks: Foundry, Pressure Works and
Turbine Gantry. Each chunk has a 24 by 24 tile footprint. Gantry already supports
multiple walkable spans at one XY through MapLayer decks and Staircase geometry.
Movement and enemy navigation can therefore support stacked maps, but the old
span collector, door rendering and terminal collision assumed nonnegative floors.
Chunk persistence retains gameplay state while releasing static geometry.

The supplied proposal was a useful narrative outline, but it did not implement
a moving lift room, several rows were not 24 cells wide, upper map arrays did not
define lower-map walls, and the gantry still ended the game. The revised direction
uses full map floors stacked vertically, with a continuous central shaft.

## Implemented layout

The next chunk contains seven named map layers. Cab dispatch begins at the collar
(0 m), rises three storeys to +9 m, pauses at sealed surface gates, then plunges
six storeys to -9 m. The two bottom maps (-6 m and -9 m) form the reactor complex.
This uses the user's clarified meaning of a layer: a full map floor, not one
small platform. Decks, room partitions and the central shaft share world space.

The lift is a 4 by 4 m room with a floor, ceiling, side walls, barred windows and
two doors. Its north door is open for boarding; both doors close during travel;
the south emergency exit opens after impact. The player keeps first-person look
and movement within the room. The cab and loose floor objects move together.
The fall does not apply unavoidable fatal damage. Restart resets the sequence;
unloading and rebuilding the chunk preserves its phase and height.

The reactor's east service staircase connects the two bottom map floors. Combat
and the completed descent gate the lower exit. That exit finishes the currently
implemented content; the separate future core-vessel level is not yet authored.

## Assets

Existing imported assets from the requested library provide the PSX Texture
materials, PSX Bunkers control cabinets, computers, generators and clutter,
Modular Retro FPS Kit lamps, and pump/compressor props. Their source records
remain in assets/materials, assets/facility and assets/pressureworks.

The main OST fades out during ascent as the lift motor takes over. At the jam,
a strained creak precedes the cable snap; the impact is a separate cue. Echoes'
Pitch Black loop fades in after the crash. Effects come from ROT and Rust & Blood.
Music uses MP3 storage and the already bundled miniaudio decoder to preserve
the executable size limit. See assets/audio/SOURCES.md for exact source entries
and conversions. Source archives are unchanged.

## Verification commands

Run from diagnostics: `../RawMetal.exe --lift-test`, `--lift-inspection` and
`--smoke-test`. The focused test checks boarding via the real movement controller,
E dispatch, the full rise/jam/fall sequence, passenger support, crash sound,
reactor stairs, persistent state and 30/60/120 Hz behavior. Inspection renders
five stages of the same room and both reactor floors without screen capture.
`--lift-audio-test` produces a 17-second mixed preview and checks the envelopes,
cue sequence, independent music/effects controls and restart behavior.
`--surface-lift` starts a playable game at the new chunk for direct review.

## Rendering and developer access

The enclosed cab has split sliding gates, broad barred inspection windows,
recessed panels, handrails, threshold hazard tape, an overhead hoist and a
side-mounted dispatch control. Snapped cables retract above the falling room;
the opposite gate opens after impact. The collision hull carries the passenger.

Vulkan 1.0 hardware rendering replaces per-pixel CPU rasterization in normal
play, retaining the software path for fallback and reference tests. Full 640x360
is restored as the default; performance does not depend on shortening the visible
shaft or reducing the number of maps. Opaque decks conservatively occlude complete
projected bounds; lights are spatially indexed and static shadow work is bounded.
Animation caches triangulation and boundary rings instead of rebuilding them.

Press backtick for `map lift`, `map reactor`, `maps`, `where` and `fps`.
Run `--console-test`, `--vulkan-test` and `--performance-test` from diagnostics.
The hardware test checks depth, alpha cutouts, emission, normal-map response,
view-model depth isolation and near-plane clipping. The benchmark records cold
startup separately and includes live gameplay updates and audio control.
