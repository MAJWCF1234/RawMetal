# DEPTHWORKS ENGINE // MODULAR MAP INJECTION SYSTEM

Depthworks uses one portable UTF-8 `.txt` payload format for both built-in campaign injection and runtime custom campaigns. A payload can patch a dynamic MAIN slot, or it can be installed as player content that appears under **CUSTOM MAPS** without recompiling the game.

The public entry point is:

```text
InstallMap.cmd <path-to-map.txt>
```

You can also drag a payload `.txt` file onto `InstallMap.cmd`.

## What it does

The installer reads the payload metadata and now treats `META_DEFAULT_TARGET` as an actual routing contract rather than a comment.

The menu offers:

1. **Install using META_DEFAULT_TARGET**. This is the recommended path.
2. Install into the built-in MAIN campaign.
3. Install as a playable CUSTOM campaign.
4. Abort.

A payload declaring `META_DEFAULT_TARGET: CUSTOM` is protected from accidental MAIN injection. The MAIN path refuses it instead of allowing `if(m_level==1)` or another local custom-map number to be mistaken for a built-in campaign slot.

MAIN installation patches the matching dynamic slot in `src/world/World.cpp` and then runs `Build.cmd`. It still creates `src/world/World.cpp.bak` before modifying source.

CUSTOM installation never edits `World.cpp` and never requires a rebuild. It installs a playable campaign TXT under `custom maps/`. RawMetal scans that directory and lists valid campaigns under **CUSTOM MAPS** on the title screen.

The implementation is split between:

```text
InstallMap.cmd
tools/InstallMap.ps1
src/world/World.cpp
custom maps/
```

The CMD file owns the terminal UI. The PowerShell helper owns parsing, validation and source replacement.

## World.cpp anchors

Dynamic campaign levels live inside one bounded region:

```cpp
// === DYNAMIC_CAMPAIGN_MAPS_START ===

// === LEVEL_6_START ===
if(m_level==6){
    ...
}
// === LEVEL_6_END ===

// === LEVEL_7_START ===
if(m_level==7){
    ...
}
// === LEVEL_7_END ===

// additional levels...

// === DYNAMIC_CAMPAIGN_MAPS_END ===
```

Each slot is an independent `if(m_level==N)` block. This is deliberate. Replacing one slot never needs to rewrite the condition or brace structure of the neighboring maps.

Existing campaign maps 6 through 9 already have slot markers. A payload for an existing slot replaces only the text between that level's markers. A payload for a new dynamic slot is inserted immediately before `DYNAMIC_CAMPAIGN_MAPS_END`.

The common code after the dynamic region still performs door-sign setup, layer construction and the normal return path.

## Payload format

Every payload is plain UTF-8 text:

```text
META_LEVEL_ID: 7
META_LEVEL_NAME: Pump Annex
META_DEFAULT_TARGET: MAIN

--- MAP_CODE_START ---
  if(m_level==7){
   ...
  }
--- MAP_CODE_END ---
```

Required metadata:

- `META_LEVEL_ID` must be an integer. For MAIN it names the compiled campaign slot. For a legacy CUSTOM payload it is only compatibility metadata and does not address the built-in campaign.
- `META_LEVEL_NAME` names the map.
- `META_DEFAULT_TARGET` must be `MAIN` or `CUSTOM`. The installer enforces it. A CUSTOM payload cannot be injected into `World.cpp` unless its metadata is intentionally changed to MAIN.

Optional metadata:

- `META_CAMPAIGN_NAME` names the entry shown in the CUSTOM MAPS menu. If omitted, CUSTOM installation uses `META_LEVEL_NAME` as the campaign name. Level Editor exports fill this automatically from the project name.

For main-campaign installation the code block must declare the same level as `META_LEVEL_ID`. The installer rejects mismatches.

Dynamic main-campaign slots currently begin at level 6. Levels below 6 remain part of the hand-authored core campaign and are not replaced by this tool.

## Typical map payload

