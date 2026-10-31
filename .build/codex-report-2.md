# Physics and Chapter 2 completion — v1.1.5

Gameplay checkpoint: 4a82d3e. Last code change: 206e2f5; the only subsequent code change equips and verifies the shotgun in the inspection-only --map-shots fire branch. Completed conversion: e147408; release integration: 2ff00a2. Clean Release build: a5cff59; subsequent report-only commits do not alter compiled inputs.

The interrupted physics stages and Chapter 2 facing review have been completed. This report supersedes the previous placement-conflict handoff; the original reports and intermediate failed runs remain in local recovery evidence.

## Behavior delivered

- Existing portable dressing becomes physical across the campaign; authored legacy litter remains in its original kind system. Props can be lifted or pushed according to mass, thrown, shot, kicked, blasted, carried by lifts and swept by doors. Their position and damage persist in saves and across streaming.
- Glazing is split into independently damageable bays. Protective panes stop bullets, crack, and heal over seconds 20–21 after the last hit; ordinary broken glass stays broken. Noise, attribution, AI hearing, grenade chains, truce consequences and supported furniture contacts use the existing game systems.
- Bounded fixture destruction, plywood and ceiling breakables, material sounds and custom-map/editor physics records are implemented. The existing campaign gains no new destruction test partitions or ceiling tiles.
- Recessed lamp self-shadowing, dark glass blending, authored route paint, destination-lock mirroring and the rebuilt Triage beat are corrected.
- Chapter 2 theatre seating, waiting rows, boardroom chairs, booth terminals, staff-room chairs and ward-bed facing are corrected. The great-door state remains the Chapter 2 entry gate.

## Recovery corrections and deliberate deviations

The user authorized completion after the outstanding placement failures and need for corrections were explained. Literal authoring calls were corrected in their intended rooms rather than weakening audits or adding item-search postpasses.

- Added measured shelf and furniture support profiles, oriented prop footprint queries, SAT overlap checks, a 1 mm support-edge tolerance, and a medical-cart tray surface below its handles. Runtime and audit use the same prop support geometry. Added regressions for shelf wake/movement, rotated tabletop contact, flush crate corners, moving-cart loads, and NPC-owned shattering.
- Aligned stacked cartons, moved desktop items apart, centered shelf contents, and placed oversized shelf cartons on adjacent clear floor. Corrected seeded paper placement before creation and ordered the Imaging paper drift after its supporting room geometry. The desktop sets and intended room roles are retained; blocked random paper candidates are rejected during creation.
- Restored the missing Map 42 operator desk under its pre-existing desktop equipment, using a native-proportioned model. Retained suspended cargo and IV bags as attached fixtures; retained the strapped hold beds with their fixed restraint geometry. These are authored attachment cases, not general conversion exceptions.
- Kept the final author policy for the Map 70 anteroom pane: protective. The protected interlock, lead glass, records-office and Triage hatch panes remain protective. New glass-only reachability in 68/69/72 belongs to explicitly allowed local route/scare panes, with locked-state comparisons recorded in every audit.
- Expanded the model-prop dimension bound to 3 m for requested rolling furniture, retaining the 60 kg audit limit and 25 kg lifting limit. Native procedural models use curated nominal sizes; catalog-backed model proportion checks remain active.
- Fixed the reactor mounting regression to query the ceiling from each emitter, so a new transom below a lamp cannot hide its actual ceiling. This keeps the 15 cm mounting requirement and is stricter than accepting a ceiling found from an unrelated storey sample.

Detailed local correction notes: recovery-20261031/legacy-placement-notes.md, visual-review.md, glazing-safety-notes.md and literal-corrections.json. Asset-library content was referenced through the existing build; no purchased source assets were modified.

## Verification limits

Automated logic, save, collision, renderer, authoring and static-view checks are distinguished from ordinary play and headset testing. No manual VR headset/controller session or complete ordinary campaign playthrough is claimed. Native renderer timing is CPU wall time, not GPU timestamp data.

## File-by-file changes

