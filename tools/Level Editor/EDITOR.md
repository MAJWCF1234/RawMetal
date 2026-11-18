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

### Connect: ropes, chains, hinges and the rest

**Connect** (Decor page, the chain icon) joins a loose thing to another thing, a
wall or the ceiling. Pick how on the right (**Join with**), then drag from the
thing to what it joins, or click the thing and then click what it joins. While
the drag is held the 3D view shows the joint as the game will play it (the drag
preview), and the plan shows a dashed line from the thing to the pointer.
- **Rope** and **Chain**: from the thing's top to the ceiling over the drag's end,
  or to the other thing. Shoot the rope in play and it drops.
- **Hinge**: drag from a standing panel (a gate) and its pin stands at the end
  you drag toward; from a flat thing outward and it turns about that edge (a
  hatch); across a plank to where it should balance (a seesaw).
- **Spring**: hangs and bounces back. **Weld**: held fast until hit hard enough.
- **Motor**: a hinge that turns by itself (a fan, a turntable).
- **Pulley**: drag from one thing to another; each hangs from a wheel in the
  ceiling over it, and as one goes down the other comes up.
- **Hanging lamp**: click under a ceiling and a caged lamp hangs there on a
  chain, its light swinging with it.

It writes one `JOINT` record (the lamp also its loose prop and its `LIGHT`); its
options are named fields with sensible defaults, listed in the inspector by the
guide's labels. Deleting a thing takes its joints with it and renumbers the
rest. `--connect-test` drives it (native-editor-connect-test.txt; the pictures
in `.build/interaction/connect-*.bmp`); `--editing-test` runs it too.

### Shapes

**Shape** (Build page) lays ready-made building parts (PRIM records). The parts
browser's Shapes page shows every kind with its picture, in six groups: Walls &
openings, Supports, Roofs & slabs, Stairs & steps, Details and New shapes (the
roof wedge, spire, pyramid roof, spiral stair, culvert, buttress, bevelled block,
eight-sided pillar, curved wall and catwalk). The kinds, plain names and usual
sizes are `src/world/PrimitiveKinds.h`, the engine's own table.

- A click places the shape at its usual size; a drag sizes it from the corner
  pressed, with a ghost showing its outline, the way it faces and its size.
  Sizes snap softly to ones a player reads as right (1.2 m doors, 2.2 m heads,
  3 m storeys); R turns the next shape a quarter.
- Picked, a shape shows its real outline and an arrow the way it faces, on the
  plan and in 3D. In 3D its handles: the green ones on its sides stretch that
  side (the far side stays), the yellow one on top raises it, the cyan one in
  front lifts it off the floor, the blue ring above it turns it a quarter at a
  time. The sizes show in the status line as you drag; each drag is one undo.
- The inspector says Width, Depth, Height, Faces, Material and Solid; the
  numbers for where it stands are under Advanced.
- Picking uses the shape's real turned outline (a curved wall is picked on the
  wall, not in the hollow behind its bow).
- `--shape-test` runs it alone (`native-editor-shape-test.txt`, pictures
  `.build/interaction/shape-palette.bmp` and `shape-handle-drag.bmp`);
  `--editing-test` runs it too.

### Glass and window walls

**Window wall** (Build page) makes a storefront. Drag along a wall, or across an
opening in one. It writes one `GLAZING` record on the wall's box: the run of `#`
tiles, the wall `STRUCT`, or the gap between two walls. It never changes a tile.
The engine (`src/world/Openings.cpp`) does the work when it plays the map:
- it cuts the wall from sill to head;
- it builds the wall back below the sill and above the head;
- it stands a frame in the middle of the wall's thickness, with a breakable pane in
  every gap.

Delete the record and the wall is whole again. The boxes on the right:
- **Glass**: Storefront (0.45 - 2.6 m), Window (0.9 - 2.1), Floor to ceiling,
  Clerestory (1.8 - 2.7).
- **Bays**: Auto (a bay every 1.2 m) or 1-8.
- **Rows**: 1-3, with a transom between them.
- **Panes**: Clear, Dirty, Tinted, Frosted, Wired (holds), Chain-link mesh,
  Pre-cracked, Empty frame, or a mix of clear, dirty, mesh and cracked.
- **Frame**: Aluminium, Painted metal, Steel or Wood.
- **Doorway**: none, in the middle, at the start or at the end. It is a bay open
  to the floor, with a pane over it.

