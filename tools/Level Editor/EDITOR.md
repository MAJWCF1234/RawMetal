# RawMetal native level editor

Run `Start Editor.cmd` (or `RawMetal.exe`). One program, `RawMetal.exe`, plays and edits. It opens
on a menu: New campaign, a previous project, or Play. New campaign is a blank
map. A project opens that file in the editor. Play runs the game. Closing the
editor returns to the menu. The picture is the engine in this same program.
The engine owns RMC loading, world construction, materials, models, physics
and rendering.

The copied engine and source are independent of the parent game installation.
A release zips this program beside the game, with this folder's `custom maps`.
Players unzip it anywhere. It does not need the game's docs, tools, or install
folder.
Plan tools and the live engine view cover the building work the earlier C#
editor was used for: rooms, blocks, doors, hazards, windows, ground paint,
level pads, the player start, people, room surfaces, models, stamps, furnish,
outdoors and the map soundtrack.

## The friendly layout (2026-11)

### Doors stand either way

A door stands in whichever wall it is put in. With **Door**, click in a gap
(open cells with wall tiles at both ends, up to four wide) and the door fills
it: a gap between walls above and below makes a door along Y, one between walls
left and right a door along X. A drag runs along the gap, across or down the
plan. **R** turns the door the tool is about to fit; the right pane chooses
whether it slides or swings. Pick a door and **R** (or Turn) turns it a quarter
about its centre, hinge and swing side with it; the boxes under Turn choose
**Opens by** (slides, swings), **Stands** (along X, along Y), **Swings to** and
**Hinge at**, one Ctrl+Z each. Entry and exit doors stay along X. The plan
draws a swinging door as its leaf standing open from the hinge block, with the
arc its latch sweeps. Turn copy, Flip copy, group turns and floor turns carry
doors round. It is `DOOR` field 21 (axis) and 22 (hinge).

Below them: **Lock shown** (none, keycard reader, key, power release: field
24), **Breaks down** for a swinging door (Never, or how much battering it takes
before a Brute, Chimera or the Machine breaks it: field 23; pick 150 or type a
number) and **Starts open** (field 16; an ajar door does not start opening).
The plan marks the lock beside the latch post, draws a door that breaks down
with a dashed, cracked leaf, and an ajar leaf at its angle. `--editing-test`
drives all of it and leaves the plan picture in
`.build/interaction/doors-plan.bmp`.

### Creatures that hunt (v1.4.0)

Pick a creature and the inspector shows two boxes under Turn, Copy and Delete:

- **Hunts first** lists the people on the creature's map by number, name (the
  Person tool's name, and the name they speak under when they talk) and where
  they stand. The creature comes out for that person before anyone else. The
  plan draws a dashed red line from the creature to them, ringed at their end.
  It is `CREATURE` field 10, the person's actor index.
- **Recurring as** names a recurring animal (`CREATURE` field 9): it cannot
  die, it runs and is gone, and creatures with the same name anywhere in the
  campaign are that animal coming back. The list offers the names already
  given in this campaign; type a new one and leave the box.

Each change is one Ctrl+Z. Deleting a person renumbers every quarry after
them, and a creature whose quarry was deleted hunts the player again. Copying
a creature together with its quarry points the copy at the copied person.
Both raw fields stay under Advanced. `--editing-test` drives both boxes, the
plan line, deletion and Undo through the real controls
(`native-editor-interaction-test.txt`); `--demo 11` shows a Chimera after a
scientist.

### Parts palette pictures

The resonance crystals (`res_crystal_l/m/s/wall`, slots 374-377) and bedrock
outcrops (`res_rock_crag/spire/low`, 378-380) sit on the **Caves** tab as
"Resonance crystal large" ... "Bedrock low". `RawMetalEditor.exe
--make-thumbs` now makes every picture (`native-editor-thumbs.txt`, 389 of
389): paper-thin pieces (the corner web, a sheet of paper, an ID card) are
staged a centimetre thick and the Arch at its usual 2.4 x 3.4 m, since the
engine refuses a 2 m arch. Delete a `.build/thumbs/*.failed` marker to have a
picture tried again.

### Sign designer