| File | Change |
|---|---|
| `src/assets.rc` | Resources 2257–2259 reference the existing wood break, wood impact and plaster break sounds. |
| `src/audio/Sound.h` | Append sound IDs; sound-event NPC owner. Existing arithmetic resource mapping already covers the appended resources. |
| `src/game/CampaignMapTest.cpp` | Chapter 2 seal regression and rebuilt nurse/patient Triage beat. |
| `src/game/Clutter.cpp` | Healing, blasts, actor/VR contacts, thrown hits, door sweeps, drag, substeps, attributed secondary impacts, sleeping fast path, destruction and inspection/regression scenes. Shared oriented support/footprint queries, moving-cart trays, settling tolerance and workstation-preservation regressions. |
| `src/game/Game.cpp` | Age breakables each update and route barrel explosions through prop/pane blast logic. |
| `src/game/Game.h` | Physics entry points, transient contact state, owner fields and stage tests. |
| `src/game/MapAudit.cpp` | furniture-profile-aware overlap checks, native-model proportion policy, side-supported hatch frames, movable-prop route treatment and explicit glass-only/locked-state reachability reports. All final campaign placement audits pass. |
| `src/game/Mechanisms.cpp` | Cargo-platform prop carry; binds lamp power to the authored prop identity. |
| `src/game/Movement.cpp` | Heavy-prop approach collision, safe retreat and push-attempt tracking. |
| `src/game/Saves.cpp` | Version 18 healing-age archive; extends model-prop dimensions for requested rolling furniture. |
| `src/game/Streaming.cpp` | Destination lock checks on every boundary-door mirror path. |
| `src/game/ai/Hearing.cpp` | New impact sounds, event-owner stimuli and hearing-only intact-glass occlusion. |
| `src/game/ai/enemy/EnemyAI.cpp` | Acoustic consumers use hearingRayClear; sight keeps its existing path. |
| `src/game/ai/grenades/TacticalGrenades.cpp` | Grenade blasts reach model props, glazing, barrels and crates. |
| `src/game/ai/guard/Mind.cpp` | Ignore own-squad attributed prop impacts; glass muffles hearing only. |
| `src/main.cpp` | Physics/glazing test registration; fixture captures; inspection fire equips a shotgun, checks ammunition use and logs every shot. |
| `src/renderer/GpuRenderer.cpp` | Per-light shadow flag and housing volume, explicit liquid materials and matching shader data. |
| `src/renderer/GpuRenderer.h` | Expanded lamp input record. |
| `src/renderer/Performance.cpp` | Transmission measurement and cache/pixel assertions for breakable fixture parts. |
| `src/renderer/Scene3D.cpp` | Healing opacity, authored-route paint policy, light shadow option, destruction rendering and independently cached bounded fixture-part clipping. |
| `src/renderer/SoftwareRenderer.cpp` | Clean glass tint/alpha, explicit water material flag and expanded per-light input. |
| `src/renderer/SoftwareRenderer.h` | Explicit liquid flag and focused hardware transmission-test option. |
| `src/renderer/shaders/lampshadow.frag` | Discard only the light’s own recessed housing volume; retain legacy near-emitter exclusion for original fittings. |
| `src/renderer/shaders/lampshadow.vert` | Matching extended shadow push constants. |
| `src/renderer/shaders/scene.frag` | Explicit liquid tag replaces gloss-based liquid inference. |
| `src/world/Ballistics.cpp` | Protected-glass aging, material health defaults and solid stage-3 trace handling. |
| `src/world/CampaignChapterMachineHall.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CampaignChapterMaintenance.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CampaignChapterMedicalFront.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CampaignChapterPerimeterA.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CampaignChapterPerimeterB.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CampaignChapterPower.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CampaignChapterResearchFront.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CampaignChapterWater.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CampaignKit.h` | Destruction helpers, fixture parts, shadow option; preserves exact workstation sets, loose paper, posed conversions and per-bay glazing. |
| `src/world/CampaignMaps.cpp` | Literal portable-item and glazing conversions with author protection/trigger choices, existing legacy clutter, and documented support/clearance corrections. |
| `src/world/CustomCampaign.cpp` | Model CLUTTER and BREAKABLE record loading and terrain-grounded custom-map physics. |
| `src/world/ModelIds.h` | Regenerated from the tracked physics rows, not hand-edited. |
| `src/world/World.cpp` | Furniture support/clearance profiles and validation, lamp source placement, terrain-grounded breakable data; includes prop dimensions, wheelchair seat and lamp bindings. |
| `src/world/World.h` | Age storage, fixture-part queries and profile APIs; shadow option; lamp/prop authoring identity. |
| `src/world/WorldDefinition.h` | Wood/plastic impact and break voices. |
| `tools/campaign/prop_physics.json` | Curated portable/furniture rows and nominal native-model sizes. |
| `tools/level-editor/README.md` | Physics fields and custom-map record syntax. |
| `tools/level-editor/index.html` | Loose model, breakable material, protective pane and break-state controls. |
| `tools/level-editor/runtime_records.py` | Bounded preservation of CLUTTER and BREAKABLE records. |
| `tools/level-editor/server.py` | Model CLUTTER, BREAKABLE boxes and smart-window glazing export. |
| `tools/level-editor/test_map_payload.py` | Physical-model and breakable-box export round trip. |
| `tools/level-editor/test_runtime_records.py` | Exact record preservation through project/runtime export. |