The choices are the record's named fields (`cols`, `rows`, `sill`, `head`,
`pane`, `panes`, `look`, `door`; the guide lists them all). Picking the record
shows them in the inspector, labelled, to edit one by one.

One pane at a time: with the Window wall tool, **click a bay** of a storefront and
every pane in it takes the kind picked in Panes; **Shift+click** sets its bottom
pane only. It rewrites the record's `panes=` letters (one Undo); nothing else
changes.

**Glass** is a pane that snaps:
- Dragged along a solid wall, it cuts a 0.9 - 2.1 m window there (a one-bay
  `GLAZING`).
- Dragged across an opening, it stands a 24 mm pane from the floor to 2.4 m,
  centred in the wall's line, and closes the wall over it with a wall-finish
  `STRUCT`.
- On open floor, it is a thin 2.2 m screen.

Every glass pane is the v1.5.0 glass: it breaks a piece at a time where it is hit,
on its own, and a save keeps its holes. Mesh is chain-link: it stops a body, and
light, sight and rounds pass through it. Wired glass webs and holds.

`--glazing-test` (and `--editing-test`) drives both tools through the plan's mouse
and the tool boxes:
- an edit in the inspector, undo and redo;
- a delete that leaves the wall as it was;
- save and reopen;
- the engine validating each step.

It writes `native-editor-glazing-test.txt`, with pictures in
`.build/interaction/glazing-drag.bmp` and `glazing-inspector.bmp`. The lab tube
`storefront` plays one in the game.

### Breaches

**Breach** (Build page) knocks a hole through a wall. Drag along a wall (`#` tiles
or a wall `STRUCT`): it writes one `BREACH` record on the wall's box and, like the
Window wall, never changes a tile. On open floor it refuses, with a message. The
boxes on the right:
- **Shape**: Ragged (tall in the middle, falling away to the sides), Arch, Square.
- **Size**: Walk through (2.3 m), Crawl through (1.2 m), Window hole (1 - 2.1 m),
  Floor to ceiling.
- **Edge**: Concrete and rebar, Bare concrete, Brick, Clean cut.
- **Rubble**: none, some, lots.

The engine cuts the hole, chips its edge, hangs rebar and drops rubble. You walk,
see and shoot through it, light comes through it, and the creatures route through
it. `--breach-test` (and `--editing-test`) drives it; the lab tube `breach` plays it.

### Hanging cloth

**Hang cloth** (Decor page) writes one `CLOTH` record (NativeClothTool.inc). The
boxes on the right:
- **Hang**: Curtain (drag along where its top goes: it hangs from the ceiling),
  Strap (click where it hangs from), Cable (drag from one end to the other: it sags
  between them), Loose cable (drag from where it leaves the wall, at waist height,
  to where its free end lies on the floor).
- **How long**: a curtain to just over the floor or 1, 1.6 or 2.2 m down; a strap
  1 m, short, medium or long.

While the button is held the 3D view shows it hanging (the usual ghost). In play it
sways, parts round whoever walks through it, drapes over things and swings when shot;
the lab tubes `cloth` and `cloth-room` play it.

### Growth

**Growth** (Decor page) is a brush that paints a building going back to green:
- **Grow**: Ivy on walls, Moss on walls, Hanging creepers, Grass through the floor,
  Moss on the floor, or Overgrown (all of it).
- **Brush**: Grow or Erase (Alt erases too).
- **Brush size** 0.5 - 5 m, **Density** sparse to dense, **Size** small to large.
- **Seed**: empty is random; a number repeats a stroke exactly.

It finds the surfaces under the brush itself: runs of `#` tiles and the sides of
tall blocks for ivy and moss (flat on the face, turned out, never wider than the
face), the ceiling and the lips of ledges (blocks standing clear of floor and
ceiling) for creepers, and the floor and ledge tops for grass. Walls, water and
blocks stay clear. Every piece is an ordinary `FIXTURE` (solid 0) of a purchased
model, so the inspector, copy and prefabs handle it. Creepers are hanging plants:
in play they drift, part as you walk through and flinch when shot. A stroke is one
Ctrl+Z. `--growth-test` (and `--editing-test`) drives it, with pictures in
`.build/interaction/growth-ivy.bmp` and `growth-tool.bmp`; the lab tubes `growth`
and `atrium` play it. `--atrium-pictures` opens the Starter Map's atrium and pictures the
Window wall tool mid-drag, the picked storefront's fields, the Breach tool and the Growth brush, the
3D pane filled with the engine's picture of the same document (`.build/interaction/atrium-*.bmp`).

