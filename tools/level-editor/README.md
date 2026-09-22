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


## Materials workspace

The Materials tab is now a visual paint workspace instead of a file-style list.

- Materials display as large swatches.
- Starred favorites stay at the front and can be isolated with the Favorites filter.
- Each swatch shows how many tiles on the active floor currently use that material.
- **Brush** paints normally.
- **Fill Area** flood-fills a connected floor, wall, or deck region that shares the starting material.
- **Pick From Plan** is an eyedropper that copies material, real-world repeat size, and direction from an existing painted tile.
- **Erase Material** removes only the surface material while leaving the floor, wall, or deck geometry intact.
- Material direction can be set to 0, 90, 180, or 270 degrees.
- Repeat size and direction are stored per painted tile and carried into the 2D and 3D previews.
- **Update Existing On This Floor** changes every tile using the selected material to the current size and direction.
- **Clear This Material From Floor** removes that material from the active floor without deleting building geometry.


### Rectangular material repeats and alignment

Materials are no longer forced to use a square repeat. The artist can set a separate real-world **Width** and **Height** for one repeated panel, tile, sheet, or sign texture. The default lock keeps both values together for ordinary square materials, but it can be turned off for long wall panels, strips, grating, and other rectangular industrial finishes.

The Materials panel also has simple arrow controls for **Align Pattern**. Each click nudges the texture by one quarter of its repeat size, allowing large panels and seams to be lined up around doors, equipment, and architectural boundaries without moving any geometry. Size, direction, and alignment are stored per painted tile and are preserved by the eyedropper.


### Faster material selection and replacement

The Materials filter now includes **Recent** and **Used On This Floor** alongside Favorites. Recently chosen materials are remembered locally, and materials already in use can be isolated immediately when revising a finished room.

A new **Replace Material** mode works like color replacement in a paint program. Choose the new material, click one painted tile using the old material, and every use of that old material on the active floor is changed to the new one with the currently selected repeat size, direction, and alignment.


## Direct Save / Open

The editor can save and reopen projects directly without making the artist manage browser downloads. **Save** writes project JSON under `tools/level-editor/projects`, **Open** lists those projects, and **Save As** creates a second project file. Ctrl+S saves and Ctrl+O opens the project list. Import and Export still exist for portable copies.

## 3D selection and richer arrangements

Placed editor objects can now be clicked directly in the 3D preview. The corresponding blueprint object becomes selected, and double-clicking an arrangeable object opens its arrangement workspace.

Arrangement hierarchies can now be nested. An object placed on another object can itself be opened and decorated, so a crate on a table can have items placed on the crate while the whole assembly still travels together.

An arranged item can also be raised or lowered in simple 5 cm steps. **Put On Floor** detaches it while preserving its world position.


## Building section view

The editor now has a **SECTION** button for vertical work without turning the workflow into a 3D modelling program.

The section view is a blueprint-style side elevation of the active chunk. It shows every floor, each floor's ceiling, projected walls, stairs, equipment, wall-mounted items, and ceiling-mounted items. **X Section** and **Y Section** let the artist look through the building from either plan direction.

The important part is that height can be edited visually:

- drag a gold floor line up or down to move the whole building level
- drag the blue ceiling line to change that floor's room height
- ceiling-mounted objects follow the ceiling automatically
- click a projected object to select it
- **+ Empty Floor Above** creates the next level directly above the current ceiling
- **Copy Plan Above** duplicates the current architectural plan and materials onto the next level without copying loose props

This keeps the workflow in familiar floor-plan / section-drawing language. Raw XYZ coordinates remain available only as a fallback.


## Clean paint palette

The Materials tab now exposes only textures that make sense as architectural finishes: construction materials, facility wall/floor/ceiling/metal surfaces, the Pressure Works wall/floor/metal set, and a small set of paintable industrial markings.

Textures that exist to skin models are no longer offered as wall or floor paint. Medkits, shells, pumps, generators, computers, creatures, clutter, and similar assets remain available through Prefabs instead of appearing as nonsensical paint choices.


## Vertical placement without game-engine coordinates

The editor's vertical workflow is now designed around ordinary building drawings rather than XYZ transforms.