| `src/game/Reactor.cpp` | Ceiling mounting regression queries support from each emitter height. |
| `src/app.rc` | Embedded file/product version 1.1.5.0. |
| `RELEASE_NOTES.md` | Player-facing v1.1.5 changes and save compatibility. |


## Build and executable identity

Normal Build.cmd completed with 1,147 packaged resources, 116 neural reconstructions, 19 animation clips and 193 screened WebP textures. Embedded version: 1.1.5.0. Size: 47,415,808 bytes, below the 67,108,864-byte cap.

SHA-256: `073fc611af0fe870b945acaaf36bc99f3007d489f19e6ee21557b91bfe5d369e`. The build log is `N:/rawmetal/.build/recovery-20261031/build-current.log`. Final packaging and the installed copy use this exact executable; their hashes are checked again during delivery.

## Tests and result lines

Evidence root: `N:/rawmetal/.build/recovery-20261031/`. All 22 required/supplemental command invocations have a passing run. The broad suites and all-map audits used the gameplay checkpoint 4a82d3e (SHA-256 `9200f5f439be53ec3cb3a89de78f8ca19ecee3111dd2510c124af1808665668c`). On the final executable, the inspection branch, prologue, props, glass, physics-stages and command-arguments checks were rerun successfully. The later code change affects only the inspection firing branch; gameplay and test dispatch are unchanged.

The clean rebuild differs from the inspected/performance candidate only in three PE linker timestamp fields. After zeroing those timestamp fields, the complete executable images match byte-for-byte, including machine code, data and embedded resources. Details: rebuild-equivalence.json. Prologue (22/22), props, glass, physics-stages and command-arguments were also rerun on the exact final hash and all returned 0; raw matching-hash records are in release-final.

| Command | Exit/result | Seconds | Evidence below root |
|---|---|---:|---|
| `--each-test=props` | 0 | 1.72 | `validation-final/each-test-props` |
| `--each-test=glass` | 0 | 1.99 | `validation-final/each-test-glass` |
| `--each-test=physics-stages` | 0 | 0.72 | `validation-final/each-test-physics-stages` |
| `--each-test` | 0 | 238.3 | `validation-final/each-test` |
| `--smoke-test` | 0 | 218.84 | `validation-final/smoke-test` |
| `--campaign-map-test` | 0 | 44.54 | `validation-final/campaign-map-test` |
| `--chapter-route-test` | 0 | 10.07 | `validation-final/chapter-route-test` |
| `--prologue-test` | 0, 22/22 on final executable | — | `release-final/prologue-test.txt` |
| `--exclusion-test` | 0 | 137.6 | `validation-final/exclusion-test` |
| `--vulkan-test` | 0 | 113.97 | `validation-final/vulkan-test` |
| `--guard-ai-test` | 0 | 18.32 | `validation-final/guard-ai-test` |
| `--guard-combat-test` | 0 | 2.32 | `validation-final/guard-combat-test` |
| `--guard-pursuit-test` | 0 | 5.14 | `validation-final/guard-pursuit-test` |
| `--guard-policy-test` | 0 | 2.95 | `validation-final/guard-policy-test` |
| `--guard-tactics-test` | 0 | 1.46 | `validation-final/guard-tactics-test` |
| `--stealth-test` | 0 | 2.77 | `validation-final/stealth-test` |
| `--hearing-test` | 0 | 0.41 | `validation-final/hearing-test` |
| `--tactical-grenade-test` | 0 | 2.95 | `validation-final/tactical-grenade-test` |
| `--save-test` | 0 | 49.29 | `validation-final/save-test` |
| `--content-integrity-test` | 0 | 2.02 | `validation-final/content-integrity-test` |
| `--engine-regression-test` | 0 | 2.48 | `validation-final/engine-regression-test` |
| `--creature-behaviour-test` | 0 | 7.42 | `validation-final/creature-behaviour-test` |