### Outdoors: the land, its plants and water

The builder samples the campaign's ground exactly as the engine builds it
(`src/world/TerrainField.h`, shared by both: the `TERRAIN` noise, the
`TERRAIN_FLAT` pads, `TERRAIN_SCULPT` grids and the `GROUND` cover). On a map
that is outdoors under a terrain sky (`zone_overcast`, `brutal_wasteland`):

- **The plan draws the land**: hill shade, the ground cover's colours and a
  contour every metre, with water filled in blue (dark when murky).
- **3D picks stand on the ground.** A click, a drag, a brush or an object drag
  in the 3D view meets the hill where the pointer is, not the map's zero plane
  under it. Picking and hover outlines, handles and drags use where things
  stand on the ground; a hill hides what is behind it.
- **Absolute heights start on the ground.** Triggers, pipes, crushers and sounds
  keep absolute heights outdoors, so new ones are made at the ground under the
  click; a dragged pool's surface sits just under its lowest bank.
- **Floors over land.** The 3D view's floor bands stand on the ground under the
  upper storeys, and the top band reaches the sky: land and trees above the
  ceiling are never ghosted or hidden as another storey.
- **Seams.** When a map becomes outdoors next to an indoor map it opens onto
  (Make outdoors, a ground paint, or + Map), the land is levelled at -1 along
  the seam (`TERRAIN_FLAT`, walked at -0.5) and three concrete steps climb to
  the indoor floor across every open metre: the guide's doorway recipe.

**Terrain** (World page) paints the land. **Brush**: Raise, Lower, Smooth,
Flatten to a level (where you press, or a level typed in **Level**), Ramp
(press at one level, drag to another: a slope between them), Paint the ground
(the cover picked under it). **Size** 1 to 12 m across, **Strength** 0.1 to 1 m a
dab. Each stroke is one Ctrl+Z; the 3D view follows the stroke. Heights are
written as `TERRAIN_SCULPT|x|y|24|24|offsets` (one 24 x 24 grid a map, at its
origin, in decimetres, `N*v` repeats); a brush over a seam writes the
neighbour's grid too, so the land stays one surface. Ground paint is written
as the fewest `GROUND` boxes covering the stroke.

**Nature** paints plants from the purchased nature pack as `FIXTURE` records:
**Plant** Trees, Bushes, Grass, Rocks, Plants & ferns or a Woodland mix, at a
**Density**, with **Sizes** from one size to wide variety and a random turn.
Trees are spaced apart, solid at the trunk only (core 0.1), upright, and never
on a cliff; **Align to slope** lays the rest along the hill (FIXTURE fields 15
and 16, the slope; the engine shears the model so it hugs the ground) and sinks
tree roots into it. Nothing grows in walls, blocks, shapes, stairs, doors,
furniture or water. **Erase** (or Alt) clears plants under the brush, with their
collision copies in neighbouring maps.

**Water** has three ways: **Draw a pool** (drag; outdoors it finds its bank),
**Fill a hollow** (click: the water rises until it would spill out of the map
and floods every metre below that level as `WATER` boxes at one surface) and
**Surface from the bank** (pick a pool, click its bank: the surface goes to the
ground there). A picked pool has corner handles and **Murk** (WATER murk) and
**Wet banks** (a `WET` box 1.5 m round it) sliders, one Ctrl+Z each.

`RawMetalEditor.exe --outdoor-test` drives all of it through the plan's mouse,
3D pick rays, the tool boxes and sliders on the Ashfall Exclusion Zone and a
new indoor/outdoor campaign, saves and reopens byte for byte, has the engine
validate it, and renders the builder's 3D picture before and after
(`native-editor-outdoor-test.txt`; pictures `.build/interaction/outdoor-*.bmp`
and `outdoor-*.ppm`). `--editing-test` runs the same checks without pictures.

### Building on hills, raised floors, flooring and finishes (2026-11-17)

- **Rooms on a slope.** Drag a room onto a hill and it stands on a footing by itself:
  level ground under it where the hill is gentle, a foundation down to the ground where
  it is steep, the land blended back round it and steps from a raised door to the
  ground. Pick the room's footing (Select, click its floor) to choose **Pad**,
  **Foundation**, **Stilts** or **Steps** (terraces up the slope). **Exact heights...**
  shows where the floor sits (the lowest, average or highest ground, or a typed height)
  and the Raise. Move the footing and it measures the ground where it went.
