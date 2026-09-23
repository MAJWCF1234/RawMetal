# DEPTHWORKS ENGINE // MODULAR MAP INJECTION SYSTEM

The campaign can now be edited as lightweight, self-contained map payloads instead of moving the whole `World.cpp` around for every level change.

The public entry point is:

```text
InstallMap.cmd <path-to-map.txt>
```

You can also drag a payload `.txt` file onto `InstallMap.cmd`.

## What it does

The installer reads the payload metadata, validates the target level, and offers three choices:

1. Install into the main campaign, patching only that level's slot in `src/world/World.cpp`, then run `Build.cmd`.
2. Copy the payload into `custom maps/` as a standalone archive.
3. Abort without changing anything.

Main-campaign injection creates `src/world/World.cpp.bak` before touching the source. The backup is ignored by Git so the normal sync workflow will not accidentally publish it.

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
- `META_DEFAULT_TARGET` is informational metadata retained with the payload.

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

## Custom map vault

Choosing the custom-map option does not touch `World.cpp` and does not build the game. It archives the complete payload under:

```text
custom maps/<Level_Name>_Slot<ID>.txt
```

This is useful for exchanging maps, keeping alternate revisions, or handing one level to an AI or collaborator without sending the whole world implementation.

## Important scope note

Installing a brand-new campaign slot only injects the level implementation. A truly new campaign index must also exist in `WorldDefinition.h` so the engine has a chunk origin, player start, spawn height and environment entry for that index.

Replacing an existing level does not require changing `WorldDefinition.h`.