- **+ Floor Above** creates the next floor at the current room ceiling with no height dialog.
- **Copy Plan Above** duplicates the architectural plan and finishes, but not loose props.
- **Ceiling / RCP** is a reflected-ceiling-style plan. Place a light, pipe, sign, terminal, block, or prefab there and it hangs from the ceiling automatically.
- **Hang Below** gives plain presets for flush, 25 cm, 50 cm, or 1 m below the ceiling.
- A ceiling-mounted object's own panel has the same drop presets.
- **Place Height Visually In Section** opens the building section with the object already selected.
- In the section view, props and overhead equipment can be dragged vertically. Floor objects dragged upward become visually positioned room-height objects, wall items slide up and down the wall, and ceiling objects remain attached to the ceiling while their drop changes.
- 3D Preview now has **All Floors**, **Cut Above**, **This Floor**, and **Show Ceilings** controls. Cut Above is useful while dressing an upper floor; Show Ceilings makes it possible to inspect lamps, pipes, and other equipment hanging under the roof.
- Walk Preview turns on the ceiling/roof preview automatically so the interior reads like an enclosed building.

The numerical height fields remain available for precision, but the normal workflow no longer requires an artist to think about a game-engine vertical axis.


### Multi-chunk vertical preview

The 3D floor filters use the active floor's **elevation**, not a per-chunk internal layer ID. That means **This Floor** and **Cut Above** work across a multi-chunk industrial district instead of accidentally isolating only one chunk. Walk Preview also starts in the center of the active chunk at the active floor's height, so upper floors and mezzanines can be inspected directly.


## Blueprint-first workflow

The editor is deliberately aimed at someone who designs physical buildings rather than someone who knows Unity, Blender, or C++.

- **LIVE SPLIT** keeps the 2D blueprint and the generated 3D space visible at the same time. Draw or move something in plan view and the 3D side updates from the same project data.
- **MEASURE** is a non-destructive tape measure. Drag between two points to see total distance plus horizontal and vertical offsets in metres.
- **ROOM LABEL** places a blueprint-only area label such as Pump Hall, Electrical, Office, Receiving, or Maintenance. Labels are saved with the project but do not create game geometry.
- The **Floor Snapshot** panel shows the active floor's approximate floor/deck area, wall-grid occupancy, placed item count, and blueprint-label count.
- Every placed object now has a simple editable **Name** field so an artist can call things what they mean in the building rather than working from asset filenames.

These are editor-only authoring improvements. They do not require rebuilding RawMetal or compiling the game.


### Fast placement

The 2D plan now behaves more like a simple drawing program when placing equipment:

- Move the pointer over the blueprint to see a translucent **placement ghost** before clicking.
- Choose 1 m, 0.5 m, 0.25 m, or free-form **Place Snap**.
- Press **R** to turn the next placed prefab or primitive by 90 degrees before stamping it down.
- With Select / Move active, **R** rotates the selected object by 90 degrees.
- **Escape** returns to Select / Move without touching project data.

This keeps ordinary prop and equipment layout mouse-driven instead of requiring coordinate entry.


## Blueprint ink and character-start dummies

The 2D editor now has a **Blueprint Ink** view for the building designer. It hides finish textures without changing them, keeps the metre grid, adds 4 m rulers around the active plan area, and makes rooms, walls, doors, stairs, labels, and placement markers easier to read. **Show Finishes** switches the material preview back on instantly.

**Export Plan PNG** temporarily frames the building in Blueprint Ink and saves a clean PNG of the current plan. This does not change project data or require RawMetal to be rebuilt.

The old game-facing word "chunk" is de-emphasized in the artist UI. The same 24 x 24 m pieces are presented as **Plan Areas**, because they are contiguous pieces of one building rather than separate levels.

The **People / Spawns** tool now works as a real blueprint placement tool. Choose Worker, Security, Scientist, generic NPC, Hostile, Creature, or Player Start, then click the plan. The marker has a facing arrow in 2D and a simple scale-reference dummy in 3D. These markers are deliberately editor-native and easy to reposition or rotate, so the artist can communicate intended population and spawn layout without knowing gameplay code.


### Always-visible quick tools

The left sidebar now keeps a small workflow strip pinned at the top while the asset browser scrolls. **Select, Room, Wall, Door, Equipment, People, and Finishes** are always one click away, so the artist does not have to scroll back through editor controls after browsing a long asset library.