A payload may define its own local ASCII slices, then register normal world content using the same structures as `World.cpp`:

```text
META_LEVEL_ID: 7
META_LEVEL_NAME: Pump Annex
META_DEFAULT_TARGET: MAIN

--- MAP_CODE_START ---
  if(m_level==7){
   static constexpr MapRows GroundSlice = {
    "########################",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "#......................#",
    "########################"
   };

   m_layers={{"Payload / ground",-9,0,GroundSlice}};
   m_doors={{2,5,.5f,0,false,false,true}};
   m_terminals={{{12,12},"PAYLOAD TEST","SELF-CONTAINED MAP SLOT.","READY.",0,false}};
   m_lights.push_back({{12,12},-6.2f});
  }
--- MAP_CODE_END ---
```

The payload code executes inside the existing dynamic-map setup scope, so it can use the helpers already defined there:

```cpp
wall(...)
shelf(...)
cabinet(...)
tank(...)
post(...)
event(...)
action(...)
stairs
roof
```

It may also populate the normal World containers directly, including layers, structures, fixtures, props, pipes, doors, terminals, creature spawns, pickup spawns, clutter, lights, water, hazards, compactors and script events.

## Safety behavior

The installer checks all of the following before writing:

- the payload exists
- all required metadata exists
- the level ID is numeric
- the map code markers exist
- metadata and `if(m_level==N)` agree
- `World.cpp` contains exactly one dynamic campaign region
- any existing level-slot markers are balanced

A source-patching failure restores `World.cpp` from the backup automatically.

If source injection succeeds but `Build.cmd` fails, the injected source is left in place for repair and the backup remains available at:

```text
src/world/World.cpp.bak
```

This lets compiler diagnostics point at the actual payload code rather than silently discarding it.

## Runtime custom campaigns

CUSTOM is now a runtime install target, not an archive-only folder.

Choosing the custom-campaign option does not touch `World.cpp` and does not build the game. The installed file is written under:

```text
custom maps/<Campaign_Name>.txt
```

RawMetal scans `custom maps/` when it starts and again when the **CUSTOM MAPS** menu is opened. Valid campaign files become selectable deployments in that menu. Runtime-ready TXT files that already contain `CUSTOM_CAMPAIGN_DATA` may be copied there directly. Older `MAP_CODE`-only files should be passed through `InstallMap.cmd` once so the compatibility converter can add their runtime data.

A runtime-ready payload contains:

```text
--- CUSTOM_CAMPAIGN_DATA_START ---
CAMPAIGN|My Campaign|0
MAP|0|First Map|...
...
--- CUSTOM_CAMPAIGN_DATA_END ---
```

The runtime campaign data owns its own map indices, origins, player starts, layers, doors, structures, fixtures, props, lights, terminals, hazards, stairs, creatures, pickups, clutter, pipes, water and compactors. These indices are completely separate from the built-in campaign's `m_level` numbers.

An authored map with a nonnegative `MAP` residency group can retain other halls for observation windows or overlooks:

```text
VISIBLE_GROUP|mapIndex|group
```

For example, `VISIBLE_GROUP|0|2` keeps every chunk in residency group 2 loaded while map 0 is active, even with closed connecting doors or a distant view. This adds visibility residency; it does not open boundaries, join traversal groups, or create a connection. Repeat the record for additional groups. Groups must be nonnegative integers; duplicate records are harmless, and omitted records preserve existing streaming behavior. Once the player leaves the viewing map, ordinary group, proximity and door rules determine which chunks remain loaded. This optional record can be added to the runtime data block manually; the editor currently has no control for it.

`TERMINAL` records accept an optional final yaw, in radians, to orient the console and its collision footprint:

```text
TERMINAL|mapIndex|x|y|z|control|title|line1|line2|activateState|toggleState|requireState|yaw
```

