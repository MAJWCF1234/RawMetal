# RawMetal

RawMetal is a compact C++20 game and engine project built around a self-contained Windows executable, a native Vulkan renderer, streamed modular worlds, and a deliberately small release footprint.

The current release is **v0.5.7, Ashfall Coast**. The published game remains a single executable with its runtime assets embedded.

[Download the latest release](https://github.com/MAJWCF1234/RawMetal/releases/latest)

## Current release

v0.5.7 expands Ashfall from twelve streamed surface chunks to fifteen by adding a connected eastern coastline with:

- three new coastal chunks
- walkable rock and sand terrain
- a real submerged seabed beneath the water
- coastal water, materials, sky, and atmospheric tuning
- save-compatible chunk expansion that preserves the original Ashfall chunk IDs
- dedicated seam, traversal, renderer, save, and console validation

Recent renderer work also added restrained contact shading, improved material response, parallax relief, filmic color handling, bright-pixel bloom, square-fixture volumetric shafts, world-space barrel fire and smoke, and depth-aware camera focus.

See [RELEASE_NOTES_v0.5.7.md](RELEASE_NOTES_v0.5.7.md) for the current release notes.

## Download and run

Download **RawMetal.zip** or **RawMetal.exe** from [GitHub Releases](https://github.com/MAJWCF1234/RawMetal/releases/latest).

The release executable is self-contained. No loose asset package, shader folder, or runtime SDK is required.

Windows x64 is currently the supported platform.

Vulkan hardware rendering is the default. Use:

\`\`\`text
RawMetal.exe --software
\`\`\`

to force the CPU renderer.

The renderer targets Vulkan 1.0 without vendor-specific extensions. The graphics layer is portable in design, but the current window, input, and audio platform layer is Win32.

## Main campaign

The built-in campaign is organized as streamed map chunks and multi-floor spaces.

Current campaign areas include:

| Index | Area |
| --- | --- |
| 0 | Foundry |
| 1 | Pressure Works |
| 2 | Turbine Gantry |
| 3 | Surface Lift / Reactor Complex |
| 4 | Reactor Service Gallery |
| 5 | Coolant Return |
| 6 | Cable Vaults |
| 7 | Pump Annex |
| 8 | Utility Junction |
| 9 | Waste Handling |

The Surface Lift sequence links the industrial upper facility to the lower reactor complex through a continuous animated shaft. The reactor section includes a multi-stage authorization puzzle, hostile encounters, and a dedicated Reactor Stalker.

The later service maps add water basins, buoyant debris, electrical hazards, machinery, observation routes, utility connections, and the Waste Handling conveyor and compactor system.

Campaign chunks stream through the same world system used by the rest of the game. Carried and thrown clutter can cross adjacent chunk seams with the player.

## Ashfall

Ashfall is the outdoor terrain test world and now consists of fifteen connected chunks.

The original twelve chunks form a 4 x 3 wasteland region. v0.5.7 adds a fifth eastern column containing:

- north headland
- tidal shelf
- south bluffs

Ashfall uses shared Surface Nets terrain with continuous seam heights, world-space terrain UVs, slope-aware player and creature movement, and separate world identity from the main campaign.

The coast reuses the existing terrain and water systems rather than introducing a separate outdoor renderer. Sand, pale coastal rock, seawater, and the coastal sky are derived or generated at runtime to preserve the executable size budget.

Use the developer console command:

\`\`\`text
map coast
\`\`\`

to jump directly to the middle coastal chunk.

## Controls

| Input | Action |
| --- | --- |
| WASD | Move |
| Mouse | Look |
| Left click | Fire or punt held junk |
| R | Reload shotgun |
| Mouse wheel up | Select shotgun |
| Mouse wheel down | Select fists |
| E / Enter | Interact, operate, equip, place, or pick up |
| F | Toggle flashlight after acquiring it |
| I | Inventory |
| Esc | Pause, settings, save/load, or back |
| Backtick | Developer console |

The shotgun uses a six-round tube. Inventory supports equipment, ammunition, first aid, and quest items.

Loose junk can be lifted, dropped, thrown, placed on supported shelves and surfaces, and carried across streamed chunk boundaries. Objects use material-specific impact audio and simple rigid-body behavior.

## Rendering

RawMetal renders at a fixed **640 x 360 internal resolution** and keeps the HUD full resolution.

The Vulkan path currently supports:

- depth-tested triangle rendering
- embedded textures
- normal maps
- emission maps
- relief and parallax sampling
- cached static lighting
- budgeted shadow sampling
- material batching
- static geometry caching
- conservative occlusion
- atmospheric extinction
- square-fixture volumetric light shafts
- gloss and specular response
- filmic color mapping
- bright-pixel bloom
- depth-aware camera focus
- world-space fire and smoke effects
- animated transparent water
- underwater presentation effects

The software renderer remains available as a fallback and for comparison testing.

\`RawMetal-renderer.txt\` records the selected GPU or the reason hardware rendering fell back.

## World and save architecture

Campaign, Ashfall, and runtime custom campaigns have separate world identities and world-space chunk origins.

World queries convert through world space instead of assuming that chunk IDs form a single linear strip. This allows Ashfall to form a real 2D streamed grid while retaining the campaign's authored layout.

The save system stores chunk ownership and world identity. Current saves preserve campaign state, inventory, enemies, doors, clutter, lift state, reactor state, and custom world progress. Older supported save versions are migrated when possible.

Save files are stored under:

\`\`\`text
%LOCALAPPDATA%\RawMetal\saves
\`\`\`

Invalid saves report an error without replacing the current session.

## Level Editor

Run:

\`\`\`text
LevelEditor.cmd
\`\`\`

from the repository root.

The browser-based editor is designed around floor-plan and building-design workflows instead of exposing raw engine transforms as the primary interface.

It includes:

- 2D floor-plan editing
- multi-floor buildings
- named floors and ceiling heights
- room drawing
- straight wall drawing
- smart doors
- stairs that connect floors
- floor, wall, and ceiling object placement
- prefab placement using real game assets
- material painting with real-world repeat size and alignment
- 3D preview
- section views for vertical editing
- object arrangement workspaces for placing items on, under, and around furniture or machinery
- nested arrangements
- direct Save / Open
- undo and redo
- local recovery autosaves
- multi-chunk layouts
- installer-ready map export

The editor intentionally hides much of the engine-specific complexity from artists. RawMetal remains authoritative for final rendering, collision, lighting, animation, and gameplay.

See [tools/level-editor/README.md](tools/level-editor/README.md) for the full editor guide.

## Custom maps

RawMetal supports runtime custom campaigns without rebuilding the game.

The portable map format is a UTF-8 \`.txt\` payload. Install one with:

\`\`\`text
InstallMap.cmd <path-to-map.txt>
\`\`\`

or drag the TXT file onto \`InstallMap.cmd\`.

The installer supports two targets:

**MAIN**

Injects a supported dynamic map slot into the built-in campaign and rebuilds RawMetal.

**CUSTOM**

Installs the campaign under \`custom maps/\` without modifying \`World.cpp\` and without recompiling the game. Valid campaigns appear under **CUSTOM MAPS** on the title screen.

A Level Editor export can contain an entire multi-map custom campaign, including all plan areas and floors, in one TXT file.

\`META_DEFAULT_TARGET\` is enforced so a CUSTOM payload cannot accidentally overwrite a built-in campaign slot.

See [MAP_SYSTEM.md](MAP_SYSTEM.md) for the complete payload format, runtime campaign schema, MAIN injection rules, and legacy payload conversion behavior.

## Developer console

Press **backtick** to open the console.

Useful commands include:

\`\`\`text
help
maps
map foundry
map pressureworks
map gantry
map lift
map reactor
map cable
map annex
map junction
map waste
map wasteland
map coast
reload
where
fps
give flashlight
r_scale 50
r_scale 75
r_scale 100
clear
\`\`\`

Map commands start a fresh map session. Gameplay pauses while the console is open.

## Building from source

Requirements:

- Windows x64
- Visual Studio 2022 C++ build tools
- CMake 3.20 or newer
- Vulkan SDK with \`glslc\`

Run:

\`\`\`text
Build.cmd
\`\`\`

The build produces the canonical \`RawMetal.exe\` in the repository root.

Shaders are compiled to embedded SPIR-V. Runtime textures, models, audio, and other game data are losslessly packed into the executable.

The build is intentionally size-constrained and rejects a release executable at or above **19,800,000 bytes**.

Purchased source assets remain unchanged. Runtime packing selects compact lossless representations and verifies embedded asset data against source pixels or bytes.

## Validation and diagnostics

RawMetal includes targeted tests and inspection modes for gameplay, rendering, world seams, saves, and content.

Common checks include:

\`\`\`text
--smoke-test
--vulkan-test
--performance-test
--physics-ai-test
--world-isolation-test
--service-map-test
--campaign-extension-test
--flashlight-test
--stalker-test
--hazmat-test
--ashfall-inspection
--coast-inspection
--campaign-inspection
--service-inspection
--water-wall-inspection
--shading-inspection
--terrain-seam-inspection
\`\`\`

Most visual inspection modes can be combined with \`--vulkan\` to capture the hardware-rendered path.

The performance test records update, audio, render, and frame timing across representative scenes. Results are machine and workload dependent and should not be treated as universal hardware requirements.

## Repository layout

\`\`\`text
src/
  audio/        audio engine and decoding
  game/         gameplay, movement, AI, saves, scripts, streaming
  renderer/     software and Vulkan renderers, shaders, performance tests
  world/        built-in worlds and runtime custom campaigns
  assets/       embedded source assets and provenance records
  tools/        build-time import and packing tools

tools/
  level-editor/ browser level editor
  InstallMap.ps1
  ConvertMapPayload.ps1

custom maps/    installed runtime custom campaigns
docs/           architecture and design documentation
\`\`\`

Build intermediates and generated diagnostics are not intended to be committed as source.

## Asset provenance

RawMetal uses documented supplied and purchased asset libraries alongside original runtime code and generated data.

Asset provenance is documented in:

- [docs/ASSET_SOURCES.md](docs/ASSET_SOURCES.md)
- [src/assets/materials/ASHFALL-SOURCE.md](src/assets/materials/ASHFALL-SOURCE.md)

The project keeps source asset provenance separate from runtime packing and does not require players to install the original asset libraries.

## More documentation

- [MAP_SYSTEM.md](MAP_SYSTEM.md) for map injection and runtime custom campaigns
- [tools/level-editor/README.md](tools/level-editor/README.md) for the Level Editor
- [docs/world-system-boundaries.md](docs/world-system-boundaries.md) for world ownership and isolation
- [RELEASE_NOTES_v0.5.7.md](RELEASE_NOTES_v0.5.7.md) for the latest release
