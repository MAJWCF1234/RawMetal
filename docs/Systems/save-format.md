# Save format

The current format is `Game::SaveVersion`, 28 (src/game/Game.h). A save starts with a text header line, `RAWMETAL_SAVE <format> <checksum>`. The rest depends on the format:

- **Format 28 and on: sections.** A run of named, versioned, checksummed sections (src/game/SaveSections.h, src/game/Saves.cpp). New systems add sections without a new format.
- **Formats 1-27: the positional text archive.** One stream of numbers written by `Game::archiveSave`, now frozen. Every older save is still read through it, and is then refitted into format 28 (see below).

## The container (format 28)
After the header line, the payload is a run of sections. Each section is a frame and then its body. All numbers in the frame are little-endian:

| Field | Size | |
|---|---|---|
| name length | u8 | 1-48 |
| name | bytes | letters, digits, `.`, `-`, `_` |
| section version | u16 | the layout of this section's body, from 1 |
| flags | u16 | bit 0 `Required`: a build that does not know this section must refuse the save |
| body length | u32 | |
| body checksum | u32 | FNV-1a of the body |

The header checksum is FNV-1a of the whole payload.

Bodies are written with `SaveOut` and read with `SaveIn`:
- flags are one byte;
- whole numbers and enums are LEB128 varints, zigzag for signed;
- floats are 4 raw bytes and doubles 8;
- text is a varint length, then the bytes.

The same game always writes the same bytes. A fresh campaign save is about two thirds the size of the old text: 347 KB against 517 KB. A one-room save is a little larger (about 1 KB against 0.8 KB), because the text wrote a zero in two bytes. A load takes as long as before; the time goes to building the game, not to reading the save. Format 28 is binary, so `.rms` files are marked `binary` in `.gitattributes`.

### Reading
1. Every frame is checked before anything is read. Any of the following refuses the save, naming the section and the reason:
   - a cut-off frame or body: `truncated save: section 'props' holds N bytes, M left`;
   - a body checksum mismatch: `section 'player' is damaged`;
   - a bad name, version 0, or the same section twice;
   - a header checksum mismatch.
2. Sections are read in **this build's registration order**, whatever order the file has them in.
3. A section this build does not know, or a newer version of one it knows, is **skipped**. The load log names it. If the section is flagged `Required`, the save is refused instead.
4. A **core** section missing from the save, or newer than this build reads, refuses the save. A missing optional section runs its `absent` hook, if it has one.
5. Each body must be read to its last byte (`N bytes left unread` otherwise). A bad value refuses the save as `section 'name': reason`.
6. Then the authored-map reconciliation runs, then the usual player checks (below).
7. The 64 MB cap stands both ways. A save past it is refused on load, and `encodeSave` will not write one (the slot keeps what it had).

### The sections
| Section | Core | Holds |
|---|---|---|
| `game` | yes | world (campaign, Ashfall, custom), custom campaign key, map, elapsed time, won, how many chunks the chunk sections carry |
| `player` | yes | body, camera, motion springs, weapon in hand and its motion, switch and reload under way, the held prop |
| `inventory` | yes | first aid, each firearm's pool (total and loaded), the item grid cells, quest items |
| `hazmat` | yes | the hung hazmat suit's simulation |
| `script` | yes | every state flag, objectives, fired one-shot events. NPC, grenade and drone state lives in state flags (`friendlyKey`), so it is here. |
| `world` | yes | per chunk: kills, resident, lift and reactor, destroyed tiles, broken breakables, pane damage and ages, doors, torn leads, glass hits |
| `creatures` | yes | per chunk: every creature |
| `pickups` | yes | per chunk: every pickup |
| `props` | yes | per chunk: every prop, piece and shard |
| `entities` | no | runtime changes to entity properties (the store below). Written only when not empty. |
| `variables` | no | named values scripts set (the store below). Written only when not empty. |
| `physics` | no | the rigid-body solver's prop state. Written only once the solver installs its hooks. |