The default yaw is pi when the final field is absent or empty, preserving older packs. Yaw 0 faces the positive Y direction; pi faces negative Y. For example, `TERMINAL|0|5|5|0|0|SHIFT LOG|NO CALLS|LOCAL RECORD||0||0` places a console facing positive Y without an activation or prerequisite state. Nonfinite yaw values are rejected. The Level Editor exports an explicit terminal rotation in radians, including inherited placement rotation, and preserves pi for older objects without a rotation field.

`STAIR|mapIndex|x1|y1|x2|y2|bottom|top|steps|alongY|ascending|openUnderside|treadThickness|sideRails` supports open steel flights. The last three fields are optional: legacy stairs remain solid and have no added side rails, and tread thickness defaults to 0.12 metres. Open flights generate separate metal treads with matching collision and optical occlusion, leaving usable space underneath. Tread thickness must be finite and between 0.025 and 0.5 metres. The level editor preserves `openUnderside`, `treadThickness` and `sideRails` object properties on export. Side rails generate matching 1.05-metre tread-local guards for rendering, hull collision and optical occlusion. Stringers can be authored separately with ordinary pipe geometry.

`PIPE|mapIndex|ax|ay|bx|by|z|radius|endZ|material` accepts an optional material field: 0 service pipe, 1 rusted metal, 2 steel, 3 yellow painted steel, 4 red painted steel, 5 pale painted steel, 6 dark finish. An explicit `endZ` defines the end elevation and prevents automatic ceiling hangers. Omitting material retains the original service pipe finish.

`LEAD|mapIndex|ax|ay|az|bx|by|bz|length|radius|material|live|state|prop|ox|oy|oz|loose|panel|socket|terminal` is an electrical lead: a cable simulated every frame, not a pipe. It is plugged in at `ax|ay|az` (absolute heights, on a cabinet face or a machine's posts) and its far end is clipped to a carryable prop: `prop` is the map's `CLUTTER` record it clips to, counted from 0 in file order (it must be a model prop, kind 7), at `ox|oy|oz` in the prop's own frame (x across its width, y its depth, z up from its base). With `prop` empty or -1 the far end is tied to `bx|by|bz` instead; with `loose` 1 it has already been pulled off and hangs from its anchor, any of it that reaches the floor lying off toward `bx|by`. `length` (0.1-12 m) is the cable's own length, so a fifth or so over the span hangs a natural sag; `radius` is 0.002-0.05 m and `material` a pipe finish 0-6 or a cable's insulation, 7 black and 8 red (default 7; a test lead's pair is 8 and 7). Every field after `radius` may be left empty or dropped from the end of the record and takes its default: `material` 7, `live` 0, no `state`, no `prop`, `ox|oy|oz` 0 and `loose` 0, so a fixed lead can stop at `radius` (`LEAD|0|9.96|9|1.2|9.3|9|0|1.6|0.006`) and a loose one can leave the offsets empty (`LEAD|0|9.96|9.5|1.2|9.4|9.5|0|1.1|0.006||||||||1`); a record that stops part-way through the three offsets is rejected. While clipped the cable keeps off its prop: it leaves the clip outward through the face of the prop's box the clip is on (up to 5 cm, as a plug's boot holds it) and runs over the prop's edge rather than through it. A clip set below that face, such as a plug on a cell's lid under the top of its box, brings the face down to its own level, so the cable may lie across the lid but not sink below it (`--map-audit` fails a clipped cable that opens more than its own radius inside its prop). Every lead opens already hanging at rest, and a settled lead whose prop is still sleeps at no cost until the prop moves; only maps that are loaded simulate theirs. While clipped it follows the prop wherever it is carried, pushed or thrown inside its slack, and runs taut past it; it tears when the prop pulls the clip more than 0.3 m past `length`, is carried into another map or breaks. A `live` lead (default 0) arcs as it tears: sparks off the torn end, the `LeadArc` crackle (heard by guards and creatures 14 m off), a small shock (6 health, never the last point) if the player is holding the prop, then a few seconds of spitting before it goes dead. Tearing sets `state` to 1. A torn lead hangs limp from its anchor and swings until it settles. Saves (version 21) keep which leads have torn; the cables are hung again on load. A limp cable (torn on load, or `loose` from the start) is laid as a dropped cable falls: straight down from its anchor onto whatever is under it, along that and off its edge, turning aside from walls. It runs out of its socket's boot for a test lead, toward where its prop was put for any other torn lead, and toward `bx|by` for a loose one. It then settles under its own weight before the map or the save opens. `--map-audit` fails a cable that is asleep but not at rest (LOOSE: woken for a second of play, it moves more than 3 cm), such as one held out in the air past a bench's edge. The anchor must be on something solid (`--map-audit` checks it, and that each clip opens on its prop inside its length). For example `LEAD|0|6.01|8|1.6|0|0|0|2.4|0.007|8|1|desk_lead_torn|0|0|0|0.41` clips a red live lead from a wall at 1.6 m to the top of the map's first prop. The editor has no control for it yet. In C++ it is `kit::lead` and `kit::looseLead` (CampaignKit), or a `Lead` in `AuthoredMapData::leads`.

