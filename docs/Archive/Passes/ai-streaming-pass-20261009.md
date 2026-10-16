# Escort streaming and GitHub recovery — October 9, 2026

Escorts can physically follow the player between aligned 24 m chunks, including
two-axis turns and non-sequential map IDs. Civilian Follow/Wait uses the same
engine path. Existing campaign encounters remain scripted unless explicitly
configured with `ACTOR_AI`; no campaign maps or assets were added.

Each autonomous actor has one runtime record with an immutable home identity
and a separate physical chunk/pose. Stable state keys, commands, health and
death survive migration. The renderer draws the runtime actor in its physical
chunk and skips its original timeline slot. Enemy targeting, shots and body
collision use that runtime record. Saves retain format version 13 and restore
the actor's current chunk without cloning its authored track.

Portal routing follows paired open boundaries or matching physically open
north/south transfer doors. Every movement step checks both chunks' support,
full hull, rails and door leaves. Closed transfers are never forced; unsafe
height differences stop the actor. The player can command an actor across an
open seam, and an actor approaching a player in the doorway stops clear of the
player's body. Current and next-hop geometry remain resident while following;
home geometry can unload. This also fixes a legacy neighbor-loading rule that
overwrote the NPC residency request.

Portal and local route searches share four searches per mechanisms tick.
Failed remote searches back off so inaccessible early actors do not starve
later actors or repeatedly load closed routes. Tests run streaming every frame,
cover both travel directions, mid-route save/load, destination commands/death,
paired transfer doors, invalid saved poses/chunks and nine deferred remote actors.

The unexpected GitHub commit `0ed45de` introduced a separate human/security
system without a verified Windows build. Its unsupported `Vec2` division caused
a Windows compilation error. It also duplicated the tested actor AI and had
targeting/shield defects. Recovery commit `dd3a0f0` retains that commit as a
merge parent while restoring the tested engine tree from `4df981e`. The push
was a normal fast-forward: the experiment remains recoverable in history.

This pass retains active-player-chunk enemy simulation and combat targeting.
It does not introduce chapter-wide background enemy simulation, arbitrary
overlapping portal spaces, automatic lift use or full GoldSrc behavior parity.
VR checks are offline regressions; live headset/controller behavior still
requires physical testing. This is an updated local build and source sync,
not a newly published binary release.

All 52 friendly-AI assertions passed. Physics/AI, world isolation, mechanisms,
campaign maps, saves, controls, offline VR and Vulkan smoke checks returned zero.
The Release packer verified all 185 embedded resources losslessly. The local
executable is 23,446,528 bytes, below the 28,000,000-byte limit.

Raw validation results are recorded in
[the evidence directory](ai-streaming-pass-20261009/). Existing scripted worker
encounter captures were inspected after the actor-rendering refactor; they are
not screenshots of a newly authored escort encounter.
