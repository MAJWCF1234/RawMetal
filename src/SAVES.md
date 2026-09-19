# Save slots and reactor geometry repairs

The Esc menu retains its settings and adds Save Game / Load Game, each with
three slots and a Back row. Mouse and keyboard use the same hit regions.
Overwrite and load confirmation default to Cancel. Esc backs out one page;
only Esc on the root page resumes play. Save does not advance simulation.
Successful load resumes with held-click fire suppression and a new audio-session
revision, including when loading a save from later in the campaign clock.

Slots live in `%LOCALAPPDATA%\RawMetal\saves\slot-N.rms`. The versioned text
format writes explicit scalar fields with round-trip float precision: player,
inventory, motion/cooldowns, all chunks' enemies, pickups, clutter, kills, doors,
lift phase/height/velocity/timer and reactor authorization state. Geometry and
pointers are never serialized. Local audio/input preferences are retained.

Saves use a checked temporary write and same-volume replacement. Parsing is
bounded to 2 MiB with capped collections, finite numbers, range checks and a
payload checksum. Loading builds a separate candidate and only replaces the
live game after validation. The checksum detects corruption, not malicious
editing/authenticity. Future format versions are rejected explicitly.

`--save-test` exercises exact state roundtrips at seven ride times, continued
simulation, puzzle completion after load, inactive chunks, held clutter,
corruption/truncation/version/range failures, menu navigation, overwrite/load
confirmation, failed-write preservation, three slots and input suppression.
Tests use a unique diagnostics-local directory, never the player's save folder.

Map 3 no longer generates redundant rails/backing panels along existing solid
outer walls. Tall rail infill is recessed from the frame; posts end beneath the
cap instead of sharing faces. Reactor stairs have an explicit material tag,
consistent concrete bodies and inset anti-slip treads. The elevator dispatch
sign mounts on a steel panel connected to the lower wall and header.
`--repair-inspection` renders walls, both stair approaches, the mounted sign and
Esc menu pages; `--lift-test` covers stair traversal and duplicate-wall guards.

Verification on this revision: save-slot tests, full software/Vulkan smoke suites,
Vulkan material/depth tests and audio-transition tests passed. Synchronization
validation logged no errors. Visual inspection confirmed clean outer walls,
separated rail faces, consistent stair surfaces and the mounted sign.
The 1,560-frame performance run had two frames above 50 ms (51.2 and 73.4 ms),
so it did not meet the strict minimum-frame-rate gate. All six culling/reference
images were identical. This revision does not claim the earlier intermittent
rendering hitch is fixed; its CPU/GPU split is recorded in the benchmark report.
