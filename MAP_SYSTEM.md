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

- `META_LEVEL_ID` must be an integer.
- `META_LEVEL_NAME` is used for display and custom-vault filenames.
- `META_DEFAULT_TARGET` must be `MAIN` or `CUSTOM`. The installer enforces it. A CUSTOM payload cannot be injected into `World.cpp` unless its metadata is intentionally changed to MAIN.

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

RawMetal scans `custom maps/` when it starts and again when the **CUSTOM MAPS** menu is opened. Valid campaign files become selectable deployments in that menu.

A runtime-ready payload contains:

```text
--- CUSTOM_CAMPAIGN_DATA_START ---
CAMPAIGN|My Campaign|0
MAP|0|First Map|...
...
--- CUSTOM_CAMPAIGN_DATA_END ---
```

The runtime campaign data owns its own map indices, origins, player starts, layers, doors, structures, fixtures, props, lights, terminals, hazards, stairs, creatures, pickups, clutter, pipes, water and compactors. These indices are completely separate from the built-in campaign's `m_level` numbers.

For example:

```text
META_LEVEL_ID: 1
META_LEVEL_NAME: City Outskirts Sector
META_DEFAULT_TARGET: CUSTOM
```

does **not** mean built-in campaign level 1. When installed as CUSTOM it becomes map 0 of its own one-map runtime campaign unless the TXT already contains a larger `CUSTOM_CAMPAIGN_DATA` block. The legacy `META_LEVEL_ID` is retained only so the same payload can still describe its old source-code slot.

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
