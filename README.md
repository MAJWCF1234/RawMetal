# RawMetal

Campaign and Ashfall now have separate world identities, chunk origins and save
ownership. See [world-system boundaries](docs/world-system-boundaries.md) for the
isolation fix, reusable spawn/effect/trigger records, tests and remaining legacy
code. Custom Maps currently plays the built-in Ashfall world; loading arbitrary
editor-exported map packages is not implemented yet. Save version 9 preserves
world identity; older saves are interpreted as campaign saves.

The flashlight is reserved for later chapters: scripts can grant the `flashlight`
quest item, or use `give flashlight` in the developer console for testing.
Press **F** after acquiring it. Ownership and its on/off state survive saves;
removing the item turns it off. Menus pause flashlight input. The beam follows
aim with a soft cone and a limited shadow-ray budget.

Run `--flashlight-test` for Vulkan on/off captures, timings and save/toggle checks;
add `--software` to test the CPU fallback. Lossless texture packing now compares
PNG, reversible predictors and exact WebP, retaining the smallest result.
The packer `--verify-exe src RawMetal.exe` mode compares every embedded asset
against source pixels or bytes without executing the game.

Movement now resolves wall contact without discarding the whole movement step,
follows descending stairs, and preserves launch momentum with restrained air
steering. Thrown clutter rebounds off the wall normal while keeping tangential
velocity. These are improvements to the existing jump/crouch controller, not
a complete vaulting or wall-running system.

The reactor now contains a **Reactor Stalker**, a masked 1,386-triangle creature
with retargeted skeletal idle, walk, melee, hit and death clips. It closes to
melee range and commits to a dodgeable swing; it has no ranged attacks.
The previous antlered Warden is replaced, including in existing saves.
Bugs now separate only from creatures on
overlapping floors, hear actual shots instead of reload animation, and request
a new route when blocked.

**R** reloads the six-round tube; mouse wheel up selects the shotgun and down
selects fists. Restart is in Esc with confirmation. Save format 4 includes the
hazmat ragdoll and still reads version 1/2/3 saves. Run `--physics-ai-test` for targeted
movement, creature behavior and clutter checks, or `--stalker-test` for animation
deformation checks and fifty front/side reactor pose captures.

The Stalker's shoulder/elbow/wrist retargeting now follows the source limb
directions instead of applying incompatible bone rolls. An olive-suited gas-mask
worker lies in the lift boarding room, with blood on the suit and floor. This
uses `Characters_psx/Models/Male/Character_28_HM.fbx` and its original matching
texture from the supplied asset library (988 triangles), not the rejected heavy
yellow radiation suit. The blood texture is `Textures/textures2/bloodsplotch_zdw3k.png`.
Fifteen articulated joints react to bumps and shots; saves retain the pose and
velocity. `--hazmat-test` checks floor clearance, constraints, fixed-step timing,
save/load and ray contact, and captures three inspection views. Texture packing
uses a verified reversible predictor without reducing texture resolution.

Vulkan hardware rendering is now the default, at full **640x360** internal
resolution. The GPU handles triangles, depth, textures, normal maps and emission;
materials are uploaded at startup and geometry is batched by material. Conservative
deck occlusion preserves shaft and stairwell views. Static lighting is cached,
shadow sampling is budgeted, and animation reuses mesh topology. A persistent
worker evaluates the arm rig in parallel with world geometry preparation.

Windows x64 remains the supported game platform. A Vulkan-capable graphics driver
is required for hardware acceleration; `--software` forces the CPU fallback.
The renderer targets Vulkan 1.0 without vendor-specific extensions. Its portable
graphics API does not yet make the Win32 window/input/audio layer cross-platform.
`RawMetal-renderer.txt` records the selected GPU or fallback reason.

Surface Lift now follows Turbine Gantry. Seven full map floors surround a
continuous shaft: the enclosed lift room rises three storeys, jams, then drops
six into the two-floor reactor complex. The main OST fades into lift machinery,
followed by cable creaking, a snap, impact and a darker reactor soundtrack.

Run `RawMetal.exe --surface-lift` to play the new section directly. E operates
the cab control. After impact, leave through the opposite door; the east service
stairs connect the reactor floors. Find the upper maintenance authorization disk,
insert it in the lower computer's floppy drive, prime lower FEED, open upper
RETURN, then confirm at the computer. Clear hostiles to open the authorized exit.
The extended lift sequence includes interrupted ascents, a power failure, a
temporary brake catch and a second fall before emergency egress.
Passing shaft floors are compact scenic machinery bays rather than full maps;
close guide rails, floor markers, sparks and a loose cable sell the movement.
The boarding room and both reactor floors remain playable.