The broad runner returned exit 1 because its first standalone prologue run could not create a checkpoint under the sandbox system temporary directory. The same binary passed outside that restriction and with TEMP/TMP set to a workspace directory. The final executable also passes 22/22 with workspace-local TEMP/TMP. Raw exits.json is retained unaltered; passing final-hash retries are in release-final.

```text
validation-final/each-test/each-test.txt: ALL PASSED 0
PASS guard-kicked fragile prop retains NPC ownership when it shatters
PASS bookcase binders retain their measured shelf support after wake and motion
PASS rotated prop moves and settles on its oriented tabletop footprint
PASS medical cart supports its tray load and wakes it to fall when rolled away
PASS flush crate corners retain support at floating point boundaries
ALL PASSED physics-stages
PASS holds stays at stage 2 at 19 seconds
PASS holds heal timer survives byte-exact save
PASS holds intact at 21 seconds with no pane decals
PASS breakable glass never heals
ALL PASSED glass
Glazing linear-light transmission 0.773372 direct 227 through 218
Glossy glazing preserves 70-80 percent of the room brightness: PASS
Sleeping model props reuse their per-cell cache with stable pixels: PASS
Breaking panes rebuilds only the breakable cache and never the main chunk: PASS
One pane break adds under 4 ms CPU scene time: PASS
Breaking a fixture screen updates its bounded part cache without rebuilding the chunk: PASS
PASS: Vulkan material / depth / clipping checks
release-final/prologue-test.txt: 22/22 checks passed
```

Editor record tests: 7 tests, OK (`editor-runtime.txt`); map payload export: PASS (`editor-payload.txt`). Edge checker over maps 34-74: `edges ok` (`edge-check.txt`).

## Campaign placement and glass reachability

Every map below returned exit 0 and `PASS: 0 placement problems`. Counts are the audit result lines for open-state glass-only cells and locked-state glass reachability. n/a means the audit emits no such line because that map has no relevant breakable glass; full raw reports remain under validation-final/audits/map-N.

