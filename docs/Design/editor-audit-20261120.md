# Campaign Builder audit, 2026-11-20

Scope: tools/Level Editor/editor (NativeEditor.cpp and its .inc files), the loader (src/world/CustomCampaign.cpp)
and the guide that gives the Builder its schemas (custom maps/GUIDE.html, from tools/campaign/make_custom_map_guide.py).
Method: four sweeps that stay as checks, so a new tool, record kind or field is covered by being there.

- `RawMetalEditor.exe --audit-test` (tools/Level Editor/editor/NativeAuditRegression.inc, about 40 s; `--editing-test` runs it too):
  schema parity, every placing tool in `toolInfo` on floors 0, 1 and 2 of map 0 and floors 0 and 1 of map 2, the inspector,
  the input guards, malformed files. Failures of the placing and inspector sweeps are collected and reported together.
- `lab.py run --feature=engine.map-format,map-format` (src/game/MapFormatTest.cpp): the Builder's limits table
  (src/world/CampaignFieldRanges.h) is loaded by the loader itself, row by row.

Severity: crash / data loss / wrong result (the game refuses the map or the thing is not where it should be) / papercut.

## Fixed

| # | Sev | Finding | Fix |
|---|-----|---------|-----|
| 1 | wrong result | A place name or darkness drawn on an upper floor was written without heights, so it covered every floor of its box (a darkness on floor 1 also darkened floor 0, a place name showed on all floors). Sibling of the upper-floor pickup. | `authorKind`: DARK and ZONE get that floor's bottom and top above the ground floor. Checked for every tool by the placing sweep. |
| 2 | wrong result | The Pipe tool ran a ceiling pipe at floor + 2.6 m; a top floor made by Add floor has 2.4 m of headroom, so the pipe stuck through the ceiling. | Run kept under the room's ceiling. |
| 3 | wrong result | Guide rows stopped short of what the loader reads, so the Builder wrote nothing for them and showed them only as "Engine field N": ACTOR dropsSidearm, LIGHT shadows and prop, TERMINAL readable, taped, surface, reactor, HAZARD state/value/invert/period/onTime/phase, SCRIPT_TRIGGER once, TIMED_SEQUENCE soundX/Y/Z, BREAKABLE fixture and part, COMPACTOR stop. The WET row was joined with "or" instead of "+", so it parsed as one garbage schema. | Rows corrected in make_custom_map_guide.py, GUIDE.html regenerated. Plain labels for LIGHT shadows, TERMINAL readable and taped. A check compares each row's reach with the loader's. |
| 4 | wrong result | CRATE, CRATE_PLATE, CRATE_RESET, NAV_HINT, HUNG_SUIT and CHAIN had no row, so the Builder could not list, pick, move or delete them. | Rows added. (No placing tool: see Remaining.) |
| 5 | wrong result | The number-box guard only protected a box that already held a number. A typo (`1d`) in an empty optional box (a lamp's flicker, a door's strength) was saved and the game refused the map. | The guard also covers boxes that are numbers by name or by the limits table. |
| 6 | wrong result | Numbers the loader refuses were saved: warmth over 12000, tint or flicker past 1, door strength, creature scale, a joint's friction over 5000 (the artists' "12" case was the named-field half), cloth mass, spawn team and the rest. | One table, src/world/CampaignFieldRanges.h (53 fields, positional and named): the inspector brings a number to the nearest the loader takes and says so. The table is loaded by the loader in the engine test, both ends and a step beyond. |
| 7 | wrong result | Names the loader refuses: a trigger or state name with a space or odd character (letters, digits and `_ - . /` only, 96 long), a place name with `\|` or `%` (the loader never decodes it, so it shows `%7C`), objective text that is not printable ASCII, a door label over 24. | Text rules in the same table: spaces become `_`, forbidden characters go, length is cut, with a word. Loader-tested the same way. |
| 8 | wrong result | The 65th SPAWN, EMITTER or DARK on a map, the 129th trigger: the loader refuses the whole map. | Record caps in the table; the tools stop with "The game takes at most N of these". Loader-tested. (GROUND and TERRAIN_FLAT caps are in the table and loader-tested, but their tools do not check yet.) |
| 9 | wrong result | A picked film camera lost its selection after any edit (nudge, R, drag): `repairSelection` treated its `# EDITOR_FILM` line as a comment. | Film cameras count as records there. |
| 10 | papercut | Page Up did nothing to a health or ammo pack on the ground floor (no height to raise). | It takes the floor's height first, then rises. |
| 11 | papercut | Ctrl+D on a trigger said "Copy supports placed objects". | A trigger copies with its gates, its firing and its actions (named `<id>_copy_N`). |
| 12 | papercut | Ctrl+D on a person did the same. | The person is copied whole (poses, guard post, the rest), as a picked group is. |