- **Raise.** Pull the picked floor's top edge up in the 3D view: it snaps softly to a
  step, two steps, half a storey and a storey, and a base fills under it. Indoors the
  Room tool's **Raise the floor** row does the same for a new room.
- **Floors on the land.** The Floor tool lays a floor on open ground level, cell by cell
  (a patio), at the level of the floor it touches.
- **Flooring** (Build page). Pick a floor look (planks, tiles, concrete, carpet, stone,
  brick, grating...) and drag it out; a click floors a whole room. On a slope it
  **follows the ground** (a path) or is **made level**. **Edge trim** puts a band of a
  matching material round it.
- **Painting.** Click a stair's step tops, step fronts or side rails, a balcony's floor,
  edge or railing, a footing's floor, foundation or edge, or a flooring's edge trim with
  the Paint tool, or pick the thing and use its **Part** and **Finish** lists. The
  material list shows a swatch of each material.
- **Recovery.** Unsaved work is copied every 15 seconds. The next start brings back the
  newest copy of any map, on that map; opening a map with newer unsaved work brings that
  back (**Start fresh** returns to the saved file). If the Builder stops, it saves the map
  beside its recovery copy and writes what happened and the last things done to
  `custom maps/Autosave/last-crash.txt`.
- Tests: `--finish-test`, `--ground-test`, `--soak-test [steps] [seed]`; `--editing-test`
  runs the first two. `--shots SCRIPT` takes pictures of the live Builder.

### Emitter tool: the air's own stuff

**Emitter** (Decor page, where Smoke was) places `EMITTER` records, the steam,
smoke, sparks, dust, drips, spray, fire, gas and glow the engine draws
(`src/renderer/Particles.cpp`). Pick a **Preset** on the right and click where
it comes from. The presets are the kit's own helpers in
`src/world/CampaignKit.h`, written as the record each makes: steam leak, steam
jet, smoke column, spark shower, dust motes, drips, spray, fire, toxic pool,
ceiling grit, arc glow, and the old plume. Each leaves at its own height
(drips and grit hang under the ceiling).

Pick an emitter and the inspector shows, under Turn, Copy and Delete:

- **What**: the kind (smoke, steam, sparks, dust, grit, drips, spray, fire,
  gas, the glow round live current, the old plume). A new kind takes that
  kind's preset numbers and keeps where it stands, its heading and its switch.
- **Restyle**: any preset's numbers on this emitter, the same way.
- **Points**: the way smoke, steam, sparks, spray and the plume are carried
  (0 east, 90 south), its reach kept. **R** (or Turn) turns it an eighth.
  Dust and gas fill a box (the drift is its half sizes); fire, grit, drips and
  the glow point nowhere, and the slider stays off for them.
- **Strength**: 0.05 to 4 (1 usual): how thick, or how bright.
- The fields below: height, carry, rise, rate, size, swell, how many at once,
  and the switch (**Only while switch... is set to**, **Turned round**) and the
  timed window (**Repeats every**, **On for**, **Starts this far in**), the same
  as a `HAZARD`, so a vent can breathe in time with its scald.

Each change is one Ctrl+Z. The plan draws each kind's glyph in its own colour,
a ring its size as it leaves (a fire its width), the box dust and gas fill, an
arrow the way it is carried, and a dotted ring round one a switch or a timed
window gates. A picked emitter shows its carry (or its box) in the 3D view.

The picture is the engine's: emitters show in the 3D view and move there, since
the builder runs the game's clock between edits (smoke rises, steam drifts,
sparks spit, water ripples; nothing else moves). **Light ▾ > Air moves between
edits** holds it still. A gated emitter shows once its switch is set
(Simulate, F6, or play).

### Light view and Rebake

**☀ Light ▾** (top band) sets how the 3D view is lit:

- **Lit**: as the game draws it.
- **Probe fill only**: just the light the probes carry (the engine's
  `--detail-probes`): the lamps' bounce off walls, floors and ceilings and the
  crystals' cyan.
- **Finish**: crystal red, metal blue, polish grey, black for no shine
  (`--detail-finish`).
- **No probes**: the lamps' flat room bounce, as before the probes
  (`--no-probes`), to compare.
- **Rebake light probes now** bakes every map's probes again from the map as it
  stands. The engine already bakes a map again when its lamps or its rooms
  change; Rebake is for when something else moved, or to see it happen. The
  status line says when the new bake landed and how long it took.