| Map | Placement result | Glass-only cells | Locked-state cells |
|---:|---|---:|---:|
| 0 | PASS: 0 placement problems | 0 | 0 |
| 1 | PASS: 0 placement problems | 0 | 0 |
| 2 | PASS: 0 placement problems | 0 | 0 |
| 3 | PASS: 0 placement problems | 0 | 0 |
| 4 | PASS: 0 placement problems | n/a | n/a |
| 5 | PASS: 0 placement problems | n/a | n/a |
| 6 | PASS: 0 placement problems | n/a | n/a |
| 7 | PASS: 0 placement problems | 0 | 0 |
| 8 | PASS: 0 placement problems | 0 | 0 |
| 9 | PASS: 0 placement problems | 0 | 0 |
| 10 | PASS: 0 placement problems | 0 | 0 |
| 11 | PASS: 0 placement problems | n/a | n/a |
| 12 | PASS: 0 placement problems | n/a | n/a |
| 13 | PASS: 0 placement problems | 0 | 0 |
| 14 | PASS: 0 placement problems | n/a | n/a |
| 15 | PASS: 0 placement problems | 0 | 0 |
| 16 | PASS: 0 placement problems | 0 | 0 |
| 17 | PASS: 0 placement problems | n/a | n/a |
| 18 | PASS: 0 placement problems | n/a | n/a |
| 19 | PASS: 0 placement problems | n/a | n/a |
| 20 | PASS: 0 placement problems | 0 | 0 |
| 21 | PASS: 0 placement problems | 0 | 0 |
| 22 | PASS: 0 placement problems | n/a | n/a |
| 23 | PASS: 0 placement problems | n/a | n/a |
| 24 | PASS: 0 placement problems | n/a | n/a |
| 25 | PASS: 0 placement problems | n/a | n/a |
| 26 | PASS: 0 placement problems | n/a | n/a |
| 27 | PASS: 0 placement problems | n/a | n/a |
| 28 | PASS: 0 placement problems | n/a | n/a |
| 29 | PASS: 0 placement problems | n/a | n/a |
| 30 | PASS: 0 placement problems | n/a | n/a |
| 31 | PASS: 0 placement problems | n/a | n/a |
| 32 | PASS: 0 placement problems | 0 | 0 |
| 33 | PASS: 0 placement problems | n/a | n/a |
| 34 | PASS: 0 placement problems | 0 | 0 |
| 35 | PASS: 0 placement problems | n/a | n/a |
| 36 | PASS: 0 placement problems | n/a | n/a |
| 37 | PASS: 0 placement problems | n/a | n/a |
| 38 | PASS: 0 placement problems | n/a | n/a |
| 39 | PASS: 0 placement problems | n/a | n/a |
| 40 | PASS: 0 placement problems | n/a | n/a |
| 41 | PASS: 0 placement problems | 0 | 0 |
| 42 | PASS: 0 placement problems | 0 | 0 |
| 43 | PASS: 0 placement problems | n/a | n/a |
| 44 | PASS: 0 placement problems | n/a | n/a |
| 45 | PASS: 0 placement problems | n/a | n/a |
| 46 | PASS: 0 placement problems | n/a | n/a |
| 47 | PASS: 0 placement problems | 0 | 0 |
| 48 | PASS: 0 placement problems | n/a | n/a |
| 49 | PASS: 0 placement problems | 0 | 0 |
| 50 | PASS: 0 placement problems | n/a | n/a |
| 51 | PASS: 0 placement problems | n/a | n/a |
| 52 | PASS: 0 placement problems | 0 | 0 |
| 53 | PASS: 0 placement problems | 0 | 0 |
| 54 | PASS: 0 placement problems | 0 | 0 |
| 55 | PASS: 0 placement problems | 0 | 0 |
| 56 | PASS: 0 placement problems | n/a | n/a |
| 57 | PASS: 0 placement problems | 0 | 0 |
| 58 | PASS: 0 placement problems | 0 | 0 |
| 59 | PASS: 0 placement problems | 0 | 0 |
| 60 | PASS: 0 placement problems | 0 | 0 |
| 61 | PASS: 0 placement problems | 0 | 0 |
| 62 | PASS: 0 placement problems | n/a | n/a |
| 63 | PASS: 0 placement problems | 0 | 0 |
| 64 | PASS: 0 placement problems | 0 | 0 |
| 65 | PASS: 0 placement problems | 0 | 0 |
| 66 | PASS: 0 placement problems | n/a | n/a |
| 67 | PASS: 0 placement problems | n/a | n/a |
| 68 | PASS: 0 placement problems | 30 | 30 |
| 69 | PASS: 0 placement problems | 27 | 27 |
| 70 | PASS: 0 placement problems | 0 | 0 |
| 71 | PASS: 0 placement problems | 0 | 0 |
| 72 | PASS: 0 placement problems | 2 | 2 |
| 73 | PASS: 0 placement problems | 0 | 0 |
| 74 | PASS: 0 placement problems | n/a | n/a |
| 179 | PASS: 0 placement problems | n/a | n/a |

The nonzero deltas in 68 (30), 69 (27) and 72 (2) are explicitly authored local breach/scare routes between spaces already connected without a locked state. The Chapter 2 seal, clearance gate, Investigator departure, protected hatch, records and lead-glass barriers retain protective panes or solid wall returns. Owning state/door analysis is recorded in glazing-safety-notes.md; aggregate counts alone are not treated as proof of a sequence break.

## Prop performance

Inspected executable with machine code/data identical to the final clean rebuild, leaf timers enabled. Times below are CPU wall milliseconds per frame. Update-only scenes use 60 Hz simulation; rendered pairing uses Vulkan, RTX 5060 Ti, linear HDR, 4x MSAA, 640x360 and a hidden presented window.

| Scene | Prop update mean / p95 | Whole update mean / p95 / max |
|---|---|---|
| 150 resting props | 0.043 / 0.065 ms | 0.510 / 0.725 / 0.929 ms |
| 40-prop shove | 0.200 / 0.663 ms | 0.670 / 1.145 / 1.903 ms |
| Map 70, 260 props | 0.109 / 0.168 ms | 1.172 / 1.502 / 1.819 ms |
| Map 73, 379 props | 0.094 / 0.138 ms | 0.533 / 0.656 / 0.812 ms |

The 150-resting-prop update is below 0.2 ms, and the 40-prop shove p95 is below 1.5 ms. No measured update exceeded 8.3 ms. The shove is an initial impulse scene, not a claim that all 40 objects remain awake throughout its settling period.

