# Freight actors and shared machinery

Receiving now contains the first human encounter: a grounded 1.7 m radiation
worker, two animated wasp tracks, a hand-held pipe, a retreat and a persistent
death pose. The fight uses six retargeted skeletal clips from the local Universal
Animation Library: idle, walk, punch, hit, death and crouch. The original heavy
radiation suit supplies all 4,572 triangles, UVs and three textures. The OBJ is a
runtime rest-pose export; the original FBX remains in `src/assets/actors/source`.

This is authored content using the engine's shared actor timeline. There is no
Receiving-specific engine update or campaign-number branch in the mechanism
runner. Timelines persist in the existing named-state save store and continue
while their chunk remains resident. Physical platforms use the same sampled pose
for rendering, support, underside collision and passenger movement. Their motion
does not invalidate static Vulkan geometry caches.

## Teleport staging

`map receiving` and `map 13` now spawn at the actual overlook, at (7.6, 9.5,
-19), facing the worker. Closing the console resumes simulation and starts the
encounter. Natural arrivals still follow the corridor to the overlook. Existing
saves preserve the saved player position and sequence progress; a completed
encounter does not automatically replay on loading a save.

Spawn yaw and pitch are authored metadata, usable by custom campaigns. The
console regression test verifies that the encounter remains paused while the
console is open and starts after Escape closes it.

## Shared payload records

All coordinates are metres and key times are milliseconds. Actor indices are
zero-based in declaration order. Existing campaign payloads remain valid.

```text
ACTOR|map|timerState|visual|scale|loop|tool|index
ACTOR_KEY|map|index|timeMs|x|y|z|yawRadians|clip|phase
ACTOR_PLATFORM|map|index|width|depth|thickness
ACTOR_SUSPEND|map|index|absoluteTopZ
ACTOR_HEALTH|map|index|health|damageState|deadState
SEQUENCE_CUE|map|timerState|timeMs|soundId|x|y|gain|pitch|caption
```

Visual IDs are Worker=0, Wasp=1, Huntsman=2 and Cargo=3. Worker clips are idle=0,
walk=1, punch=2, hit=3, death=4 and crouch=5. A platform renders a steel deck
instead of its visual model. Timed sequences accept an optional final loop flag.
MAP accepts optional fields 21 and 22 for spawn yaw and pitch; pitch retains the
game's camera units, where radians equal pitch / 140.

The built-in receiving actors currently use an observational, fixed sequence;
they are not autonomous enemy AI. Captions accompany existing pain and impact
audio, rather than newly recorded spoken dialogue. Actor health is optional for
other authored encounters and is disabled for this fixed scene.

## Other content and rendering

Freight content calls the same system for warehouse cargo lifts, conveyor cargo,
a suspended crane crate and a rideable maintenance cradle. The section also adds
carts, a four-wheel forklift, rack markings, a printer and papers, platform
phones, depot overhead movement and damaged tunnel dressing.

Manifest glazing uses shared transparent structure material 17. Vulkan defers
transparent draws until all opaque chunks and actors have drawn, before clearing
depth for the weapon. Transparent batches are not yet sorted back to front.
Structure material 16 supplies organic blood staining.

Chunk storage now lives on the heap. This avoids exhausting the Windows native
stack when save previews nest complete game instances during validation.

Use `--console-test`, `--mechanisms-test`, `--campaign-map-test`,
`--world-isolation-test`, and `--smoke-test --vulkan` for behavioral validation.
`--freight-actor-inspection --vulkan` captures matched close and overlook views
at six sequence times. These rendered frames prove pose changes; a pixel
difference percentage is not a measure of art quality.

## Verified build

On 2026-10-01, the Release build passed console, mechanisms, campaign traversal,
world isolation and Vulkan smoke tests. Mechanism tests additionally cover
nearest actor hits and persistence of killed actor poses. The worker animation
test confirms six finite, grounded clips and interpolated hand attachments.
The lossless pack verifier confirmed all 165 resources; the executable is
21,825,536 bytes against the 22,000,000-byte limit.

Fresh Vulkan frames and a contact sheet are in
`diagnostics/freight-actors/worker-sequence-contact.png`. Matched overlook frames
at 1 ms and 4,500 ms differ in 2,244 of 230,400 pixels; at 23,000 ms they differ
in 3,782 pixels. These are animation checks at a fixed camera, not before/after
quality scores.