Placement rotation no longer depends on knowing the `R` shortcut. **Turn Left** and **Turn Right** buttons are visible beside placement snapping, with the current facing angle shown directly underneath. The keyboard shortcut still works for speed.

The raw asset browser is now labeled **Advanced Files**. Normal building work should happen through Equipment/Prefabs and Finishes/Materials rather than source filenames.


## Whole-building canvas and New Building wizard

The editor now treats the 24 x 24 m engine pieces as an implementation detail instead of an artist task.

- **New Building** starts from real building dimensions, floor count, and floor height.
- It generates however many internal plan areas are required and can create the exterior shell automatically.
- **Room** and **Wall Line** can be dragged straight across old 24 m boundaries. Missing plan areas are created silently while drawing.
- Matching floor elevations across neighboring plan areas are edited and displayed as one continuous floor plan.
- New neighboring plan areas inherit the building's floor stack, so an upper floor does not suddenly disappear at an internal map seam.
- Floor Snapshot totals now cover the whole active building elevation rather than only the currently selected 24 m area.

The intended workflow for a building designer is now: enter the building dimensions, draw rooms and walls like a blueprint, place equipment and people, choose finishes, and use Live Split or 3D Preview. The underlying plan-area split is still visible when needed, but normal design work no longer has to stop at those boundaries.


## Simplified artist surface

The permanent toolbar is intentionally smaller now. The normal building workflow exposes Plan, Live Plan + 3D, 3D, Walk, Undo/Redo, New Building, Open, and Save. Less common operations are kept under **More** instead of competing for attention.

The sticky left-side quick tools are the primary workflow: **Select, Room, Wall, Door, Equipment, People, Finishes**. The internal Plan Areas panel and the larger set of specialty drawing tools are collapsed by default. Nothing was removed, but the editor no longer presents engine-oriented controls as if the artist must understand them before drawing a building.


### Visible placement controls

Character role, placement snapping, and the Turn Left / Turn Right controls now stay in the normal artist workflow instead of hiding inside the advanced drawing-tools section. Choosing **People** immediately reveals the Worker / Security / Scientist / NPC / Hostile / Creature / Player Start selector where the artist can actually see it.


## Designer-first interaction pass

The normal workflow has been pushed further toward drawing software instead of a game editor.

- An always-visible **floor ribbon** sits on the blueprint. Floors switch like drawing sheets, and Floor / Ceiling plan modes are available there without hunting through panels.
- **+ Floor** is available directly from that ribbon.
- Equipment and props can be **dragged from the library straight onto the blueprint**. Clicking an asset and then stamping it still works.
- Finishes can also be dragged onto a tile, after which the same finish remains active for ordinary brushing.
- The quick tools now include a plain **Erase** tool. Click a placed object to remove it, or drag across building tiles to erase geometry and its finish together.
- Obvious architectural prefabs can carry a sensible mounting default. Ceiling lamps and vents hang from the ceiling automatically, while wall cabinets try to snap to a nearby wall. The artist can still change Floor / Wall / Ceiling after placement.
- Prefab cards say **WALL** or **CEILING** when the editor already knows the expected mounting surface.

The intended mental model is a floor-plan drawing program: choose a thing, put it where it belongs, change floors like sheets of paper, and let the 3D view be generated from the plan.


## Artist placement pass

The editor's normal workflow is now even less game-editor-like.

- **Equipment**, **Finishes**, and **Files (Adv.)** replace developer-oriented asset terminology in the main palette.
- The always-visible **Equipment Goes** control offers Auto, Floor, Wall, and Ceiling. Auto keeps sensible prefab defaults, while the other choices let a building designer override placement without touching XYZ values.
- Reflected **Ceiling / RCP** mode still wins for overhead work, so dropping equipment there behaves like a ceiling plan.
- Floor elevation and room-height numbers are now tucked under **Exact Heights (Optional)**. Normal floor changes happen from the floor ribbon, + Floor Above, Copy Plan Above, and the visual Building Section.
- A selected prop, dummy, hazard, or other ordinary object gets a plain **Floor** selector, so moving something to a mezzanine or upper floor does not require recreating it or editing height coordinates.
- Placement ghosts say whether an equipment item is going on the floor, wall, or ceiling before the artist clicks.

These are editor-only HTML/JavaScript changes. They do not require rebuilding or compiling RawMetal.