A test lead has a plug at each end and is placed by name, not by offsets: `panel` is the map's `FIXTURE` record (from 0, file order) of an instrument with sockets, `socket` the socket on it, and `terminal` the terminal post on the lead's `prop`. The engine knows where each model's ports are (`leadPorts()` in World.cpp): the `lab_field_meter` test set has sockets `+` and `-` (the hex binding posts on its front) and a pilot lamp on its top; `anl_cell_charged`, `anl_cell_spent` and `anl_cell_cap` have terminal posts `+` and `-` on the lid (+ red, - black). With a `panel` the anchor is set from the socket, so `ax|ay|az` may be left empty; with a `terminal` the clip is set on the post, so `ox|oy|oz` may be left empty. Records without the three fields are read exactly as before. The socket's plug sits in the panel's socket inside a coloured collar, and its boot holds the cable straight out for a segment before it bends. The terminal's plug (an insulated banana plug in the lead's colour) sits on its post, and the cable leaves it in line with the post, so it turns with the cell while carried, tipped or thrown. The first lead on a panel adds the panel's pilot lamp, a small amber practical light. It stays lit while the circuit holds and goes out when any lead on the panel tears, stuttering with the arc of a live lead. A loose test lead (`loose` 1 with a `panel`) hangs from its socket with its plug on its end and leaves the pilot dark. When a test lead tears, its plug pulls off the post and stays on the cable's end, dangling and swinging with its weight, and the bare post is left on the cell. Once the end comes to rest on the floor or a bench, the plug lies on its side on it. `--map-audit` fails a test lead whose plug is not seated (UNSEATED): an anchor more than 3 mm off a socket of its panel, or a clip more than 3 mm off a terminal post of its prop. This happens when a panel or a prop is moved after its leads were placed. For example, with the test set as `FIXTURE` 1 and a charged cell as `CLUTTER` 0, `LEAD|0||||0|0|0|0.8|0.007|8|1|stand_red_torn|0||||0|1|+|+` runs a live red lead from the set's `+` socket to the cell's `+` post, and `LEAD|0||||7.9|9.2|0|0.7|0.007|7|||||||1|1|-|` is a black lead already pulled off, hanging from the `-` socket toward (7.9, 9.2). In C++ it is `kit::testLead(m,panel,"+",prop,"+",length)` and `kit::looseTestLead(m,panel,"-",toward,length)`. `--lead-inspection` renders the test bay (`Game::leadInspection`) in ten views: panel, terminals, seated, carried, tearing, torn, dangling, bare, reloaded (the dangling bay saved and loaded) and loose (the black lead pulled off from the start).

`LIGHT` records support practical wall fixtures as well as the original ceiling lamps:

```text
LIGHT|mapIndex|x|y|z|mount|yaw|intensity|range|kelvin|tint|state|value|invert|period|onTime|phase|flicker|fitting|width|depth|height|gelR|gelG|gelB
```

Switching (the last seven fields, all optional) mirrors `HAZARD` gating: the lamp is lit while (`state` is empty or that state equals `value`, default 1) xor `invert`, and, with `period` > 0 seconds, only during the first `onTime` seconds of each period (offset by `phase`). `flicker` (0-1) adds a failing-tube pattern: mostly on, with stretches of rapid off-blinks, buzzing dim holds and, near 1, whole drop-outs. `LIGHT|0|12|8|3.2|0|0|1|10|5600|0.3|power_on|1|0||||0.4` is a green tube that lights when `power_on` is 1 and flickers. Power changes never rebuild lighting caches or redraw shadow maps. Period must be 0 or more, on-time within it and flicker 0-1.

`DARK|mapIndex|x1|y1|x2|y2|bottom|top|state|value|invert|jam` marks true darkness: while the player's eye is inside an active zone (same gating rule; empty state means always) the ambient fill, light floor, haze and exposure lift fade to black over about 0.3 s, and with `jam` (default 1) the flashlight sputters for 1.5 s and dies until the zone is left or switched off. Lamps keep working; switch them off with their own state. The muzzle flash then lights the room for an instant. Bottom/top default to the whole height.

In C++, use `WorldLight::enabledState`, `enabledValue` (default 1), `invert`, `period`, `onTime`, `phase`, and `flicker`. Put `DarkZone` entries in `AuthoredMapData::darkZones`; their exact fields are `x1`, `y1`, `x2`, `y2`, `bottom` (default -1000), `top` (default 1000), `state`, `value` (default 1), `invert`, and `jamFlashlight` (default true). Bounds include the lower faces and exclude the upper faces; height is checked against the player's eye. All coordinates and timing/colour fields must be finite, boxes nonempty and overlapping the map, and `0 <= onTime <= period`, `period >= 0`, `0 <= flicker <= 1`. Negative phase offsets work. States already persist in saves.

Vulkan keeps authored animated lighting on the per-frame lamp-buffer path, including under the diagnostic `--nopixel` option; its static geometry and lamp shadows remain cached. Software rendering scales cached lamp contributions on the CPU. `--darkness-test` writes PPM frames to `.build/darkness/` and numeric luminance/cache results to `darkness-render-test.txt`; `--darkness-test-software` checks the CPU path. The Vulkan test is part of `--each-test`.

The light fields after `z` are optional; absent or empty fields preserve a ceiling lamp with yaw 0, intensity 1 and a 12-metre influence radius. Mount 0 is a ceiling lamp. Mount 1 is a caged wall lamp: `x/y/z` locates the centre of its backing on the wall, and yaw points outward into the room (+X at 0, +Y at pi/2). The visible fixture is approximately 0.34 metres wide, 0.57 metres high and 0.16 metres deep. Its emitter and shadow probes sit in front of the backing, so authoring the backing on a solid wall does not bury the source inside that wall. For example, `LIGHT|0|1|6|2|1|0|1.2|6` mounts a wall lamp on the x=1 face, facing positive X, with a local six-metre pool. Wall lamps use static cached steel guards and warm emissive diffusers; they do not generate ceiling suspension rods or vertical light shafts. Intensity must be 0 through 8 and range must be greater than zero through 24 metres. These fields control direct material lighting and influence; switching uses the state fields above. Nonfinite values and unknown mounts are rejected. `kelvin` (1000-12000, default 3400) sets the lamp's colour from a blackbody at that temperature: about 2000 for sodium, 2800 for incandescent, 4500-6500 for tubes and daylight. `tint` (-1 to 1, default 0) shifts it green (positive, fluorescent) or magenta (negative). `gelR|gelG|gelB` (each 0-1, all three or none; the fitting fields before them may stay empty) put a colour gel over the lamp: `1|0.1|0.06` is a red emergency lamp, `0.1|1|0.15` a green resonance glow, `0.15|0.35|1` monitor blue. A gel keeps the lamp's brightness unless a channel would pass 2, so saturated blue and red come out a little dimmer. The gel colours the lamp's light, its shadows and the haze it lights (`WorldLight::gel`, or the last argument of `lamp()` in CampaignKit). The optional fields can be authored in the runtime data block manually; the editor currently exports the original ceiling-only record.

For example:

```text
META_LEVEL_ID: 1
META_LEVEL_NAME: City Outskirts Sector
META_DEFAULT_TARGET: CUSTOM
```

does **not** mean built-in campaign level 1. When installed as CUSTOM it becomes map 0 of its own one-map runtime campaign unless the TXT already contains a larger `CUSTOM_CAMPAIGN_DATA` block. The legacy `META_LEVEL_ID` is retained only so the same payload can still describe its old source-code slot.

### Office set, script actions and alarm sounds (2026-10-23)

Office set, from the purchased OFFICE pack. `FIXTURE` models 24-38:

- 24 desk, 25 corner desk, 26 office chair
- 27 CRT monitor (lit green terminal), 28 keyboard, 29 PC tower
- 30 CCTV TV (lit camera feed)
- 31 wide file cabinet, 32 tall file cabinet
- 33 desk phone, 34 mug, 35 water cooler, 36 coffee machine
- 37 bookcase, 38 binder

Real sizes are listed in `src/assets/office/OFFICE-SOURCE.md`.

Office surfaces, `STRUCT` materials 21-24: 21 office block wall, 22 drop ceiling,
23 office carpet, 24 office tile. In custom maps every structure texture
repeats per metre.

An optional final `STRUCT` field selects collision: `...|rail|material|solid`.
It defaults to `1`; `0` keeps decorative doorway trim visible while the surrounding
wall and lintel own collision. Door leaves still block the opening while closed.

### Surface sets and finish materials (2026-10-25)

A map can choose the finish of its `#` walls, its floor and its ceiling:

```text
SURFACE|mapIndex|wall|floor|ceiling
```

Each value is a surface set: -1 keeps the default finish, 0 cast concrete,
1 pressure works (the default painted wall), 2 painted industrial brick,
3 clinic tile, 4 lab panel (luminous ceiling), 5 natural rock, 6 rusted
corrugated sheet (dirt floor), 7 office plaster, 8 mine timber (gravel floor,
rock ceiling), 9 ancient stone (flagstone floor). For example,
`SURFACE|0|5|5|5` makes map 0 a rock cavern and `SURFACE|0|3|3|-1` tiles a
room but keeps its ceiling. Values outside -1..9 are rejected. Finishes are
laid in world metres, so they run on unbroken across tiles, floor risers,
ceiling steps and the wall over tall door headers. `STRUCT` material 3 (the
wall finish) follows the map's wall surface when one is set. C++ maps set
`AuthoredMapData::wallSurface`, `floorSurface` and `ceilingSurface`. The editor
has no control for it yet: put the record in the project's `runtimeRecords`,
which the exporter carries through unchanged. A pack without it renders exactly
as before.

`STRUCT` materials 26-35 are the matching box finishes, each repeating at its
own real scale: 26 rock, 27 clinic tile, 28 lab panel, 29 corrugated sheet,
31 brick, 32 mine timber, 33 ancient stone, 34 resonance growth (a dark
purple-red crust), 35 crystal (cyan and self-lit, like material 25). Material
30 stays the invisible collision proxy. The textures come from the purchased
PSX texture packs through `tools/library/surfaces.py`.

`RawMetal.exe --each-test=campaign-maps` checks all surface resources, derived
normal/mip chains, parser values and crystal emission. It also renders an
isolated test room with each finish on both CPU and Vulkan. The numerical
report is `surface-sets-test.txt`; the 40 PPM captures are written under
`.build/surface-evidence/`. These checks measure frame luminance, variation,
clipping, distinct finish hashes and reuse of GPU static geometry. They require
Vulkan and do not change campaign maps or inspect source images.

A `TERMINAL` placed within a solid fixture's footprint draws no console. Put
the interaction point on a desk's front edge and dress the desk with fixture
27/28 so its CRT is the visible computer.

Additional `SCRIPT_ACTION` kinds:

- `SCRIPT_ACTION|map|event|SHAKE|amount`: camera shake and flash, amount above
  0 up to 3.
- `SCRIPT_ACTION|map|event|ENTER_CAMPAIGN`: starts the main campaign at Map 0,
  exactly as the prologue hands off. It applies at the start of the next update. Use it
  to end a prologue.

Sound ids 42 `Alarm` (a short klaxon, 1.85 s) and 43 `AlarmDistant` (a 22 s
facility alarm bed). A running `TIMED_SEQUENCE` whose repeating tick sound is
42 also drives the red alarm beacon wash, which peaks on each blast. Example:
`TIMED_SEQUENCE|0|alarm|alarm_done|-2|-2|-1|-1|-100|100|60000|5200|1900|60000|42|0.8|1`.

Personnel computers. A `TERMINAL` that is neither a control nor a valve opens
full screen as a CRT running Depthworks SecureOS. Captioned `SEQUENCE_CUE`s
placed within 1.5 m of the terminal appear in its chat window, and the window
stays open while that conversation still has lines to come. Captions starting
with `YOU:` are the player's replies. Walking more than 2.4 m away closes it.

`MAP` field 23 (optional), start unarmed: `1` begins the map with no weapon
drawn and no viewmodel until the first punch or weapon swap. Use it for
off-duty openings.

See `src/assets/campaigns/RawMetal-Security-Office-Prologue.txt` for all of these in use.
It is the main campaign's opening: New Game starts it (embedded resource 398),
so it is not in the Custom Maps list. `tools/make_security_office_prologue.py`
generates it.

### Legacy MAP_CODE-only CUSTOM payloads

Older or hand-written payloads may contain only:

```text
--- MAP_CODE_START ---
if(m_level==1) {
    ...
}
--- MAP_CODE_END ---
```

If such a payload declares `META_DEFAULT_TARGET: CUSTOM`, `InstallMap.cmd` now translates the supported map schema into runtime campaign records and appends a `CUSTOM_CAMPAIGN_DATA` block to the installed copy. The original source TXT is not modified.

The compatibility converter supports the normal standalone-map authoring subset used by the documented schema:

- local 24 x 24 `MapRows`
- `m_layers`
- doors, including `requireState` and swinging-door flags
- `wall(...)`
- stairs
- pipes
- direct fixtures plus `shelf(...)`, `cabinet(...)` and `tank(...)`
- terminals, including simple `activateState` switches
- creature spawns
- pickups
- clutter
- direct lights and the common `for(Vec2 p : {...}) m_lights.push_back(...)` form

It intentionally refuses advanced source-only constructs that cannot be translated without changing their meaning, such as arbitrary script events and hand-written hazard/compactor/water scripting. For those maps, export from the Level Editor or author an explicit runtime campaign block instead of silently losing gameplay.

Legacy rows are validated during conversion. A row shorter than 24 characters is right-padded with an appropriate boundary/floor character and produces a visible installer warning; rows wider than 24 characters are rejected. This lets older hand-written CUSTOM maps survive simple off-by-one row mistakes without making MAIN source injection silently different.

This compatibility path is primarily for older single-map payloads. Full multi-map custom campaigns should be exported by the Level Editor or authored directly as one TXT containing the complete runtime campaign block.

## MAIN versus CUSTOM scope

For **MAIN**, installing a brand-new built-in campaign slot still only injects the level implementation. A truly new MAIN campaign index must also exist in `WorldDefinition.h` so the compiled campaign has a chunk origin, player start, spawn height and environment entry. Replacing an existing MAIN slot does not require that edit.

For **CUSTOM**, none of that applies. Runtime campaign TXT data carries its own map count, origins, starts and environment definitions. A custom campaign can therefore add its own maps without editing `WorldDefinition.h`, `World.cpp`, or rebuilding RawMetal.


## Level Editor payload export

The browser level editor can generate this exact installer format directly.

Open `LevelEditor.cmd`, author the map normally, then use:

```text
MORE -> EXPORT INSTALLER TXT
```

The export dialog asks for:

- map name
- level ID
- default target: `MAIN` or `CUSTOM`
- which 24 x 24 plan area to export

For MAIN installation, the selected plan area supplies the compile-time map slot.

For CUSTOM installation, the same exported TXT also contains a complete runtime campaign block for the **entire editor project**. All plan areas and their floors are serialized into that one file, so a multi-map custom campaign is uploaded and shared as one TXT rather than one TXT per chunk.

The exporter converts editor data into normal `World.cpp` map content:

- the selected area's 24 x 24 ASCII floor/deck slices
- additional floors as structural `=` decks
- upper-floor wall geometry
- smart horizontal doors, with north/south boundary doors inferred as entry/transfer doors
- smart window openings as structural sill/header apertures
- stairs
- lights
- terminals
- hazards
- blocks/structures
- mapped facility equipment, Pressure Works props, pickups, clutter and creature assets
- generic hostile/creature spawn markers

Blueprint-only information such as room labels and dimensions is intentionally not emitted as runtime geometry. Per-tile finish painting is also still editor-only because the current World payload API has no per-tile material override table; the exporter warns when a selected area uses painted finishes.

The generated file is named like:

```text
Map_07_Pump_Annex.txt
```

and already contains the required:

```text
META_LEVEL_ID
META_LEVEL_NAME
META_DEFAULT_TARGET
--- MAP_CODE_START ---
--- MAP_CODE_END ---
```

markers, so it can be dragged directly onto `InstallMap.cmd`.

The editor refuses invalid MAIN IDs below level 6 and surfaces export warnings before download instead of silently dropping unsupported content. Current runtime limitations are called out in the payload itself. In particular, the engine's `Door` type is horizontal-only, so a vertical smart door is exported as a valid open passage with a warning, and smart windows export as wall apertures because there is not yet a dedicated runtime glass/window entity.

For MAIN source injection, Player Start markers remain comments and compiled campaign starts still come from `WorldDefinition.h`.

For CUSTOM runtime campaigns, Player Start markers are serialized into the runtime map definitions and are used directly. The custom campaign does not need a `WorldDefinition.h` entry.

## Zones

```
ZONE|mapIndex|x1|y1|x2|y2|name|bottom|top|title|hazeR|hazeG|hazeB|hazeDensity
```

A zone names part of a map for the HUD location label (for example
`ZONE|0|0|0|24|8|01 INTAKE`). The first zone that contains the player wins;
where no zone applies, the HUD shows the map name. `bottom` and `top` are
optional heights that separate stacked spaces, such as a gantry above a hall.
Names are at most 40 characters.

### Haze

Every map has its own air: a haze colour (linear RGB, each 0-1) and a density
per metre (0-0.1). Without one a map keeps the environment's: interior
`0.11|0.15|0.18` at `0.008`, outdoors by sky. A map sets its own with four
fields after `MAP`'s start-unarmed flag (`...|0|0.30|0.22|0.14|0.016` for an
amber, smoky foundry; the residency group, floor, cue, yaw, pitch and unarmed
fields before them must then be written). A zone's last four fields give its box
its own air inside the map, a misted flooded room or a smoky furnace bay
(`ZONE|0|2|2|9|8|||||0.30|0.22|0.14|0.016`); a haze-only zone needs neither name
nor title. The zone the player's eye is in wins, then the map's, and walking from
one air into another blends over a second. All four fields or none. Density also
sets how brightly lamps glow through the air, so a denser room brightens its
light shafts too (0.016 doubles them against today's interior). In C++ it is
`ChunkDefinition::haze`, and the last argument of `zone()` in CampaignKit.
