# RawMetal v0.6.2 — Freight Worker Encounter

Receiving now stages the first human encounter. The radiation worker wanders
until the player looks at him from the overlook. After a three-second delay,
he walks to the fight spot and is attacked by two wasps. Faster duck, strike,
hit and death animations use the supplied skeletal animation library. He turns
toward the active bug during each strike, carries his pipe using animated hand
anchors, and leaves a persistent body behind the cargo.

The supplied `worker dying.wav` starts once at the first attack, 6.2 seconds into
the encounter. The complete 9.43-second clip is embedded unchanged. Shared audio
decoding supports its IMA ADPCM format and resamples it for the existing mixer.

`map receiving` and `map 13` place the player at the overlook facing the worker.
Close the console with Escape to resume play. Looking away delays the trigger.
The wander clock, approach origin and encounter progress survive save/load, and
the sequence continues across resident Receiving chunks. Completed saved
encounters do not automatically replay.

Shared authored actor timelines, sight triggers, facing targets, repeated
animation cycles, spatial cues and moving platforms support native and custom
campaigns. Freight content uses them for cargo lifts, conveyors, suspended
crates, a rideable cradle and distant creature movement. The section also adds
a four-wheel forklift, carts, rack signs, office props and platform phones.
The worker fight remains a staged observational sequence rather than autonomous
NPC combat AI.

Vulkan rendering improvements include corrected world-space material lighting,
better roughness behavior for distant normal maps, weathered steel beams, and
transparent draws deferred until the opaque world and actors have drawn.
Manifest offices gain glass partitions. Transparent batches are not yet sorted
back to front. Chunk storage moves off the native stack to avoid exhaustion
during nested save previews.

No maps were added. Existing save format 13 remains in use. The release remains
a self-contained Windows executable under the 22,000,000-byte budget.
The executable is 21,956,096 bytes. All 166 embedded resources pass exact
verification, including the original worker WAV.

Validation covers the console teleport, sight trigger, observation delay,
continuous saved approach, both strike facing targets, one-shot voice timing,
compressed voice decoding, shared custom-campaign records, campaign traversal,
world isolation and the Vulkan smoke suite. Fresh Vulkan captures and fixed
camera pixel comparisons verify visible staging and animation changes.