Settings are not in the save, by design: a loaded game keeps the session's settings (`adoptSession`).

## Adding a section
Call `Game::registerSaveSection` at start-up, before the first save or load. Registering under a name already taken replaces that section in its place.

```cpp
Game::registerSaveSection({.name="lights.flicker",.version=1,
 .write=[](Game& g,SaveOut& out){ /* out(a,b,c); out.text(s); out.value(v); */ },
 .read=[](Game& g,SaveIn& in,int version){ /* the same, in the same order; throw std::runtime_error on a bad value */ },
 .upgrade=[](Game& g,int fromVersion){ /* optional: bring an older layout up to date */ },
 .absent=[](Game& g){ /* optional: the save has no such section */ }});
```

- **Writing nothing.** If `write` produces no bytes, the section is not written (unless it is `core`).
- **Changing a layout.** Raise `version`, read every older layout by its `version` in `read`, and fill in what an older one lacked in `upgrade`. The format number stays 28.
- **Older builds.** Set `required=true` only when a build that does not know the section would be wrong to load the save without it. Otherwise older builds of format 28 skip it.
- **Read order.** A section that indexes another's lists must come after it. `physics` comes after `props`.
- **Retired names.** Never reuse a retired name for different content.
- **Where the code lives.** Sections run as `Game` code. Register from a `Game` static member to reach private state, or keep the state behind public accessors.

### The stores (`entities`, `variables`)
For the map format's named entities, their `p.` custom properties changed at runtime, and map variables scripts set:

```cpp
g.setEntityProperty(chunk, "door_lab", "p.locked", true);                      // chunk -1: the whole campaign
g.setEntityProperty(chunk, Game::entityIdKey("prop", 17), "p.mass", 2.5);      // an entity without a name: "#prop:17"
const SaveValue* v = g.entityProperty(chunk, "door_lab", "p.locked");
const SaveStore::Properties* all = g.entityProperties(chunk, "door_lab");    // every override, to apply when the entity spawns
g.clearEntityProperty(chunk, "door_lab", "p.locked");
g.setMapVariable(-1, "alarm_count", std::int64_t(3));  g.mapVariable(5, "lights");  g.clearMapVariable(5, "lights");
```

- **Values.** `SaveValue` is `std::variant<bool, std::int64_t, double, std::string>`.
- **Limits.** Names are 1-128 bytes with no control characters. Text is at most 4096 bytes. Doubles must be finite. The store holds at most 65536 entities of 256 properties each, and 65536 variables.
- **Refusals.** A `set` past any limit returns false.
- **Keys.** Keys are per map (chunk index), so entity names need only be unique within their map.
- **Order.** The stores are ordered maps, so their bytes are deterministic.
- **Who applies them.** The store only keeps the values. The system that spawns an entity reads its overrides when it does.

### Window walls and breaches
A `GLAZING` record's glass panes are breakables after the map's own `BREAKABLE`s, numbered in record order and bay by bay, so the `world` section's broken breakables, pane damage and glass hits address them like any other pane. A save stays exact while the map's `GLAZING` records stay as they were. A `BREACH` keeps nothing at run time: the hole is the map's, not the save's. Hanging creepers keep no state either; their swing is the renderer's.

### The physics section
`PropBody` holds a prop's orientation quaternion (x, y, z, w; unit), its angular velocity (rad/s, world axes) and its sleep island (-1 for none). The solver sets `physicsSaveHooks()`:
- `capture(const Clutter&, PropBody&)` returns true for a prop that has solver state;
- `restore(Clutter&, const PropBody&)` puts it back on load.

The section is written per chunk, by each prop's index in the chunk's props list. It is read after `props` and before the reconciliation, which keeps the props' fields.

When writing, a quaternion is normalised (a degenerate one becomes the identity) and spin is clamped to 1000 rad/s. When reading, anything not near unit or out of range is refused. Without the hooks, the section is not written, and a saved one is read, checked and dropped.

