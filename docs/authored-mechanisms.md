# Shared authored mechanisms

`world/CampaignMaps.cpp` registers built-in campaign geometry. It contains map content, not a separate engine for an area. `CustomCampaign.cpp` parses external campaign content into the same `AuthoredMapData` representation.

`game/Mechanisms.cpp` updates authored moving cargo platforms and saved timelines. It does not reference campaign IDs, map numbers, or freight state names. The renderer draws a cargo cage whenever the active world authors a cargo lift. Native and custom content supply their own state IDs.

`ChunkDefinition.residencyGroup` keeps every chunk with the same nonnegative group loaded together. The existing nearby-chunk buffer and spatial door synchronization still apply. The optional eighteenth MAP field sets this group; omitted fields retain existing custom campaign behavior.

Custom payload extensions (all numbers use metres except timeline milliseconds):

```text
CARGO_LIFT|map|x1|y1|x2|y2|lower|upper|speed|callState|releaseState|positionState|downState|arrivedState|descendedState
TIMED_SEQUENCE|map|timerState|finishState|x1|y1|x2|y2|bottom|top|durationMs|finishAtMs|soundIntervalMs|soundUntilMs|soundId|gain|pitch
SEQUENCE_CUE|map|timerState|timeMs|soundId|x|y|gain|pitch|caption|absoluteZ
SIGN|map|x|y|absoluteZ|width|height|yawRadians|title|subtitle|accentRGBDecimal
```

The optional nineteenth MAP field sets the floor material: 0 industrial (default), 1 office carpet. Signs use absolute elevation and face along `{sin(yaw), cos(yaw)}`. Author their mounting structures separately; a sign is a surface, not collision geometry. Fixture models 15, 16 and 17 are a wooden cargo pallet, service van and wooden crate. The crate uses the original PSX Bunkers wooden_crate_8 model and texture and is available to native and custom campaigns through the same fixture record. Structure materials 8, 9, 10 and 11 are dark steel, opaque cab glazing, cargo wood and rusty steel.

The optional twelfth TERMINAL field names its prerequisite state. State names are campaign-authored identifiers; the existing state save codec persists platform positions and sequence timers. Platform position uses millimetres relative to the authored lower stop. Timelines start when the player enters the trigger volume, advance while that chunk remains resident, and set the completion state at the authored time. A zero sound interval disables the recurring sound.

The final `SEQUENCE_CUE` height field is optional. Supply absolute elevation for
sounds on an upper deck, in a shaft or attached to machinery. The original
ten-field format and caption position remain unchanged. Recurring sequence ticks
use their associated actor's position/contact height; without an actor they use
the trigger region's center and source floor. They do not use the player's position.

Structure materials 12, 13, 14 and 15 are weathered structural steel, hazard
stripes, route paint and industrial concrete. Material 12 retains the original
pressureworks steel texture's rust and scratches; it is not a flat color fill.
These IDs are available to native maps and custom campaigns through the shared
structure renderer.

Existing payloads remain valid. Invalid mechanism definitions are rejected during campaign discovery. `--mechanisms-test` verifies shared mechanisms in a custom campaign with different map IDs and state names, including residency, passenger movement, timeline completion and save/load. `--campaign-map-test` verifies the main campaign traversal; its old `--freight-district-test` command is retained as a diagnostic alias.

The optional twentieth MAP field chooses music: 0 original (default), 1 freight.
Music routes through authored metadata in native and custom campaigns and keeps its
playback cursor across chunk seams. CREATURE kind 4 is the mutated human, with
180 health and a 1.85 m standing hull. Save version 13 preserves this new kind;
versions 1–12 remain readable and receive newly authored encounters automatically.

Human guards use the shared actor AI rather than a campaign-specific NPC system.
The existing `ACTOR_AI` record accepts mode 3 for Guard; modes 0 (Scripted),
1 (Civilian) and 2 (Escort) retain their existing meanings. Define the actor and
its normal timeline first, then configure its AI and append patrol points:

```text
ACTOR_AI|map|actor|mode|enableFlag|speed|followDistance|dangerRange
ACTOR_GUARD|map|actor|sightRange|sightDot|recognitionMs|searchMs|alertFlag
ACTOR_PATROL|map|actor|x|y|absoluteZ|pauseMs
```

`ACTOR_GUARD` is optional: defaults are an 11 m sight range, a 0.65 sight-cone dot
threshold, 600 ms recognition delay and 4000 ms search duration. An empty alert
flag disables the authored alert output; an empty enable flag enables the AI
immediately. These configuration records do not change an actor's AI mode.
They require an existing actor whose final mode is Guard, including when records
are reordered. Guard configuration or patrol points on another mode are rejected.

Guards must use the Worker visual and cannot be moving platforms. Author 1–64
patrol points, visited in the listed order. Points use coordinates within the
guard's home chunk (0.25–23.75 m on each horizontal axis) and absolute elevation
(-100–100 m). Each point pauses for 0–60000 ms. Patrol routes remain in the home
chunk; they do not use escort chunk migration. A single point authors a stationary
post with the configured pause. Sight range accepts 1–30 m, the sight dot threshold
0–0.99, recognition 100–5000 ms and search 250–30000 ms. Existing actor records and
save version 13 remain compatible.
