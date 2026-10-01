# Shared authored mechanisms

`world/CampaignMaps.cpp` registers built-in campaign geometry. It contains map content, not a separate engine for an area. `CustomCampaign.cpp` parses external campaign content into the same `AuthoredMapData` representation.

`game/Mechanisms.cpp` updates authored moving cargo platforms and saved timelines. It does not reference campaign IDs, map numbers, or freight state names. The renderer draws a cargo cage whenever the active world authors a cargo lift. Native and custom content supply their own state IDs.

`ChunkDefinition.residencyGroup` keeps every chunk with the same nonnegative group loaded together. The existing nearby-chunk buffer and spatial door synchronization still apply. The optional eighteenth MAP field sets this group; omitted fields retain existing custom campaign behavior.

Custom payload extensions (all numbers use metres except timeline milliseconds):

```text
CARGO_LIFT|map|x1|y1|x2|y2|lower|upper|speed|callState|releaseState|positionState|downState|arrivedState|descendedState
TIMED_SEQUENCE|map|timerState|finishState|x1|y1|x2|y2|bottom|top|durationMs|finishAtMs|soundIntervalMs|soundUntilMs|soundId|gain|pitch
SIGN|map|x|y|absoluteZ|width|height|yawRadians|title|subtitle|accentRGBDecimal
```

The optional nineteenth MAP field sets the floor material: 0 industrial (default), 1 office carpet. Signs use absolute elevation and face along `{sin(yaw), cos(yaw)}`. Author their mounting structures separately; a sign is a surface, not collision geometry. Fixture models 15 and 16 are a wooden cargo pallet and service van. Structure materials 8, 9, 10 and 11 are dark steel, opaque cab glazing, cargo wood and rusty steel.

The optional twelfth TERMINAL field names its prerequisite state. State names are campaign-authored identifiers; the existing state save codec persists platform positions and sequence timers. Platform position uses millimetres relative to the authored lower stop. Timelines start when the player enters the trigger volume, advance while that chunk is active, and set the completion state at the authored time. A zero sound interval disables the recurring sound.

Existing payloads remain valid. Invalid mechanism definitions are rejected during campaign discovery. `--mechanisms-test` verifies shared mechanisms in a custom campaign with different map IDs and state names, including residency, passenger movement, timeline completion and save/load. `--campaign-map-test` verifies the main campaign traversal; its old `--freight-district-test` command is retained as a diagnostic alias.

The optional twentieth MAP field chooses music: 0 original (default), 1 freight.
Music routes through authored metadata in native and custom campaigns and keeps its
playback cursor across chunk seams. CREATURE kind 4 is the mutated human, with
180 health and a 1.85 m standing hull. Save version 13 preserves this new kind;
versions 1–12 remain readable and receive newly authored encounters automatically.