## Refitting older saves (formats 1-27)
A format 1-27 save loads in two stages:
1. **The frozen positional reader.** It leaves a field the version lacked at the fresh game's value.
2. **The upgrade chain** (`Game::refitLegacySave`). Each step runs when the save predates its version, oldest first:

| Step to | What it does |
|---|---|
| 2 | The shotgun tube holds what it can of the rounds, with no reload under way. |
| 19 | The shotgun is in hand, with no switch under way and an empty 9 mm pool. A reload under way is the shotgun's. The 9 mm stack takes the first free 1x2 cell. |
| 20 | Recognition is rebuilt from the hunt: a creature on a Chase has the player made out. |

Every load, of any format, then turns a broken crate's old quartet of junk props (kinds 0-3) into splinters, as loads did before format 28.

The reconciliation that follows is version-aware too:
- from version 15, it wakes creatures that were never disturbed;
- a save before 8 gets its props by kind;
- chunks the save never carried load fresh.

**The rewrite.** After a save of format 1-27 loads from a file (a slot or the autosave, in place or on the load worker), `Game::refitSaveFile` does the following:
1. **Keeps the original** beside it as `<file>.v<old>.bak`, for example `slot-1.rms.v16.bak`. If that name already holds something else, it uses `.v16-2.bak`, `-3` and so on. A backup is never written over another file. An existing backup with the same bytes is reused.
2. **Writes the loaded game as format 28**, but only after that new form has loaded back. The swap is atomic, and the `.label` sidecar is rewritten too.
3. **Logs it.** RawMetal-load.txt gets the line `save refit: <path> was format 16, rewritten as format 28; the original is kept as slot-1.rms.v16.bak`.
4. **Shows the HUD notice** `SAVE UPDATED TO THE NEW FORMAT / OLD COPY KEPT`.

If any step fails (no backup could be written, the new form did not load back, or the write failed):
- the original file stays as it was, and the loaded game is untouched;
- RawMetal-load.txt says `save refit skipped: ...` with the reason;
- the notice reads `OLD SAVE LOADED / NOT UPDATED (SEE RAWMETAL-LOAD.TXT)`.

The next load tries again. A slot label preview never refits.

## Where saves live
Saves go in `%LOCALAPPDATA%\RawMetal\saves`: `slot-1..3.rms`, `checkpoint.rms`, `.label` sidecars and refit backups. Settings go in `%LOCALAPPDATA%\RawMetal\settings.ini`.

Builds 1.0.4 to 1.5.0 lost the backslashes in those paths and wrote `%LOCALAPPDATA%RawMetalsaves` and `%LOCALAPPDATA%RawMetalsettings.ini` instead. At start-up, `Game::adoptMisplacedSaves` copies each file from there into the right place, under these rules:
- **Newer file in the right place.** If the right place already holds a file as new or newer, that file is kept.
- **Older file in the right place.** It is first kept as `<file>.older.bak` and then replaced.
- **The old folder** is left as it was.
- **Repeat starts.** Copies keep their times, so a second start adopts nothing more.

## Rules
- **Older saves load and are refitted.** Every format from 1 is read; formats 1-27 go through the chain above, then the rewrite.
- **Newer saves are refused.** A build refuses a format above its own, and says why. Within format 28, a newer section is skipped unless it is flagged `Required`. Raise the format number only when the container itself changes.
- **The map wins over the save for what was authored.** On load, the saved props, creatures and doors are matched against today's authored map by model and home:
  - an authored prop the save does not carry comes back where it was put;
  - a saved prop the map no longer authors is dropped;
  - chunks the save never carried are loaded fresh.
- **Writing an older format is for tests only** (`archiveSave(writer, N)` for N from 1 to 27). The writer leaves out what that version could not hold, such as physics props before version 17.
- **Bounds.** Every count and value is checked on load. A bad one refuses the save with the reason, rather than loading a broken game. Lists hold at most 1024 entries, unless they say otherwise.

