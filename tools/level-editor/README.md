# Depthworks Level Editor

Run `LevelEditor.cmd` from the repository root. No build step is required.

The editor is intended for artists and designers, not programmers. The default asset browser shows **Prefabs** instead of loose source files. A prefab combines the real model, its game texture, a friendly name, and a useful default footprint. Click a prefab and then click the 2D map to place it. Materials work the same way: click a material and paint tiles.

## Main workflow

1. Run `LevelEditor.cmd`.
2. Stay in **2D EDIT** for layout work.
3. Paint Floor, Wall, Void, or Deck tiles.
4. Open **PREFABS**, click Pump / Shelf / Crate / Computer / etc., and click the map to place it.
5. Select placed objects to move, rotate, resize, duplicate, or delete them.
6. Open **MATERIALS**, click a texture, and paint it onto floor/wall/deck tiles.
7. Switch to **3D PREVIEW** at any time to inspect the same layout with real models and textures.
8. Export the project JSON when ready.

The **FILES** tab still exposes the underlying asset library for inspection, but artists should normally work from Prefabs and Materials.

The launcher caches the pinned Three.js editor runtime into `tools/level-editor/vendor` on first use. After that, the editor can reuse the local copy. If the 3D runtime cannot be downloaded, the 2D editor still works and reports the 3D problem instead of leaving the whole editor dead.

RawMetal remains authoritative for exact rendering, collision, animation, lighting and gameplay.


## Object arrangement workspace

Select a placeable object such as a table, shelf, crate, machine, or blockout box and choose **ARRANGE ON / AROUND**. The editor opens a focused object workspace with:

- a large orbitable 3D view of the selected object
- simple quarter-metre plan slices for **On Top**, **Around It**, and **Under It**
- a prefab picker for decorations and equipment
- drag-to-position, 90-degree rotation, and removal
- automatic parent/child grouping so decorations follow the main object when it moves

This is intentionally surface-based. Artists do not need to type vertical coordinates just to put a computer on a desk or a bottle on a crate.


## Blueprint-first building workflow

The main editor now treats vertical work like a building plan instead of a game-engine transform exercise:

- **Room** is a drag tool. Drag a rectangle and it creates the room's perimeter walls and interior floor.
- Floors are shown as named building levels with height in metres.
- Each floor has a simple **ceiling height**.
- **Floor Plan** places objects on the active floor.
- **Ceiling Plan** automatically hangs placeable props and lights from that floor's ceiling.
- Ceiling-mounted objects are visually distinguished in the 2D plan and can be switched between floor and ceiling from the selected-object panel.
- The raw numerical measurements are still available, but are tucked under a **Measurements** disclosure instead of being the primary workflow.
- Chunks can be added north, west, east, or south from the chunk list.
- **Fit Plan** frames the whole multi-chunk building in the 2D editor.

These controls are deliberately phrased for someone who thinks in floor plans, rooms, ceilings, equipment, and elevations rather than game-engine coordinates.


## Material tile size

Painted materials now have an adjustable real-world repeat size. This is separate from the 1 metre blueprint grid.

When a material is selected, **Texture Size** offers 0.25 m, 0.5 m, 1 m, 2 m, 4 m, 8 m, or a custom value. A larger value makes the texture itself appear larger across the building instead of forcing every source image to repeat once per grid square.

The chosen size is stored per painted tile, previews continuously across neighbouring cells in 2D, and is carried into the 3D preview. **Resize Existing On This Floor** changes every tile using the selected material on the current floor, so an artist can fix an already-painted wall or floor without repainting it.


## Safety and fast editing

The editor now behaves more like a forgiving drawing program:

- **Undo / Redo** buttons are always visible. Ctrl+Z and Ctrl+Y work too.
- An automatic browser-local recovery copy is saved after edits. When the editor opens after an interrupted session, it offers to recover the last autosave.
- Ctrl+C / Ctrl+V copies and pastes the selected object.
- Copying or duplicating an object also carries along decorations attached through the Arrange workspace, so a dressed desk or machine stays together.
- Delete removes the selected object and its attached decoration assembly.
- **Wall Line** draws a straight horizontal or vertical wall by dragging, with its length shown while drawing.
- 3D wall height now follows the active floor's configured ceiling height instead of always being three metres.


## Smart architectural placement

The editor now removes more vertical and alignment work from the artist:

- **Smart Door** is placed by clicking a wall. It snaps to that wall, turns to match the wall direction, and the 3D preview automatically cuts the wall opening. Moving the door keeps it snapped to valid wall tiles.
- Placeable props, blocks, lights, and terminals can be switched between **Floor**, **Wall**, and **Ceiling** from the selection panel. **Wall** finds the nearest wall, moves the object to the wall face, turns it correctly, and gives it a sensible mounting height.
- **Stairs to Floor** automatically connect to the nearest floor above. Their rise, run, and step count are calculated from the actual floor elevations.
- A selected stair has a plain **To Floor** control for choosing another upper floor.
- Stair geometry in the 3D preview is now rendered as actual steps instead of a solid rectangular block.
- Door and stair symbols in the 2D plan are more blueprint-like, so their direction is readable without opening the 3D view.


### Wall mounting refinements

Wall-mounted objects now have simple **Low**, **Eye Level**, and **High** placement presets. This keeps signs, control boxes, lights, and other wall decor adjustable without exposing raw vertical coordinates.

Smart door openings also preserve the wall above the doorway in 3D, so placing a door creates an opening with a proper lintel rather than deleting the entire wall column.
