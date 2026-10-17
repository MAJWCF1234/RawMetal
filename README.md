# RawMetal

RawMetal is a compact C++20 Windows game and engine with Vulkan rendering,
streamed worlds, embedded runtime assets, and a hard **32 MiB executable ceiling**.
The current tester target is **v0.7.2-preview.7 / ASHFALL EXCLUSION ZONE — performance hotfix**.
The latest controlled three-run comparison improves average FPS **160.6 → 186.5**
and 1% lows **94.6 → 107.7**, with exact state/save comparisons and zero changed
pixels across twelve encounter views. This is a 640 × 360 lab average, not a
minimum frame rate. See the [round-three results](docs/Performance/Competitive%20Round%203%2020261017.md).
See the consolidated [release notes](RELEASE_NOTES.md).
Docs vault: open [`docs/`](docs/Home.md) in Obsidian. The
[native walkthrough notes](docs/Content/Exclusion/exclusion-preview15-playtest-20261015.md)
cover readability changes and the physical north/east approach checks.

## Start the Exclusion playtest

Run `RawMetal.exe`, choose desktop play, then **Custom Maps -> ASHFALL EXCLUSION ZONE**.
For a direct desktop launch:

```text
RawMetal.exe --pc --exclusion-zone
```

The executable includes the campaign. The direct launch uses
`custom maps/RawMetal-Ashfall-Exclusion-Zone.txt` when installed and otherwise loads
the embedded fallback. A loose asset folder or map download is unnecessary for
the embedded playtest.

This pass develops the same three connected chunks: **Forward Facility, Scrap
Basin, and South Wash**. The six existing Cleanup roles remain Assault,
Sharpshooter, Rusher, Heavy, Engineer, and Medic. Existing patrol, hearing,
awareness, firearms, support, and movement systems drive the encounter. No new
areas or animation banks are added.

The existing **Receiving 02** campaign area hosts a doomed six-role containment
detail, a field cache and a chain distraction. After two player kills, real
entities overrun its fighters while one survivor retreats into Receiving 03 and
closes the pressure door. This is an early disaster; other Cleanup squads remain
part of the facility. Reach it on the main route, or use console `MAP 14` for a
direct test. See the [breach validation](docs/Content/Exclusion/receiving-breach-20261015.md).

The six Cleanup roles and Witness now have **80 embedded ElevenLabs voice takes**,
with equipment-specific mask effects and contextual patrol, combat, support and
escort responses. [Voice details and audition](docs/Content/Cleanup/cleanup-voices-20261015.md).

The route starts behind the west cover screen:

1. Use the **arrival cache** to read the brief and collect its flashlight. Press
   **F** to toggle it.
2. Use the **north scrap rig** to draw the north patrol toward the clatter. Crouch
   around the screen's south end for a rear approach, or accept a normal fight.
3. Enter the facility and secure the **field relay**. It authorizes the east
   winch and lets the waiting witness follow or wait.
4. Cycle the **east winch** and use the staging screen's west end to approach
   its distracted patrol.
5. Follow the south connection into **South Wash** and register departure at its
   relay. Escorting the witness is optional; Scrap Basin remains an optional
   salvage and combat route.

Objectives, terminal interlocks, and checkpoints track these steps. Terrain
anchors keep local controls and cover on their actual ground. The flashlight
uses the existing beam and lighting, with a small gun or forearm housing.
The reactor's existing floppy puzzle also has clearer insertion and valve-order
prompts. [Encounter authoring notes](docs/exclusion-encounter-slice-20261015.md)
describe the route and its bounded script records.

## Desktop controls

| Input | Action |
| --- | --- |
| WASD / mouse | Move / look |
| Shift / Space / C or Ctrl | Sprint / jump / crouch |
| Left click | Fire shotgun, punch with fists, or punt held junk |
| Right click with fists | Guard |
| R | Reload the six-round shotgun tube |
| Mouse wheel up / down | Select shotgun / fists |
| E | Use doors, terminals, items, and friendly commands |
| Q behind an unaware hostile guard | Nonlethal knockout |
| E behind an unaware hostile guard | Grab and hold the guard in front |
| Fire with fists from behind | Existing lethal rear melee |
| E / Q / Fire while holding a guard | Release / knock out / kill |
| F | Toggle an acquired flashlight |
| I | Inventory, equipment, and stored first aid |
| Esc | Pause, settings, save/load, or back |
| Arrow keys / Enter | Navigate / accept menu choices |
| Backtick | Developer console |