Probes need no record: the engine places and bakes them itself. A resonance
crystal (`res_crystal_l/m/s/wall`) is a light because of its model and its size,
so picking one shows **Crystal glow** read-only (how bright, how far it
reaches); its **Height** scales it. These are runtime switches in the picture,
sent in `look.txt` beside its control file and answered in `look-status.txt`
(`src/platform/EditorPreview.cpp`).

### Play as Claire

**Play: first person** (top band) switches to **Play: over shoulder**: Play
(F5) and From start then walk the map as Claire with the camera over her right
shoulder, as the game's `THIRDPERSON` console command and `--third-person` do.
Escape comes back to building as before.

### Film camera tool

**Film camera** (World page) places named cameras for the engine to film.
Click to place a still (it faces the way **R** turned it: an eighth a press),
or drag from where it stands toward what it sees. Cameras are named shot_1,
shot_2 and on; pick one to change it:

- **Camera name** (its pictures' name), where it stands, **Lens height**,
  **Turn** (degrees; 0 east, 90 south) and **Tilt** (degrees, up is more).
- **Moves for**: 0 is a still. More seconds moves it to the **...to** point,
  turn and tilt over that time (the plan draws its path dashed and its end).
- **Turned on Claire**: 1 makes it a replay camera turned on Claire, who
  stands at the map's player start; the engine films her from outside.

The plan draws each camera as a wedge the way it looks, with its name; the 3D
view outlines what a picked camera takes in. R turns a picked camera; Copy,
Delete and Ctrl+Z work as on anything else. A camera is a builder line the game
skips: `# EDITOR_FILM|map|name|ax|ay|az|turn|tilt|seconds|bx|by|bz|bturn|btilt|claire`.

**Write film list** writes an `--film` list beside the project
(`NAME.film.txt`; with cameras on several maps, `NAME map N.film.txt` each). It
starts `custom MAP X Y FEET YAW PITCH FILE`, the project's file and its player
start, then a frame line per picture: a still is one picture, a moving shot ten
a second (`NAME_0000` on), a camera on Claire thirty a second. Unsaved changes
are filmed from a snapshot in the picture folder. **▶ Render films** writes the
lists, runs `RawMetal.exe --film=LIST --film-out=FOLDER` (the one in this
folder) and opens `NAME film`, the pictures as `.bmp`.

`--editing-test` drives all four (the Emitter tool's presets, inspector, R,
Undo, plan, export and 3D picture; the light modes and a Rebake in a running
picture; play over the shoulder; cameras, the list and a render). Its pictures
are in `.build/interaction`: `air-plan.bmp`, `air-a.ppm` and `air-b.ppm` (the
air moving), `light-0.ppm` to `light-3.ppm`, `third-person.ppm`,
`first-person.ppm`, `films-plan.bmp` and the frames in `Film test film`.

### Raw Metal Filmmaker

**✇ Raw Metal Filmmaker...** (under the Film camera tool's buttons) opens a
window for directing shots: Source Filmmaker's idea, kept small. A **shot** is a
length of time on the map you are on; its **timeline** has a lane for the
camera, one for each person or creature you direct, and one for map states.

- **Shots** (left): **+ New shot** makes `shot_1`, 3 s at 30 frames a second.
  With a Film camera picked it starts from that camera (its still, or both ends
  of its move as two keys); otherwise from where the 3D view's free camera is.
  Name, length and frames a second (60, 30, 20, 15, 12, 10...: a whole number of
  engine ticks a frame) change under **Apply to shot**; a rename takes its keys.
- **Track** picks what the boxes key: **Camera** (East, South, Eye height, Turn
  and Tilt in degrees, **Lens** the field of view across in degrees, 0 the
  game's own, about 75), **Person N** / **Creature N** (where they stand, Feet,
  Turn, and a **clip**: 0 idle, 1 walk, 2 punch, 3 hit, 4 death, 5 crouch; the
  Investigator also 6-9; a creature plays idle, walk, attack and flinch), or
  **Map state**: an `--film` op at that moment (`set NAME=V`, `door I OPEN`,
  `lights S`, `ambient A`, `torch`, `shut`, `dark`, `lamp ...`).
- **◆ Set key here** keys the boxes at the playhead (a key already there is
  changed). **Key the 3D view** keys the camera where the 3D view's free camera
  stands and looks: turn off **Show in 3D view**, fly there, key. **Ease out of
  this key** slows out of it and into the next (a dot on the lane; a straight
  line is a diamond). **Delete key** removes the key under the playhead.
- **The timeline**: click or drag the ruler to scrub (frames snap); click a key
  to pick it and drag it to retime; a person's lane shows its clips as a strip
  (walking green, dying red). Space plays (it loops), the arrows step a frame,
  K keys, Delete deletes.
- Between keys a track moves in a straight line, or eased; turns go the short
  way round. A walk's phase follows the ground covered (a stride a cycle, so feet
  do not slide); a punch, hit or fall plays once from its key; idles loop.

**Show in 3D view** (on by default) plays the shot in the live 3D view: the
engine picture sits at the shot's camera and lens with the people and creatures
posed as keyed (it hands the free camera and the authored poses back when it is
turned off or the window closes). Map-state keys show only in a render.

**Render shot** / **▶ Render all shots** write one `--film` list a shot
(`NAME film/SHOT.film.txt`), film each with the builder's `RawMetal.exe` into
`NAME film/SHOT/` (`SHOT_0000.bmp` on) and, with `ffmpeg` on PATH, encode
`NAME film/SHOT.mp4` (H.264) and, for several shots, the whole cut in shot-list
order as `NAME film/NAME.mp4`. Without ffmpeg the frames stay as pictures.
Unsaved changes are filmed from a snapshot, as the Film cameras are.
`RawMetalEditor.exe --filmmaker-render "PROJECT.rmc" [MAP]` does the same from
the command line and writes `native-editor-filmmaker-render.txt`.

Shots and keys are builder lines the game skips; Undo, save and load treat them
like any record (each key or shot change is one Ctrl+Z):

```text
# EDITOR_SHOT|map|name|seconds|fps
# EDITOR_SHOT_KEY|map|shot|track|time|ease|a|b|c|d|e|f
```

`track` is `cam` (a b c: east, south, eye height; d turn, e tilt, f lens), `aK`
person K by ACTOR index or `eK` creature K in CREATURE order (a b c: east,
south, feet; d facing; e clip), or `set` (a: the op, percent-encoded).

The engine side is small (`src/`, synced into the builder's copy by
`tools/editor/sync_engine.py`): `--film` lists take `pose aK|eK X Y FEET YAW CLIP
PHASE` (held through every tick until `pose aK off`; `src/game/FilmPuppet.h`)
and an optional last field on `f` lines, the lens in degrees across
(`Projection::setFieldOfView`; play always draws at the game's 1.3). The live
preview reads `film.txt` beside the picture's control file and answers the free
camera in `film-status.txt` (`tools/Level Editor/src/platform/EditorPreview.cpp`).

`--editing-test` drives it through the window's own buttons: a new shot, two
camera keys (eased, the lens 60 to 40) and a walking person's key, a ruler scrub,
a key dragged to retime and Undo, save and reopen byte for byte, engine
validation, the 90-frame list's poses and lens, a rename, and a 2-frame engine
take (and its MP4 when ffmpeg is on PATH). It leaves `filmmaker-window.bmp` and
`Filmmaker test film` in `.build/interaction`.

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

## Named fields and prefabs (map format 2)

The inspector shows a picked record's own fields, then its **named fields**
(`key=value` after the positional ones; docs/Systems/map-format.md), each
labelled from the guide's named-field table: Tint, Mass, Frozen, Side, Custom
property code... The last row adds one: type `tint=1,0.4,0.4`, `frozen=1`,
`faction=enemy` or `p.code=1234` and leave the box. Empty a row to remove that
field. The first named field puts `FORMAT|2` at the top of the file. A key the
Builder does not know is listed as it is and kept. Moving, turning, copying and
every other tool keep a record's named fields; a copy leaves its `name=`
behind, since names are unique per map. Everything is one Ctrl+Z.

**Make prefab** (under Copy) turns the picked things into a `PREFAB` block,
written above the first map in metres from their middle, and puts one
`PREFAB_PLACE` where they stood. **Place prefab** adds another: a metre along
from the picked placement, or of the newest prefab at the cursor. A prefab
that names things gets a fresh name prefix per placement. R turns a placement
a quarter. The engine places the records when it reads the file; the Builder
keeps the block as written. `--editing-test` covers all of it, and
`--format-test` runs that part alone (`native-editor-format-test.txt`, with
inspector pictures in `.build/interaction/inspector-before.bmp` and `-after.bmp`).

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