## Checked and sound (the sweeps pass)

- Every placing tool (30 with a record of its own, 155 placements in 5 contexts): record on the right map and floor marker, at its floor's
  height, the ghost equal to what is placed field for field and standing on that floor, pickable, nudged by the arrows, Page Up, turned,
  copied, deleted, undone and redone (Undo takes the placement back to the identical lines). Two ground tools (Grass & dirt, Flatten)
  turn the map outdoors and are driven by the Outdoors checks.
- Picking any record kind of the Starter Map and the Sample Facility in the inspector and leaving it changes nothing; each map saves
  and opens again byte for byte; an unknown future field, a named field and a comment survive an edit of another field.
- Malformed, empty, binary, truncated, 5 MB, BOM, CRLF, spaced and Unicode-named files: opening never crashes the Builder; a good map
  opens as written afterwards. Saving over a read-only or held-open file keeps the old file and leaves no temporary file.
  12 maps of one name and 32 floors (the 33rd is refused with a word).
- Lists the tools offer (pickups 7, creatures 9, hazards 8, loose props 8) equal the engine's enums.
- Read, not run: New, Open, Open from campaign and window close all ask "Keep your changes?" (Yes saves, No discards, Cancel stays);
  saves are atomic (temp file then replace) and keep a recovery copy until the save succeeds; autosave, the crash log and recovery
  exist (NativeStability.inc, `--crash-test`, `--recovery-test`, `--soak-test`). Undo keeps 50 whole-document snapshots.

## Remaining (not fixed), with size

| Sev | Finding | Size |
|-----|---------|------|
| papercut | Hazard, Place name, Darkness, Trigger, Emitter, Sound, Window wall, Breach, Cloth and Joint records have no height of their own the arrows can move: Page Up and Page Down do nothing to them (list printed by the audit). A box could raise bottom and top together. | S |
| papercut | R turns nothing for boxes without a facing (hazard, zone, darkness, water, creature, pickup, pipe, cable...). Creatures and pickups could take a facing field; boxes could swap their sides. | S |
| papercut | The 51st Undo step drops the oldest silently. Say so in the status line. Snapshots are whole documents (Starter Map x 50). | S |
| papercut | Ctrl+C is Duplicate, not copy; there is no Ctrl+V. Surprising for anyone used to clipboards, and the Copy area / Paste area tools are the real copy. | S |
| papercut | The Builder is not DPI aware: on a scaled display Windows stretches it (blurry, never broken). Per-monitor awareness needs every pixel constant in layout() made scalable. | M |
| papercut | Opening a file that is not a campaign adopts it as an empty document rather than refusing with a word (no crash, nothing lost: the old map was already offered Save). A "this is not a campaign" check on open, using the loader's marker rules. | S |
| papercut | The inspector's SCRIPT_ACTION rows: a MESSAGE or RAW action shows the first row's labels (name, value). The Trigger page edits actions in plain words; only Advanced is affected. | S |
| wrong result (rare) | GROUND and TERRAIN_FLAT beyond 1024 and 512 are refused by the loader; their tools do not stop at the cap. | S |
| gap | No tool or inspector page for: crush-the-crate puzzles (CRATE, CRATE_PLATE, CRATE_RESET), NAV_HINT (path and cover points for the AI), HUNG_SUIT, CHAIN, WET (puddle and wet patches), SHELTER, STORM (outdoors weather), CARGO_LIFT (no guide row yet), TIMED_SEQUENCE and SEQUENCE_CUE (clocks and cues outside the Person tool), DIALOGUE and DIALOGUE_REPLY trees, VISIBLE_GROUP, MAP haze and wetness, OBJECTIVE_TEXT. They survive a round trip untouched and (for those with rows) can be picked and edited under Advanced. | one small tool each (S to M); a "World" page for MAP haze, wetness, storm and shelter M; a dialogue editor L |
| gap | Switching (state, value, invert, period, onTime, phase, flicker) for lamps, hazards and emitters exists only as raw fields under Advanced; a "Behaviour" box in plain words (always on, flickers, blinks every N seconds, on when switch X) would cover all three. | M |
| gap | ACTOR_* detail records (AI mode, faction, guard post, tactics, role, stance, voice, health, suspend, prelude, platform) have no guide rows; the Person tool writes some, the rest are hand-edited. | M |

Out of reach of this audit (not driven): the live 3D picture of each placement (the ghost's outline heights are checked; the engine's
render is not), modal prompts (read, see above), window resize at odd sizes and DPI.