## Tests
`Game::testSaveFormat` runs as five lab tubes, each under the lab's ten minutes:
- **`engine.save-refit-room`**: formats 12-27 on a custom room.
- **`engine.save-format`**:
  - the load paths: a backup beside an existing `.bak`, a failed rewrite, a reload, a manual slot, the worker thread;
  - damaged, truncated, duplicate, required and unknown sections;
  - the stores, a registered section with an upgrade, and the physics hooks;
  - the misplaced-folder adoption, in a temp folder.
- **`engine.save-refit-early`** and **`engine.save-refit-late`**: the main campaign at formats 1-6, then 7-11, 19 and 27. With `RM_SAVE_FORMAT_ALL=1`, they cover 1-13 and 14-27, which takes longer than a tube is allowed.
- **`engine.save-real`**: real saves from past releases, copied to a temp folder. It looks in the `%LOCALAPPDATA%` save folders, in `lab/fixtures/saves` (a format 16 and a format 25 autosave, and a format 24 slot from v1.4), and in `RM_REAL_SAVES=dir;dir`. The originals are checked unchanged afterwards.

In every refit test, each save loads, is refitted with its backup, keeps its game, and re-saves byte-exact.

`tools/dev/savedump.py PATH` lists a save's sections, their versions and sizes, and whether each checksum holds.
- `engine.saves`, `engine.prop-saves`, `engine.handgun` and `engine.glass` keep their checks.
- `engine.load-saves` loads every `.rms` in `RM_LOAD_SAVES`.

## History
| Version | Release | What it added |
|---|---|---|
| 4 | | stalk state (older saves fall back to defaults) |
| 6 | | more than the original four chunks; chunks a save lacks load fresh |
| 14 | | creatures spawned by scripts |
| 15 | | a sleeping creature's wake switch |
| 16 | | destroyed ceiling tiles; a creature's home height, size, origin chunk and surface mode |
| 17 | | broken breakables; physics props (clutter kind 7: model, damage, authored, broken, home) |
| 18 | | ages of timed world state |
| 19 | | the handgun's ammo pool, the firearm in hand, a weapon switch under way and the reload's length |
| 20 | | how far each creature has made the player out |
| 21 | | which cable leads have torn |
| 22 | | a recurring hunter's name and prey, and its walk in through a seam |
| 23 | | the Machine creature kind |
| 24 | | where a hand left a door leaf, and the battering a door has taken |
| 25 | | what a debris shard broke from, so it draws as a piece of it |
| 26 | v1.4.8 | No new fields. Chairs standing on a floor are now loose physics props, carried in each chunk's clutter list. An older build would draw them twice (its fixed chair and the saved loose one), so it refuses a 26. A version 25 or older save loads in v1.4.8 with every chair back where it was put. |
| 27 | v1.5.0 | Each glass pane's hits (where, hole radius, crack strain; one number per hit), so its holes and fallen pieces come back exactly. Glass shards are `ShardOf::Glass`. The last positional format. |
| 28 | | The section container. Sections: `game`, `player`, `inventory`, `hazmat`, `script`, `world`, `creatures`, `pickups`, `props`, then the optional `entities`, `variables` and `physics`. Formats 1-27 are refitted on load and rewritten, the original kept as `.v<N>.bak`. Saves and settings return to `%LOCALAPPDATA%\RawMetal`. |

## Map format, for the v1.4.8 change
Custom maps (`.rmc`) are unchanged for existing content.
- **Chairs.** `FIXTURE` records for office, wooden and boardroom chairs standing on a floor (base 0) become loose props when the map loads. The optional 14th field `anchored`, set to 1, keeps a chair fixed.
- **Reference.** The record reference is `custom maps/GUIDE.html`, generated by `tools/campaign/make_custom_map_guide.py`. The Campaign Builder reads its record fields from that file, so the new field is editable there too.