Isolated render comparison uses ABBA order, 120 warm-up and 300 measured frames per scene. Empty render means: 9.231 and 8.975 ms. With 150 resting props: 9.178 and 9.175 ms. Pooled mean incremental rendering time: **0.074 ms**, below the 0.5 ms target on this machine. These are CPU wall timings including renderer synchronization, not a general hardware guarantee. Raw per-frame CSVs and reports: performance/updates.* and performance/paired.*.

## Render review

- Fresh campaign captures from the inspected executable (code/data identical to the final clean rebuild): renders/sheet-00.jpg through sheet-16.jpg, with all 68 native 960x540 frames retained as PPM/PNG. Includes desks and glass in Chapter 1 maps 47, 52, 53, 64 and Chapter 2 maps 68-73, before and two seconds after a shotgun shot. renders/map-shots.txt records 20 successful shotgun shots, each loaded 6 -> 5. Fixed observer frames deliberately omit the player HUD. Protected panes crack and remain solid; ordinary panes break. Some desk angles aim through the back of equipment, so these images supplement collision/damage regressions rather than independently proving every desktop hit.

- Final facing review covers induction theatre rows toward the screen, waiting rows into the hall, boardroom chairs toward the table, terminal seating, ward headboards and both staff rooms. Three hold-room views retain their authored restraint boards. Supplemental inspection-final/sheet.jpg supplies the cart tray/pill view and clear north/east staff-room views; its cart-side frame and the original north staff/cart angles are obstructed camera positions and are not used as evidence of support correctness. The moving-cart support/fall behavior is verified by the regression above.

- Engine D1-D3 matched before/after evidence is preserved from the earlier implementation run at N:/rawmetal/.build/codex-phys/.build/job2/engine-comparison.png, with original PPMs in engine-before-complete and engine-after-complete. It covers plant-room route bars, recessed fitting shadows in 71/72, and glass at the booth/nurses/hatch. Those rendering code paths were retained through recovery. Current Vulkan tests measure 0.773372 linear-light glass transmission and pass cache/material checks.

- Earlier fixture-part and stage inspection evidence remains at N:/rawmetal/.build/codex-phys/.build/job2/physics-inspection-sheet.png and fixture-part-comparison.png.

## Glazing site table

All paths below are in src/world. Rows describe authored creation/run sites, including repeated calls on one source line; helpers and loops may generate multiple physical pane bays. Final source lists 111 campaign creation/run sites and 14 helper defaults.

