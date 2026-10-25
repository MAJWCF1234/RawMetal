# Switchable lamps and DarkZone validation

Validated in the feature worktree on 2026-10-25, after preserving the inherited
changes in WIP commit `39993ca` and merging local `main` (`16c4d5c`). No campaign
builders were edited for this feature. The fixture is constructed in the test.
`ref-lighting.txt` was absent from both the worktree and the main checkout root;
cache hooks were reviewed directly in the renderer sources.

## Authoring

`WorldLight` uses `enabledState`, `enabledValue=1`, `invert=false`, `period=0`,
`onTime=0`, `phase=0`, and `flicker=0`. Its state predicate is
`(enabledState == 0 || state(enabledState) == enabledValue) != invert`.
For a positive period, the phase-adjusted time modulo period must be below
`onTime`. Negative phases wrap correctly. Flicker is deterministic, irregular
off-blinks and brownouts; power multiplies the authored intensity.

Add `DarkZone` entries to `AuthoredMapData::darkZones`. Fields are `x1`, `y1`,
`x2`, `y2`, `bottom=-1000`, `top=1000`, `state=0`, `value=1`, `invert=false`,
and `jamFlashlight=true`. The same state predicate applies. Bounds use local
chunk coordinates and the player's eye, inclusive below and exclusive above.
Only the current chunk's zones determine darkness. Lamps remain independent.

Custom campaign records (optional trailing fields may be empty):

```text
LIGHT|mapIndex|x|y|z|mount|yaw|intensity|range|kelvin|tint|state|value|invert|period|onTime|phase|flicker
DARK|mapIndex|x1|y1|x2|y2|bottom|top|state|value|invert|jam
```

Here LIGHT `state/value` map to `enabledState/enabledValue`, and DARK `jam`
maps to `jamFlashlight`. Timing, colour, and bounds must be finite;
`period >= 0`, `0 <= onTime <= period`, `0 <= flicker <= 1`; zone boxes must
be nonempty and overlap the map. Existing states provide save persistence.

## Implementation checks

Lamp GPU identity excludes intensity. Power updates only the intensity records,
not grid membership or shadow dirtiness. Vulkan feature maps retain this path
under `--nopixel`; software scales cached per-lamp contributions. Actor probes,
dynamic geometry, fixture glow, and fog shafts use current power. Darkness
removes ambient/floors, haze colour, and additive grade lift; exposure history
is clamped to the current ceiling, reaching 1.0. The flashlight sputters for
1.5 seconds, then reports no power until the jamming zone is left or disabled.
Player flashes last about 0.12 seconds with a faint tail and a 12 m radius;
the two strongest NPC weapon flashes use the lamp storage buffer.

## Rendered measurements

640 x 360 frames; mean Rec.709 luminance on an 8-bit 0-255 scale. Vulkan ran on
NVIDIA GeForce RTX 5060 Ti with linear HDR and 4x MSAA. Tests explicitly fail
if Vulkan falls back to software during the GPU evidence run.

| Frame | Vulkan | Software |
| --- | ---: | ---: |
| Lamp off on first frame | 13.5312 | 3.39813 |
| Lamp switched on | 35.8139 | 4.43563 |
| Lamp switched off again | 13.5280 | 3.39868 |
| Active darkness, lamps off | 0 | 0 |
| Active darkness, lamp on | 19.1669 | 1.42670 |
| Same dark view, player muzzle flash | 14.1317 | 16.2904 |
| Flash expired | 0 | 0 |
| Jammed flashlight | 0 | 0 |

The creature-only screen region (x=475..519, y=160..234, above the floor) had
mean/peak luminance 0/0 in darkness, then 7.34561/131 under the Vulkan shot
(software: 8.99946/119). Static geometry builds stayed **1 -> 1** and lamp
shadow faces **12 -> 12** across lamp toggles, darkness, flashes, and recovery.
The `--nopixel` run produced the same Vulkan results and cache counts.

Restoring the torch brightened 64,903 Vulkan pixels by more than 5/255, with
peak delta 87.2768. Software brightened 3,236 pixels, peak delta 23.6258.
These compare torch-off/on renders at the same game time, isolating the beam
from ambient recovery and animation. Captures were also visually inspected:
the creature disappears in darkness and returns under the flash.

At flicker 0.6, a deterministic 6,000-frame trace had 4,417 fully lit frames,
775 dim frames, 808 off frames, and 253 off transitions. Darkness reached
0.333333 after 0.1 s and 0.833333 after 0.25 s. Flash strength was 2.38333 at
0.01 s, 0.216667 at 0.11 s, 0.0484 at 0.21 s, and zero at 0.36 s.

Local evidence is retained in `.build/darkness-vulkan/`,
`.build/darkness-nopixel/`, and `.build/darkness-software/`; generated frames
and raw reports are not committed. Run `RawMetal.exe --darkness-test`,
`--darkness-test --nopixel`, or `--darkness-test-software` to reproduce.
The Vulkan test is included in `--each-test` (28 tests, all passed).

The prescribed PowerShell `Build.cmd` build passed (UTF-16 `build.log`),
producing a 46,608,384-byte worktree executable. `--each-test`,
`--smoke-test`, `--darkness-test --nopixel`, and `--darkness-test-software`
all exited 0 on that executable. The smoke test used the custom-map file
copied from the main checkout into the worktree. `git diff --check` passed.