Choosing the **Sign** tool opens a small **Sign designer** window beside the plan
(picking any sign on the plan opens it too). The picture at the top is the
engine's own sign, drawn by the same code the game uses (`src/renderer/SignArt.h`).

- **Big words / Small words**, a **plate style** (enamel plate, hazard, notice,
  stencil painted on the wall, route, chalkboard, picture), a **symbol**
  (warning, electric, arrows, no entry, radiation, fire, exit), the
  **colour**, the **lettering** colour (Auto uses the style's own) and the
  **size** in metres.
- **Place on wall**: click a wall face on the plan. The sign snaps 5 cm proud of
  the nearest wall (paint sits flush), faces the room, stays on that run of wall
  and hangs at eye height. Untick the snap to place it freely.
- While a sign is picked every change applies to it at once; Ctrl+Z undoes.
- **Library**: ready-made signs, every design already in this campaign, and
  **My signs** (`.rmsign` files in `custom maps\signs`). Click one to use it.
- **Import...** reads a `.rmsign` design or a picture (PNG, JPEG, BMP, TGA).
  A picture becomes the sign's art: the whole plate for the Picture style, or a
  logo in the symbol slot. It is saved inside the map (`SIGN_ART`), shrunk to
  256 px at most, so a shared map carries it.
- **Export...** writes a `.rmsign` design or a PNG of the sign. **Save to mine**
  adds it to My signs; **Delete mine** moves one to the Recycle Bin.
- The status line counts the signs on the map: the game's own maps keep to three.

`RawMetal.exe --sign-designer-test` checks snapping, a designed sign with a
picture through the engine, restyling, picture clean-up and the file round trips
(`native-editor-sign-test.txt`).

### Sound tool and room reverb

**Sound** (Decor page) places a sound from the engine's sound bank: pick it in
the list on the right and click where it comes from. Beds (vents, fans, a
buzzing tube, dripping water, drones) are placed as loops; every other sound
(creaks, bangs, distant yells, groans, stingers, the announcement voice) is
placed to play now and then. The inspector changes its loudness, how far it is
heard, the seconds between plays, and a switch: a loop or a now-and-then sound
with a switch runs only while the switch is set, and mode 2 plays the sound once
each time the switch is set (a voice over a door, a stinger on a trigger).
Creatures and guards never hear these. They are `AMBIENT` records in the map.

The game gives every room a reverb by measuring the space the player stands in.
A `REVERB` record (one per map, under Advanced) sets it by hand: how much, and
how long it rings.

The list is read from `src/audio/SoundBank.h`, so it is always the engine's.
`RawMetal.exe --sound-tool-test` places a bed and a single sound and a reverb and
has the engine validate them (`native-editor-sound-test.txt`).

### Live edits rebuild one map

An edit used to rebuild the baked geometry and light of every map the picture
draws (up to 15 on the Starter Map: about two seconds before the change showed).
The engine now keys each map's bakes by that map's own revision
(`Game::staticRevision`), so an edit rebuilds the map it changed and the maps
beside it keep their buffers. New, Open, Play and a change to the number of maps
still rebuild everything. `--editor-live-render-test` checks it with two maps
side by side; `--editor-perf-test --perf-trace` prints `static rebuilds N` for
every edit in `.build/editor-preview-trace.txt`.

### Floors, selection and adjoining maps (v1.2.16)

Use the selector and **Shift-click** to add or remove objects from the selection.
Drag any selected object to move the group; Copy, Delete and Undo operate on the
whole group. Drag a marquee over the plan to select an area.

The **Floors / layers** tab in the right dock manages native floor layers:

- **New Layer... (Ctrl+Shift+N)** opens name, elevation and thickness fields;
  press Create. A new upper floor reserves 2.4 m below the map ceiling.
- **New from Visible** copies the visible floor stack into an editable composite
  group, preserving relative story heights and native object relationships.
- **New Layer Group**, **Duplicate Layers (Ctrl+Shift+D)**, **Merge Down** and
  **Delete Layers** manage selected floors. Merge Down lowers the selected
  floor's contents into the floor beneath it. At least one floor must remain.
- **Stack** changes the dock order. Edit elevation to change physical height.
- **Mask** uses the plan selection, inverts a mask or removes it.
- **Transparency** and the opacity slider adjust the 2D and 3D editing preview.
- **Transform** exposes footprint corner resizing, flips and elevation/thickness.
- Show/hide and lock controls apply to floors or groups. Group view settings
  combine with child settings without overwriting them. Other floors have
  reference outlines; the dock can hide them.

Masks, opacity and reference settings are saved as editor comments in RMC.
**Play and exported geometry use the complete native map.** Erase tiles or edit
geometry to make a permanent opening.

Selected objects have real corner/endpoint handles in the plan and 3D view.
The opposite corner stays fixed while resizing. Drag a highlighted upper or
lower height edge vertically. Select a wall/roof, then drag its upper edge up
to raise the native ceiling. Raising a thin roof underside lifts the slab while
retaining its thickness. Native wall/floor textures keep their metre-based
repeat scale. The standing player needs 1.6 m; check the dock's headroom readout
and place the player start on the intended floor.

3D hover outlines show what the selector will pick. Dragging a prop uses a plane
through the grabbed surface; movement and elevation update the native fields
live, with one Undo when released. Esc cancels the transaction.

**+ Map** offers north/top, south/bottom, west/left and east/right. Occupied edges
are disabled. A new map shares the existing ground-floor elevation and a short
three-tile opening joins the edges. Uncheck **Open a passage between maps** in
that menu to retain the original walls and boundary settings. The editor selects its ground floor and
updates the engine view. One Undo restores the entire join. The renderer submits
the active map and nearby maps within 48 m, excluding distant maps from drawing.

The Floor tool offers **Floor** and **Open space** on upper layers.
Choose any material for a floor: concrete produces a concrete floor, including upstairs. Paint-tool drags make local material patches/stripes; Fill repaints a room. Open space or Erase cuts the actual painted floor for ramp openings. Native ramp landings open automatic guardrails.
Upper `=` cells are walkable decks with positive thickness. Upper `.` and `_`
remain empty. Upper `#` cells have native walls up to MAP ceiling, with matching
rendering and collision in the updated game. Use the matching game patch when
sharing maps that rely on upper-layer walls.

`--editing-test` drives selection, handles, layers and four-edge creation through
the hidden editor. `--layer-export-test=FILE` checks the authored catwalk export
in the engine; `--campaign-export-test=FILE` checks native seam traversal and
nearby rendering. `--editor-live-render-test` also checks 3D view settings.

### Corridors (v1.2.17)

Choose **Corridor** and drag between clear floor inside two rooms. Routing checks
the complete two-metre footprint and wall buffer, avoids authored furniture,
uninvolved rooms and enclosed ramp openings, and stays inside the map. Reversed
drags create the same corridor. A blocked route or an already open connection
leaves the document unchanged.

The tool opens native wall panels only through the active storey's height band.
It preserves the wall above/below, adds physical upper-floor surfaces where
needed, keeps existing concrete and stripe patches, and adds only missing light
coverage. Managed dressing keeps its saved passage reservation clear. Duplicate,
flip and footprint-resize operations transform that reservation with the floor.
Undo restores the full corridor transaction.

`TestCorridors.cmd` checks the grid router. `--editing-test` exercises native
corridor placement, material/gap preservation, branching, save/reopen and Undo.
`--corridor-export-test=LIST` walks the exported test routes in the actual game.

### Editing and recovery (v1.2.18)

Each opened project has its own Undo/Redo history. New, Open, Starter and campaign
imports reset the previous project's editing history and selection. An unsaved
import remains eligible for recovery even before the first edit.

Escape and returning a dragged object to its starting point restore the clean
state as well as the native geometry. Recovery waits until a gesture ends. A
failed save keeps the previous file and recovery copy; recovery-write errors
appear in the status bar.

Floor duplication and Merge Down retain automatic furniture ownership and room
dressing choices. Copying a cable with its owners attaches it to the new owners.
Copying a cable alone resolves its native endpoints and makes an independent
copy. The original cable and owners keep their relationships.

The hidden `--editing-test` also runs persistence, selection, layer and dressing
regressions, with separate `native-editor-*-regression*.txt` reports.

### Creation and ownership

Autodressing is **on by default**. The left pane has an **Autodressing: on/off**
button; its setting is remembered. Off disables automatic furniture, finishes,
lamps and doors during room creation. Explicit decorating commands still work.

All older/unmarked records are user-generated. New automatic furniture, floor
pieces and lamps carry editor ownership comments in the native RMC file. The
engine ignores these comments. Redressing replaces its own tracked output and
preserves user content. Manually editing an automatic object makes it user-owned;
cancelling a drag or undoing restores its previous ownership too.

Drawing a wall deliberately takes priority over intersecting objects on that
floor. Dependent cable/part records are cleaned up, surviving indices are updated,
and structure outside the wall volume is retained. One Undo restores both the
wall operation and removed content. Autodressing uses rotated footprints, room
cell boundaries, door/spawn clearance and connectivity checks to avoid placing
objects in walls or blocking the room. Repeated lamp addition fills coverage
gaps without duplicating already-covered lights, and relighting preserves user
lamps and other floors.

Dressing chooses a compatible furniture family from the map name and nearby
user-authored anchors; an explicit lab/office command overrides this choice.
Chairs face desks or sofas, equipment keeps usable front clearance, and a
committed edit repairs only invalid generated placements. Valid pieces stay
verbatim in the document. Room expansion keeps the unaffected composition;
manual deletion does not immediately respawn an item. Drags update the engine
picture while layout repair waits for release, so decor stays still under the
pointer. Undo restores the saved transaction without rerunning dressing.
Managed-room masks and preferences are ordinary editor comments in RMC;
older maps have none and are never opted in automatically.

`TestCreation.cmd` runs the creation/ownership and adaptive-layout regressions,
including engine-validated scenes for all six furniture families.
`RawMetal.exe --editor-live-render-test` reuses one real engine renderer and
checks that fixture movement/deletion and loose-prop movement change its pixels.

The window explains itself; nothing here needs reading to make a room.

- Top bar: New, Open, Save (saves in place once named; the bundled Starter
  always asks for a new name), Undo, Redo (grey when there is nothing to take
  back), Play F5, From start (Shift+F5), 2D paint F2, 2D + 3D F3, 3D edit F4,
  Example, Save as. Every button has a one-line tooltip.
- Left: maps, floors, grid, Add lamps, Make outdoors, music, then the palette in
  five pages (Build, Decor, People, Logic, World) of big glyph tiles.
- Right: the chosen tool and only its options (model, finish, who, creature,
  hazard, brush/box/fill...), then what is picked with Turn, Copy and Delete and
  at most a handful of plain fields ("Big words", "Brightness", "Trigger name").
  A field keeps its value when you leave it or press Enter; there is no Apply.
  Advanced shows every native field, the record kind list and the record list.
- Plan: the tool's hint sits on top; a ghost shows what a click or drag will
  make (the footprint of the chosen model, the stretched box, the door line);
  Select highlights what is under the pointer. Right click or Escape puts the
  tool down and returns to Select. Empty floor picks the map itself (name, sky,
  ceiling).
- Paint tools (Floor, Wall, Erase, Balcony, Crates) have Brush, Box and Fill.
  Room drags a box and, with "Auto lamps and door" on, hangs lamps and cuts a
  doorway with a DOOR. The same tools work in the 3D picture.
- Play is in the picture, at once: F5 or Tab drops you at the plan pointer (or
  where the 3D camera is), Shift+F5 at the map's start. Escape returns to the
  view you were building in, with the camera and selection where they were.
- Every tool makes one native record: Lamp, Sign, Furniture, Loose prop,
  Shape, Computer, Creature, Health & ammo, Smoke (click); Block, Water, Stairs,
  Hazard, Place name, Trigger, Darkness, Crusher, Glass, Grass & dirt, Flatten
  (drag a box); Pipe (drag, or click for a riser), Cable (drag), Door (drag
  along a gap). A picked trigger lists its steps and an Add list (show a
  message, turn a switch on, count up, take an item, only if a switch is on,
  fire when stepped in / anywhere on the map / anywhere, repeat).
- Deleting a person deletes all of their records and renumbers later people;
  deleting a trigger deletes its steps. Renaming a trigger or a clock renames
  everything that points at it.
- Unsaved work is copied to `custom maps/Autosave` every 15 seconds; the next
  start offers it back. Opening or checking a map no longer marks it unsaved.

`RawMetalEditor.exe --remake-test` rebuilds Starter maps 0-6, 28, 29, 33 and 34
using only tool gestures, option lists and field boxes, compares every record
with the Starter field by field, asks the engine to validate the result and
writes `native-editor-remake-test.txt` (exit code 0 on PASS). The remade
campaign is saved to `.build/remake/`, with per-map timing in
`.build/remake-progress.txt`. `--demo 1`..`8` opens real UI states for
screenshots (6-8 are the People, Logic and World pages).

## Native details

- Save writes RMC directly; unchanged records stay byte-identical. Save as...
  (and a validated save) opens the game's own `custom maps` folder when this
  editor sits inside the game workspace.
- Moving a person moves both pose keys and the guard post; moving pipes, cables
  and boxed volumes moves their endpoints together. Deleting protects cable and
  glass attachments and remaps later fixture and prop indices.
- Copy area / Paste area copy tiles and every object whose footprint meets the
  rectangle; Turn copy and Flip copy turn the footprint, positions and facing
  together, and a copied cable keeps its link when its fixture travels with it.
- Map music is native `MAP` field 19; Listen auditions it through the engine.
- The engine viewport skips the title screen, HUD, viewmodel and player body.
  In 3D, hold the right mouse to look, WASD to fly, Space up, C down, Shift fast.
  While a drag tool is held in 3D the engine shows the real result live (a
  scratch copy only the engine sees); point tools place on release.

## Speed

- In `RawMetal.exe` the builder and its engine picture are one process: the
  picture runs on its own thread and the document, control and status pass in
  memory (`src/platform/EditorChannel.h`), never through files. A separate
  engine (`RawMetalEditor.exe` beside `.build/engine/RawMetal.exe`) still
  trades small files.
- An edit reparses only the map it touched, through the engine's own parser
  (`LiveCampaign` in `EditorPreview.cpp`), and rebuilds only that map's World.
  Another map from the list is stepped to inside the same scene
  (`Game::switchEditorMap`) instead of building a new game.
- While a drag, stroke or slider is held, one change is in flight at a time:
  the engine says at once when it is in the picture (`WM_APP + 41`) and the
  next goes out.
- Document queries go through an index (`NativeDocument.h`); the plan keeps
  its map layer in a bitmap and draws only the pointer's rings, labels and
  ghost on top; autosave writes on its own thread.
- `--perf-trace` (or `RAWMETAL_EDITOR_PERF=1`) adds interaction costs to
  `.build/editor-session-trace.txt` and the picture's reload and frame costs to
  `.build/editor-preview-trace.txt`. `RawMetal.exe --editor-perf-test` times
  every gesture on the Starter Map into `native-editor-perf-test.txt`; add
  `--perf-baseline` for the old paths (`native-editor-perf-baseline.txt`).
  `RawMetal.exe "--editor-live-campaign-test=custom maps\Starter Map.rmc"`
  checks every map read on its own against a whole parse.

## Background automation

`RawMetalEditor.exe --selftest` opens Starter Map and the Ashfall Exclusion Zone, checks that save/reopen keeps every byte, and asks the engine to validate every map.

`RawMetalEditor.exe --background-test` runs an entirely hidden native UI + engine
reload and save/reopen check. It does not capture the desktop or move the cursor.

`RawMetalEditor.exe --editing-test` exercises the actual hidden mouse/command
handlers, transform preservation across Starter records, model/map search,
undo/redo/cancel, copy/delete and native cable attachment reference handling.
`--layout-test` checks pane bounds, view modes, background painting and encoding.

`RawMetalEditor.exe --script commands.txt` runs native document editing commands
without a visible window, then validates the result using RawMetal. Paths in the
script are relative to the working directory. Commands:

For command shells, `EditorCli.cmd commands.txt` waits for completion, prints the
result and returns its exit code.

```text
NEW|Campaign name
ROOM|Another map
OPEN|existing.rmc
ADD|SIGN|0|10|10|2|2|1|0|Welcome|My map|3968216
PAINT|0|0|5|5|#
SET|MAP|0|0|2|Renamed map
REMOVE|SIGN|0|0
CLEAR|0
SAVE|my-campaign.rmc
```

`SET` arguments are record kind, map index (`-1` for global), matching record
ordinal, field index, value. Record ordinals and field indices start at zero.
`REMOVE` uses the same kind, map and ordinal, and refuses to delete an object
that a cable or breakable still points at. `CLEAR` drops every object on that
map and leaves the room, its floors, its tiles, and world terrain records.
`ADD` contains an unmodified native engine record. Percent-encode literal pipes
inside text as `%7C`, as described by the custom map guide.

## Build

`BuildEditor.cmd` builds the C++ UI with Visual Studio 2022 Community.
Build the copied engine separately:

```text
cmake -S src -B .build/vs2022 -G "Visual Studio 17 2022" -A x64
cmake --build .build/vs2022 --config Release --target RawMetal
```

The plan, the people list and the engine view author the records the game
loads. Dialogue and script records stay on the field list.

## Engine sync

`src/` is the builder's own copy of the game's engine, with the builder's
code in it (adoptEditorCampaign, EditorView, observer mode, the unified
build, EditorShots). Bring it up to the game with

```text
python tools/editor/sync_engine.py --dry     what it would do
python tools/editor/sync_engine.py           sync to the game's HEAD
```

`ENGINE_SYNC` (beside this file) records the game commit the copy was last
synced to. For each file: if the builder's copy equals the game's at that
commit, the game's current file is copied; if it already equals the game's
now, nothing happens; otherwise `git merge-file` merges the game's change into
the builder's file (base: the game at the last sync). A conflicting merge is
reported and left alone (`--force` writes it with conflict markers), and
ENGINE_SYNC then stays where it was. Files only the builder has are never
touched; files the game deleted are reported. CRLF and LF count as the same;
merged files keep the builder file's line endings.

It also copies new game sources (and any source the builder's CMakeLists
names that the copy lacks), the game's tracked `custom maps/` files (GUIDE.html
is where the builder reads every record's fields), and assets: every file
`src/assets.rc` embeds (the voice bank among them) and everything new or newer
under the gitignored `src/assets/library/`. `--to REV` syncs to another
commit, `--worktree` takes the game's uncommitted files (and leaves
ENGINE_SYNC alone), `--editor DIR` runs it against a scratch copy. After a
sync, run `Build.cmd`; an `assets.rc` change repacks for ten minutes or more.