Rear actions require reach, clear space, and an unaware target. Holding slows
movement and occupies the hands. The held body physically blocks shots that
reach it; ordinary guards avoid firing through a teammate and may reposition.
Unconscious guards remain alive and stop acting. Loose junk can be lifted,
dropped, thrown, placed on supported surfaces, and carried between chunks.

## Saves and menus

**Esc -> Save / Load** provides three manual slots and an autosave entry.
Exclusion writes checkpoints after its cache, relay, protected staging, and
departure steps. Saves retain world identity, objectives, inventory, machinery,
NPC state, unconscious bodies, and a held guard. Loading does not replay one-shot
item awards or sounds. Invalid saves leave the current session intact.

```text
%LOCALAPPDATA%\RawMetal\saves
```

An installed custom campaign is needed to load its corresponding custom save;
the embedded Exclusion campaign remains available in a standalone executable.
Supported older saves are migrated where possible.

## Running and PCVR

Windows 10/11 x64, the Microsoft Visual C++ x64 runtime, and a compatible graphics
driver are required. Vulkan hardware rendering is the default; the driver
supplies its Vulkan runtime. No Vulkan SDK is needed to play. Use
`RawMetal.exe --pc --software` for the CPU renderer. `RawMetal-renderer.txt`
records the renderer or fallback reason. See [Windows compatibility](docs/windows-compatibility.md).

Double-click the executable to choose PC or VR play. `--pc` bypasses the chooser;
`--vr` launches the SteamVR preview. VR requires SteamVR and a connected headset.
Tracked hands, grip interaction, shoulder weapon stow, physical fists, and the
wrist computer use the existing PCVR path. The new Q/E rear-action guide above
describes desktop input. See [SteamVR setup and controls](docs/vr.md) for controller
bindings and the remaining headset playtest limits.

The main industrial campaign and the wider Ashfall world remain available.
The console supports `maps`, `map exclusion`, `map reactor`, `map coast`,
`give flashlight`, `fps`, and `r_scale 50`, `75`, or `100`. Map commands start a
fresh map session; opening the console pauses gameplay.

## Build and validate

Build on Windows x64 with Visual Studio 2022 C++ tools, CMake 3.20+, and a Vulkan
SDK containing `glslc`:

```text
Build.cmd
```

The Release build produces `RawMetal.exe` in the repository root, with shaders
and runtime assets embedded. The build rejects an executable at or above
**33,554,432 bytes (32 MiB)**. That ceiling is unchanged. Current binary size and
test totals must come from the fresh follow-on verification; earlier release
figures in the changelog are historical.

Targeted diagnostics include:

```text
RawMetal.exe --exclusion-test
RawMetal.exe --exclusion-authoring-test
RawMetal.exe --stealth-test
RawMetal.exe --exclusion-inspection
RawMetal.exe --flashlight-test
RawMetal.exe --reactor-test
RawMetal.exe --save-test
RawMetal.exe --controls-test
RawMetal.exe --guard-ai-test
RawMetal.exe --smoke-test --vulkan
RawMetal.exe --vr-test
```

Exclusion inspection writes native encounter captures under `.build/exclusion-preview/`.
Automated logic checks and captures have separate roles; headset/controller
comfort still requires a live PCVR playtest.

## Maps, editor, and source

`LevelEditor.cmd` opens the browser editor. Runtime custom campaigns use one
UTF-8 TXT payload, installed with `InstallMap.cmd <path-to-map.txt>` or by
dragging it onto the installer. **CUSTOM** installs under `custom maps/` and
appears on the title screen without recompiling. **MAIN** injects a supported
built-in slot and rebuilds. The payload's default target prevents accidental
cross-installation.

- [Map format and installation](MAP_SYSTEM.md)
- [Level Editor guide](tools/level-editor/README.md)
- [World ownership and isolation](docs/world-system-boundaries.md)
- [Asset provenance](docs/ASSET_SOURCES.md)
- [Release history and current follow-on work](RELEASE_NOTES.md)

`src/game/`, `src/world/`, `src/renderer/`, and `src/audio/` contain the runtime;
`src/assets/` contains embedded source assets and `tools/` contains authoring and
import utilities. Generated diagnostics and build intermediates are local output.