| File:line | Room/run | Policy | Reason |
|---|---|---|---|
| `CampaignMaps.cpp:116` | makeFoundry / 01 INTAKE | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:268` | makePressureWorks / 06 CONTROL | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:394` | makeTurbineGantry / 08 UPPER GANTRY | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:395` | makeTurbineGantry / 08 UPPER GANTRY | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:395` | makeTurbineGantry / 08 UPPER GANTRY | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:396` | makeTurbineGantry / 08 UPPER GANTRY | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:396` | makeTurbineGantry / 08 UPPER GANTRY | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:539` | makeReactorComplex / 11 CONTAINMENT (-9 m) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:539` | makeReactorComplex / 11 CONTAINMENT (-9 m) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:752` | makeReactorCore / 12A R-03 CONTROL (-6 m) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:753` | makeReactorCore / 12A R-03 CONTROL (-6 m) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:753` | makeReactorCore / 12A R-03 CONTROL (-6 m) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1212` | makePumpAnnex / Observation gallery | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1213` | makePumpAnnex / Observation gallery | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1213` | makePumpAnnex / Observation gallery | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1285` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1286` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1286` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1287` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1287` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1384` | makeWasteHandling / Control pulpit over the press | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1385` | makeWasteHandling / Control pulpit over the press | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1385` | makeWasteHandling / Control pulpit over the press | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1483` | makeFreightAccess / Personnel wing (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1484` | makeFreightAccess / Personnel wing (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1484` | makeFreightAccess / Personnel wing (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1484` | makeFreightAccess / Personnel wing (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1512` | makeFreightAccess / Freight hall | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1512` | makeFreightAccess / Freight hall | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1742` | makeReceivingOverlook / Mezzanine, booth and balcony | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1744` | makeReceivingOverlook / Mezzanine, booth and balcony | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1744` | makeReceivingOverlook / Mezzanine, booth and balcony | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1910` | makeWarehouseIntake / Intake office, west: glazed onto the floor | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1912` | makeWarehouseIntake / Intake office, west: glazed onto the floor | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1990` | makeWarehouseAB / Operations office, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1991` | makeWarehouseAB / Operations office, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1992` | makeWarehouseAB / Operations office, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2253` | makeManifestOffices / The overlook gallery and its window wall | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2285` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2286` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2286` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2288` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2288` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2348` | makeManifestRouting / Briefing room, centre | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2348` | makeManifestRouting / Briefing room, centre | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2349` | makeManifestRouting / Briefing room, centre | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2349` | makeManifestRouting / Briefing room, centre | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2394` | makeManifestRouting / Routing control room | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2394` | makeManifestRouting / Routing control room | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2395` | makeManifestRouting / Routing control room | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2395` | makeManifestRouting / Routing control room | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMachineHall.cpp:696` | makeMachineHallNorthGallery / makeMachineHallNorthGallery | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMaintenance.cpp:273` | makeMaintenanceDistrict / NE: break room, where the survivors camped | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMaintenance.cpp:306` | makeMaintenanceDistrict / SW: time office, the shift board and the PA | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMaintenance.cpp:433` | makeCentralStores / Storeman's office, north-east: glazed onto the floor, the hatch onto the passage | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMaintenance.cpp:657` | makeFabricationShop / The machine line, west | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMedicalFront.cpp:130` | makeMedicalScreening / Plan: walls (faces line up with the 9-14 seams) | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
| `CampaignChapterMedicalFront.cpp:158` | makeMedicalScreening / Plan: walls (faces line up with the 9-14 seams) | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
| `CampaignChapterMedicalFront.cpp:160` | makeMedicalScreening / Plan: walls (faces line up with the 9-14 seams) | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
| `CampaignChapterMedicalFront.cpp:168` | makeMedicalScreening / Plan: walls (faces line up with the 9-14 seams) | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. State/trigger: medf_hold_wake. |
| `CampaignChapterMedicalFront.cpp:171` | makeMedicalScreening / Plan: walls (faces line up with the 9-14 seams) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMedicalFront.cpp:171` | makeMedicalScreening / Plan: walls (faces line up with the 9-14 seams) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMedicalFront.cpp:598` | makeMedicalTriage / Plan (walls .2 m, painted; faces at y 9.0 and 14.0 carry 71's corridor through the seam) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMedicalFront.cpp:600` | makeMedicalTriage / Plan (walls .2 m, painted; faces at y 9.0 and 14.0 carry 71's corridor through the seam) | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
| `CampaignChapterMedicalFront.cpp:618` | makeMedicalTriage / Plan (walls .2 m, painted; faces at y 9.0 and 14.0 carry 71's corridor through the seam) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMedicalFront.cpp:840` | makeMedicalTriage / Overflow day-room: when the bays ran out | **breakable** | Existing fixture glass/screen; frame and casing retained. |
| `CampaignChapterMedicalFront.cpp:1034` | makeMedicalImaging / Plan (walls .2 m, painted). The corridor keeps the 4 m of the N door and both seams | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMedicalFront.cpp:1037` | makeMedicalImaging / Plan (walls .2 m, painted). The corridor keeps the 4 m of the N door and both seams | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMedicalFront.cpp:1043` | makeMedicalImaging / Plan (walls .2 m, painted). The corridor keeps the 4 m of the N door and both seams | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
| `CampaignChapterMedicalFront.cpp:1148` | makeMedicalImaging / X-ray 2: fluoroscopy, the generators for both rooms, a trolley still waiting | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:268` | makeOperationsCorridor / The line: partition, turnstiles and the guard booth | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:268` | makeOperationsCorridor / The line: partition, turnstiles and the guard booth | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:290` | makeOperationsCorridor / Clean side: the corridor walls | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:294` | makeOperationsCorridor / Clean side: the corridor walls | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:470` | makeSecurityPerimeterYard / Check 1: the arrivals booth; its intercom is wired to the Lock 2 bell | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:471` | makeSecurityPerimeterYard / Check 1: the arrivals booth; its intercom is wired to the Lock 2 bell | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:612` | makeSecurityPerimeterGatehouse / The gatehouse: walls, window band, door and roof | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:700` | makeCheckpointDelta / The security office mezzanine (west) and its stair | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:772` | makeAdministrativeLink / The office floor, hung over the shaft | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterA.cpp:789` | makeAdministrativeLink / The security chief's office (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:225` | makeRestrictedUtilities / Cascade control, on the deck east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:225` | makeRestrictedUtilities / Cascade control, on the deck east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:226` | makeRestrictedUtilities / Cascade control, on the deck east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:226` | makeRestrictedUtilities / Cascade control, on the deck east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:422` | makeResearchReceiving / Receiving office and personnel sort, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:422` | makeResearchReceiving / Receiving office and personnel sort, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:440` | makeResearchReceiving / Receiving office and personnel sort, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:609` | makeDecontamination / Stage 1: disrobe | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:630` | makeDecontamination / The chamber: air shower and exposure monitor | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:632` | makeDecontamination / The chamber: air shower and exposure monitor | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPerimeterB.cpp:632` | makeDecontamination / The chamber: air shower and exposure monitor | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterPower.cpp:146` | makePowerDistribution / PPE and permit room, north-west | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterResearchFront.cpp:166` | makeResearchIntake / R1 The interlock vestibule (x 8.8-15.2, y 1-3.15) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterResearchFront.cpp:170` | makeResearchIntake / R1 The interlock vestibule (x 8.8-15.2, y 1-3.15) | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
| `CampaignChapterResearchFront.cpp:178` | makeResearchIntake / R1 The interlock vestibule (x 8.8-15.2, y 1-3.15) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterResearchFront.cpp:220` | makeResearchIntake / R2 The security post (x 3.6-8.6, y 1-6.4) and R3 the kit room (x 1-3.4) | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
| `CampaignChapterResearchFront.cpp:334` | makeResearchIntake / R9 The intake concourse (x 6.8-17.2, y 7.4-17.6) and R10 the exit lobby | **breakable** | Existing fixture glass/screen; frame and casing retained. State/trigger: rf_intake_alert. |
| `CampaignChapterResearchFront.cpp:578` | makePersonnelProcessing / Plan | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
| `CampaignChapterResearchFront.cpp:857` | makeResearchAdministration / Plan | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterResearchFront.cpp:857` | makeResearchAdministration / Plan | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterResearchFront.cpp:866` | makeResearchAdministration / Plan | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterResearchFront.cpp:881` | makeResearchAdministration / Plan | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:227` | makeWaterTreatment / Control room (east end of the gallery) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:229` | makeWaterTreatment / Control room (east end of the gallery) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:308` | makeWaterTreatment / Water lab, south-west | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:593` | makeSumpWorks / MCC-2 room (south-west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:594` | makeSumpWorks / MCC-2 room (south-west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:594` | makeSumpWorks / MCC-2 room (south-west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:988` | makeHeatExchange / South: plate exchangers, loop pumps, the control room, the way on | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:989` | makeHeatExchange / South: plate exchangers, loop pumps, the control room, the way on | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterWater.cpp:989` | makeHeatExchange / South: plate exchangers, loop pumps, the control room, the way on | **holds** | Protective/sequence barrier; preserves authored access. |

111 campaign run/creation call sites listed.

| Builder | Policy |
|---|---|
| `m71Glaze` | breakable by default; explicit run arguments override where provided. |
| `m72GlazeY` | breakable by default; explicit run arguments override where provided. |
| `mdGlass` | breakable by default; explicit run arguments override where provided. |
| `mhGlazed` | holds by default; explicit run arguments override where provided. |
| `mntGlazed` | holds by default; explicit run arguments override where provided. |
| `pbGlazed` | holds by default; explicit run arguments override where provided. |
| `pbLabGlass` | holds by default; explicit run arguments override where provided. |
| `perCurtain` | holds by default; explicit run arguments override where provided. |
| `perGlass` | holds by default; explicit run arguments override where provided. |
| `perWindow` | holds by default; explicit run arguments override where provided. |
| `rfBooth` | breakable by default; explicit run arguments override where provided. |
| `rfGlass` | breakable by default; explicit run arguments override where provided. |
| `rfiGlass` | breakable by default; explicit run arguments override where provided. |
| `watGlazed` | holds by default; explicit run arguments override where provided. |

## Local delivery

The release is integrated locally in engine-pass and main after checks, with user promotional changes preserved. RawMetal.exe is the verified v1.1.5 executable; the previous root executable is backed up under recovery evidence. The local release folder contains DepthWorks.exe, DepthWorks-v1.1.5.zip, notes, a Windows guide and SHA-256 checksums. No remote push or release publication is part of this completion.