Esc now includes **Save Game** and **Load Game** submenus with three slots.
Click or use arrows/Enter; Esc goes back before resuming. Overwriting or loading
requires confirmation. Saves retain all map progress, inventory, enemies, doors,
the lift ride and reactor puzzle, in `%LOCALAPPDATA%\RawMetal\saves`.
Invalid saves report an error without replacing the current game.
Design notes and asset provenance are in `src/LIFT_DESIGN.md`.

Press **backtick (`)** to open the developer console. Type `maps` or `help`,
then use `map foundry`, `map pressureworks`, `map gantry`, `map lift`, or
`map reactor`. IDs 0-3 also work. Map loading starts fresh; `map reactor` skips
the lift sequence. `reload`, `where`, `fps`, and `clear` are available, along
with `r_scale 50`, `r_scale 75`, and `r_scale 100` (the default). The view is
never cropped and the HUD stays full-resolution. Up/Down recall commands;
backtick or Esc closes the console. Gameplay pauses while it is open.

Download **RawMetal.zip** from [GitHub Releases](https://github.com/MAJWCF1234/RawMetal/releases/latest), extract it, and run **RawMetal.exe**. The ZIP contains one self-contained executable; no companion asset file is required. **RawMetal.cmd** is an optional launcher in the source checkout.

Press **I** for inventory and **Esc** for settings (or to close inventory). Select an inventory item, then click an empty storage cell to move it. **E / Enter** equips or stows the selected shotgun, or consumes selected first aid. Item previews use the game models; ammo counts reflect your current supply.

The current renderer uses cached soft shadow samples, normal maps with normalized mip blending, and discrete door poses for shadow-cache updates.

The canonical build outputs are **RawMetal.exe** and **RawMetal.zip** in the project root. Binaries belong in GitHub Releases. Runtime audio/error logs and temporary archives are excluded from source control.

Build with **Build.cmd** (Visual Studio 2022 C++ tools, CMake and the Vulkan SDK
with `glslc` required). Shaders compile to embedded SPIR-V; players do not need
the SDK or loose shader files. Every configuration writes the same root
executable. Close the game before rebuilding; do not create alternate executable
folders to work around a running game.

- `src/`: game source, embedded assets and detailed documentation.
- `.build/`: disposable compiler intermediates and symbols.
- `diagnostics/`: verification reports and inspection images.

For smoke verification, run `..\RawMetal.exe --smoke-test` from `diagnostics/`.
`--vulkan-test` checks hardware materials, depth, alpha cutouts and near clipping.
`--performance-test` measures update/audio/render time at full resolution across
seven scenarios and compares six culled views pixel-for-pixel with an unculled
reference. It fails if any measured gameplay frame exceeds 50 ms (20 FPS).
Startup is reported separately; window presentation is not included. Results are
machine/load-dependent, not a universal minimum-FPS guarantee.

All map arrays and named layers are in `src/world/World.cpp`; layer types are in `World.h`. Turbine Gantry has a separate upper-catwalk array at 3 m. See `src/CHUNKS.md` for traversal and door-controlled streaming.

E lifts/drops loose junk; left click punts it for 5 damage. PSX Bunkers debris, ration packs, power supplies, circuit boards and floppy disks accompany the purchased glass bottles. Objects tip, tumble, settle on their sides and make material-specific spatial impact sounds.

The build losslessly compresses runtime assets into the executable and rejects it at or above **19,800,000 bytes** (decimal MB). Materials use supplied 256-pixel texture variants; the generator atlas has a documented 384-pixel runtime copy to meet that limit. Purchased originals remain unchanged. Packing tools and reports live under `.build/`.

Main walls, grating and square steel bulkheads use the supplied PSX Texture packs,
including matching normal maps. Machinery retains its authored proportions and
material assignments. See `src/assets/materials/SOURCES.md` and
`src/assets/facility/SOURCES.md` for provenance.

Coolant Return uses the purchased water color/normal textures with animated transparent surfaces and buoyancy/drag for loose junk. Carried and thrown objects cross adjacent chunk seams with the player; save format 8 preserves transferred objects. The lift boarding collar is sealed with solid walls.
Run --water-wall-inspection (optionally --software) for water and lift-wall views.
