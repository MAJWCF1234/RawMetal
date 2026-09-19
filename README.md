# RawMetal

Download **RawMetal.zip** from [GitHub Releases](https://github.com/MAJWCF1234/RawMetal/releases/latest), extract it, and run **RawMetal.exe**. The ZIP contains one self-contained executable; no companion asset file is required. **RawMetal.cmd** is an optional launcher in the source checkout.

Press **I** for inventory and **Esc** for settings (or to close inventory). Select an inventory item, then click an empty storage cell to move it. **E / Enter** equips or stows the selected shotgun, or consumes selected first aid. Item previews use the game models; ammo counts reflect your current supply.

The current renderer uses cached soft shadow samples, normal maps with normalized mip blending, and discrete door poses for shadow-cache updates.

The canonical build outputs are **RawMetal.exe** and **RawMetal.zip** in the project root. Binaries belong in GitHub Releases. Runtime audio/error logs and temporary archives are excluded from source control.

Build with **Build.cmd** (Visual Studio 2022 C++ tools and CMake required). Every configuration writes the same root executable. Close the game before rebuilding; do not create alternate executable folders to work around a running game.

- `src/`: game source, embedded assets and detailed documentation.
- `.build/`: disposable compiler intermediates and symbols.
- `diagnostics/`: verification reports and inspection images.

For smoke verification, run `..\RawMetal.exe --smoke-test` from `diagnostics/`.

All map arrays and named layers are in `src/world/World.cpp`; layer types are in `World.h`. Turbine Gantry has a separate upper-catwalk array at 3 m. See `src/CHUNKS.md` for traversal and door-controlled streaming.

E lifts/drops loose junk; left click punts it for 5 damage. PSX Bunkers debris, ration packs, power supplies, circuit boards and floppy disks accompany the purchased glass bottles. Objects tip, tumble, settle on their sides and make material-specific spatial impact sounds.

The build losslessly compresses runtime assets into the executable and rejects it at or above **19,800,000 bytes** (decimal MB). Materials use supplied 256-pixel texture variants; the generator atlas has a documented 384-pixel runtime copy to meet that limit. Purchased originals remain unchanged. Packing tools and reports live under `.build/`.

Main walls, grating and square steel bulkheads use the supplied PSX Texture packs,
including matching normal maps. Machinery retains its authored proportions and
material assignments. See `src/assets/materials/SOURCES.md` and
`src/assets/facility/SOURCES.md` for provenance.