## Open from campaign

- **Open from campaign** (next to Open) lists the game's levels by chapter, with
  a search box, and opens one as a new, unsaved map. The engine exports it
  (`RawMetal.exe --campaign-map-list`, `--export-rmc`; MAP_SYSTEM.md "Campaign maps as .rmc").
- **Save into the game** writes `campaign-overrides/NN.rmc` in the game this
  builder sits in (the repository), or in a game folder the player points it at
  once (remembered in `game-folder.txt`); with no game it saves as a custom map.
  The game builds that level from the file. **Back to original** deletes it.
  **Save as** still writes an ordinary custom map; a sealed level's first offer
  is the `Campaign edits` folder, never `custom maps`.
- **Export my changes** (Ctrl+Shift+E) puts every changed campaign level in
  `Campaign edits/<user>-campaign-edits-<date>.zip` and opens Explorer on it;
  `tools/campaign/merge_edits.py` merges it on the other side.
- Sealed chapters are hidden only in a public release (`public-release.txt`
  beside the exe). There a whitelisted admin unlocks them: **Admin** in the
  corner of the list, or Ctrl+Shift+A, asks for a name and PIN; the window
  title then shows the admin badge and **Lock** shuts them again.
- Headless: `RawMetalEditor.exe --campaign-open-test <level> <out.rmc>` and
  `--campaign-export-test` (reports in `native-editor-campaign-*.txt`).
