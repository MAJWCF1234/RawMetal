# Physics job 2 — implementation and verification report

**Status: B, C and D are committed and verified. A remains uncommitted and is not complete. E has passing functional/graphics tests and completed render artifacts, but the required all-PASS placement audit condition is not met.**

The exact-position conversions expose authored overlaps and unsupported items. The brief requires the literal creation position/size/yaw to stay unchanged and also requires all audits to pass. A choice between small placement corrections and retaining unsafe items as fixtures is pending. No permission to move those items has been inferred.

## Branch and commits

Branch: `codex/physics-stage1`. Fast-forwarded `engine-pass` to `a977305`, preserving and restoring the 22 interrupted files. No push.

```
aaaeb5a Heal protected glazing after twenty seconds and preserve its timer
5f81050 Extend prop contacts and destruction to blasts, actors and custom maps
95d4c99 Keep resting props cheap and retain kick ownership through contacts
e54907f Fix authored route paint, lamp self shadows, glazing and Chapter 2 seals
6d28572 Support terrain grounded breakables in Ashfall and custom maps
d21e8fc Clip broken fixture parts to their authored bounds and retain their casings
```

## Implemented behavior

- **B:** Holds panes retain bullet stopping, crack stages and new-hit timer reset. They begin fading at 20 seconds and recover by 21 seconds; their crack decals fade and are erased. Ordinary breakable glass never heals. Save version 18 stores the age and reads older saves.
- **C:** Blast LOS/impulses and pane damage, grenade barrel/crate chaining, attributed guard/creature kicks, thrown-guard damage and squad-truce consequences, hearing-only glass muffling, heavy-prop blocking with a half-second escape rule, furniture profiles, door sweeps, cargo carry, paper drag, adaptive substeps and tracked-hand pushes. Sleeping-contact work and sound ownership through secondary contacts were corrected and profiled.
- **C:** Plywood/cubicle and ceiling breakables, material sounds, cabinet helper, fixture-part masks and bounded face clipping that preserves native casings/UVs, separate fixture-part cache, model CLUTTER and BREAKABLE parser/editor support, round-trip checks and terrain grounding for Ashfall. Campaign maps gain no new partitions, ceiling tiles or cabinets.
- **D:** Campaign route bars are suppressed for authored chunks. Recessed emitters are moved below their fitting and each light excludes its own housing from its shadow map. WorldLight and kit light helpers support shadows=false; maps do not use that option. All door-mirroring paths honor the destination lock, including streaming. Triage watches patient actor 1 and then looks toward nurse actor 0 from the rebuilt positions.
- **D:** Glossy transparent glass had been misclassified as liquid and blended at a fixed 84 percent. Liquids now have an explicit material flag; glass keeps its highlights and texture alpha. Measured two-surface transmission is **0.773372 in linear light** before the reference display curve (display bytes: direct 227, through 218).
- **A, pending:** Literal fixture calls become props, workstation contents retain their original placements and variants, existing clutter lists are appended rather than replacing earlier props, glazing runs split into individual bays while keeping their frames, and author-specific protection/trigger/pose choices are applied. Existing desk lights turn off when their bound prop is displaced; specified radio cue sources remain anchored. The Intake case trips rf_intake_alert, and the hold window uses medf_hold_wake.

## File-by-file changes

| File | Change |
|---|---|
| `src/assets.rc` | Resources 2257–2259 reference the existing wood break, wood impact and plaster break sounds. |
| `src/audio/Sound.h` | Append sound IDs; sound-event NPC owner. Existing arithmetic resource mapping already covers the appended resources. |
| `src/game/CampaignMapTest.cpp` | Chapter 2 seal regression and rebuilt nurse/patient Triage beat. |
| `src/game/Clutter.cpp` | Healing, blasts, actor/VR contacts, thrown hits, door sweeps, drag, substeps, attributed secondary impacts, sleeping fast path, destruction and inspection/regression scenes. Pending A includes settling tolerance and workstation-preservation checks. |
| `src/game/Game.cpp` | Age breakables each update and route barrel explosions through prop/pane blast logic. |
| `src/game/Game.h` | Physics entry points, transient contact state, owner fields and stage tests. |
| `src/game/MapAudit.cpp` | Pending A: furniture-profile-aware overlap checks, native-model proportion policy, side-supported hatch frames, movable-prop route treatment and explicit glass-only/locked-state reachability reports. Remaining failures are retained. |
| `src/game/Mechanisms.cpp` | Cargo-platform prop carry; pending A binds lamp power to the authored prop identity. |
| `src/game/Movement.cpp` | Heavy-prop approach collision, safe retreat and push-attempt tracking. |
| `src/game/Saves.cpp` | Version 18 healing-age archive; pending A extends model-prop dimensions for requested rolling furniture. |
| `src/game/Streaming.cpp` | Destination lock checks on every boundary-door mirror path. |
| `src/game/ai/Hearing.cpp` | New impact sounds, event-owner stimuli and hearing-only intact-glass occlusion. |
| `src/game/ai/enemy/EnemyAI.cpp` | Acoustic consumers use hearingRayClear; sight keeps its existing path. |
| `src/game/ai/grenades/TacticalGrenades.cpp` | Grenade blasts reach model props, glazing, barrels and crates. |
| `src/game/ai/guard/Mind.cpp` | Ignore own-squad attributed prop impacts; glass muffles hearing only. |
| `src/main.cpp` | physics-stages and glazing test registration; fixture inspection captures. |
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
| `src/world/CampaignChapterMachineHall.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CampaignChapterMaintenance.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CampaignChapterMedicalFront.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CampaignChapterPerimeterA.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CampaignChapterPerimeterB.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CampaignChapterPower.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CampaignChapterResearchFront.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CampaignChapterWater.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CampaignKit.h` | Destruction helpers, fixture parts, shadow option; pending A preserves exact workstation sets, loose paper, posed conversions and per-bay glazing. |
| `src/world/CampaignMaps.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/CustomCampaign.cpp` | Pending A: literal portable-item and glazing conversions; original creation positions retained, with author protection/trigger choices and existing legacy clutter preserved. |
| `src/world/ModelIds.h` | Regenerated from the tracked physics rows, not hand-edited. |
| `src/world/World.cpp` | Furniture support/clearance profiles and validation, lamp source placement, terrain-grounded breakable data; pending A includes prop dimensions, wheelchair seat and lamp bindings. |
| `src/world/World.h` | Age storage, fixture-part queries and profile APIs; shadow option; pending A lamp/prop authoring identity. |
| `src/world/WorldDefinition.h` | Wood/plastic impact and break voices. |
| `tools/campaign/prop_physics.json` | Pending A curated portable/furniture rows and nominal native-model sizes. |
| `tools/level-editor/README.md` | Physics fields and custom-map record syntax. |
| `tools/level-editor/index.html` | Loose model, breakable material, protective pane and break-state controls. |
| `tools/level-editor/runtime_records.py` | Bounded preservation of CLUTTER and BREAKABLE records. |
| `tools/level-editor/server.py` | Model CLUTTER, BREAKABLE boxes and smart-window glazing export. |
| `tools/level-editor/test_map_payload.py` | Physical-model and breakable-box export round trip. |
| `tools/level-editor/test_runtime_records.py` | Exact record preservation through project/runtime export. |

## Authoring and deviations

New helpers: `boardedGap(m,x1,y1,x2,y2,lo,hi,broken=0,trigger=0)`, `ceilingTiles(m,x1,y1,x2,y2,z,broken=0,trigger=0)`, `glassCabinet(m,x,y,z,w,d,h,broken=0)`, and `fixturePart(m,fixture,partMask,x1,y1,x2,y2,lo,hi,broken=0,trigger=0)`. Part masks select material groups and their authored spatial region; removing a screen does not require deleting its casing.

The editor format is documented in `tools/level-editor/README.md`:

```text
CLUTTER|map|7|x|y|z|yaw|model|width|depth|height|pitch|roll|breakState
BREAKABLE|map|x1|y1|x2|y2|bottom|top|material|health|holds|brokenState|triggerState[|fixtureIndex|partMask]
```

- The original Stage 1 desktop-size bound is extended to 3 m for the Stage 2 rolling-furniture conversions, with the 60 kg audit limit retained. The gurney row uses 55 kg; it cannot be lifted.
- Initial settling tolerates up to 2.5 cm of floor-finish penetration, matching the existing collision tolerance, so ordinary authored floor-level items can rest on thin carpet/paint finishes. Creation coordinates remain unchanged.
- Native facility models have no canonical catalog proportions. Their prop audit uses the same catalog-only distortion rule as the fixture audit; curated nominal sizes still supply masses and desktop dimensions.
- Two existing seam glazing sites are inset to 0.3 m as A5 requires. No other map positions have been deliberately moved.
- Hanging IV bags stay attached fixtures under A2’s hanging-item exclusion; there is no new hanging-rope constraint. Specified mounted TVs stay fixed; the overflow TV receives a bounded breakable screen.
- Protected story/lead/transfer/interlock panes hold. Triage’s spattered band hatch holds under the author’s final amendment, retaining its spatter and nurse story.
- The reachability audit also closes locked doors for an additional glass-only comparison. Positive Chapter 2 differences correspond to the authors’ explicitly permitted route/scare panes. Routing-wing glass that opened new access is protective.
- Raw material-17 floor slivers are existing glass litter, not glazing; they stay visual litter.

## Glazing site table

This lists campaign creation/run call sites. Parameterized helpers and their policies are listed afterward; their individual bays inherit the run’s policy. Room labels use the authored function/section name.

| File:line | Room / run | Policy | Reason |
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
| `CampaignMaps.cpp:751` | makeReactorCore / 12A R-03 CONTROL (-6 m) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:752` | makeReactorCore / 12A R-03 CONTROL (-6 m) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:752` | makeReactorCore / 12A R-03 CONTROL (-6 m) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1211` | makePumpAnnex / Observation gallery | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1212` | makePumpAnnex / Observation gallery | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1212` | makePumpAnnex / Observation gallery | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1284` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1285` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1285` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1286` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1286` | makeUtilityJunction / Dispatch office | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1383` | makeWasteHandling / Control pulpit over the press | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1384` | makeWasteHandling / Control pulpit over the press | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1384` | makeWasteHandling / Control pulpit over the press | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1482` | makeFreightAccess / Personnel wing (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1483` | makeFreightAccess / Personnel wing (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1483` | makeFreightAccess / Personnel wing (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1483` | makeFreightAccess / Personnel wing (west) | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1511` | makeFreightAccess / Freight hall | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1511` | makeFreightAccess / Freight hall | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1741` | makeReceivingOverlook / Mezzanine, booth and balcony | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1743` | makeReceivingOverlook / Mezzanine, booth and balcony | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1743` | makeReceivingOverlook / Mezzanine, booth and balcony | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1909` | makeWarehouseIntake / Intake office, west: glazed onto the floor | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1911` | makeWarehouseIntake / Intake office, west: glazed onto the floor | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1989` | makeWarehouseAB / Operations office, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1990` | makeWarehouseAB / Operations office, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:1991` | makeWarehouseAB / Operations office, north-east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2252` | makeManifestOffices / The overlook gallery and its window wall | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2284` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2285` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2285` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2287` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2287` | makeManifestOffices / Supervisors' offices, east | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2347` | makeManifestRouting / Briefing room, centre | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2347` | makeManifestRouting / Briefing room, centre | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2348` | makeManifestRouting / Briefing room, centre | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2348` | makeManifestRouting / Briefing room, centre | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2393` | makeManifestRouting / Routing control room | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2393` | makeManifestRouting / Routing control room | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2394` | makeManifestRouting / Routing control room | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignMaps.cpp:2394` | makeManifestRouting / Routing control room | **holds** | Protective/sequence barrier; preserves authored access. |
| `CampaignChapterMachineHall.cpp:695` | makeMachineHallNorthGallery / makeMachineHallNorthGallery | **holds** | Protective/sequence barrier; preserves authored access. |
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
| `CampaignChapterResearchFront.cpp:866` | makeResearchAdministration / Plan | **breakable** | Ordinary glass or the Chapter 2 author’s explicit break/distraction route. |
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

## Tests, audits and profiles

Every executable was copied to a scratch directory before running. Each test directory has `exe-sha256.txt`; audit directories contain their copied EXE and exit log. These snapshots distinguish earlier broad checks from later focused verification.

| Evidence directory | Result lines |
|---|---|
| `.build/job2/baseline` | `--each-test=glass exit 0 elapsed 1.75s`<br>`--each-test=props exit 0 elapsed 1.26s` |
| `.build/job2/stages-final` | `--each-test=physics-stages exit 0 elapsed 0.9s`<br>`--each-test=props exit 0 elapsed 1.39s`<br>`--each-test=glass exit 0 elapsed 1.62s`<br>`--each-test=clutter exit 0 elapsed 2.66s`<br>`--each-test exit 1 elapsed 261.29s` |
| `.build/job2/engine` | `--campaign-map-test exit 0 elapsed 52.3s`<br>`--vulkan-test exit 0 elapsed 186.39s` |
| `.build/job2/guards` | `--guard-ai-test exit 0 elapsed 27.03s`<br>`--guard-combat-test exit 0 elapsed 2.96s`<br>`--guard-pursuit-test exit 0 elapsed 6.58s`<br>`--guard-policy-test exit 0 elapsed 3.53s`<br>`--guard-tactics-test exit 0 elapsed 1.79s`<br>`--stealth-test exit 0 elapsed 3.5s`<br>`--hearing-test exit 0 elapsed 0.6s` |
| `.build/job2/regression` | `--smoke-test exit 0 elapsed 265.89s`<br>`--chapter-route-test exit 0 elapsed 11.76s`<br>`--prologue-test exit 0 elapsed 3.95s`<br>`--exclusion-test exit 0 elapsed 190.49s`<br>`--content-integrity-test exit 0 elapsed 2.18s`<br>`--engine-regression-test exit 0 elapsed 2.64s`<br>`--creature-behaviour-test exit 0 elapsed 8.67s` |
| `.build/job2/glazing-linear` | `--glazing-test exit 0 elapsed 13.3s` |
| `.build/job2/physics-final` | `--each-test=physics-stages exit 1 elapsed 0.99s`<br>`--each-test=props exit 0 elapsed 1.57s`<br>`--each-test=glass exit 0 elapsed 2.15s`<br>`--perf-profile=N:/rawmetal/.build/codex-phys/.build/job2-props-perf.txt --perf-norender --perf-leaf exit 0 elapsed 2.1s` |
| `.build/job2/owner-final` | `--each-test=physics-stages exit 0 elapsed 0.98s` |
| `.build/job2/current-final` | `--each-test=physics-stages exit 0 elapsed 0.85s`<br>`--each-test=props exit 0 elapsed 1.53s`<br>`--each-test=glass exit 0 elapsed 2.06s`<br>`--each-test exit 0 elapsed 278.14s` |
| `.build/job2/review-final` | `--each-test exit 0 elapsed 257.43s`<br>`--campaign-map-test exit 0 elapsed 49.14s` |
| `.build/job2/vulkan-final` | `--vulkan-test exit 0 elapsed 151.61s` |
| `.build/job2/fixture-vulkan-final` | `--vulkan-test exit 0 elapsed 133.84s` |
| `.build/job2/physics-renders` | `--physics-inspection --vulkan exit 0 elapsed 13.49s` |
| `.build/job2/physics-renders-final` | `--physics-inspection exit 0 elapsed 9.93s`<br>`--vulkan exit -1 elapsed 35.56s` |
| `.build/job2/engine-before-complete` | `--map-shots=N:/rawmetal/.build/codex-phys/.build/job2-dviews.txt --clean-shots --nopost exit 0 elapsed 20.06s` |
| `.build/job2/engine-after-complete` | `--map-shots=N:/rawmetal/.build/codex-phys/.build/job2-dviews.txt --clean-shots --nopost exit 0 elapsed 20.29s` |
| `.build/job2/campaign-renders` | `--map-shots=N:/rawmetal/.build/codex-phys/.build/job2-contact-views.txt --clean-shots exit 0 elapsed 36.31s` |
| `.build/job2/campaign-renders-final` | `--map-shots=N:/rawmetal/.build/codex-phys/.build/job2-contact-views.txt --clean-shots exit 0 elapsed 34.81s` |

The table preserves intermediate failures as history. Later `current-final` and `review-final` runs supersede the earlier full-suite failure; `owner-final` and the later physics-stages runs supersede the intermediate contact-ownership failure. The standalone `--vulkan` entry in `physics-renders-final` was an accidental normal-game launch after the inspection completed and was closed deliberately; it was not the Vulkan test. The actual `--vulkan-test` passed in `fixture-vulkan-final`.

Representative result lines:

```text
current-final: ALL PASSED 0
review-final: ALL PASSED 0
current-final: PASS holds glass stops every bullet
current-final: PASS holds glass stops every bullet
current-final: PASS holds glass stops every bullet
current-final: PASS holds glass accumulates three crack stages and never breaks
current-final: PASS holds pane stops even a muzzle crossing its box
current-final: PASS holds stays at stage 2 at 19 seconds
current-final: PASS holds heal timer survives byte-exact save
current-final: PASS holds cracks fade over one second
current-final: PASS new hit resets healing clock
current-final: PASS holds intact at 21 seconds with no pane decals
current-final: PASS breakable glass never heals
current-final: ALL PASSED glass
current-final: PASS Ashfall terrain grounds breakable records and collision boxes together
current-final: PASS terrain-grounded glazing breaks through the normal world path
current-final: ALL PASSED physics-stages
fixture-vulkan-final: Glazing linear-light transmission 0.773372 direct 227 through 218
fixture-vulkan-final: Glossy glazing preserves 70-80 percent of the room brightness: PASS
fixture-vulkan-final: Breaking a fixture screen updates its bounded part cache without rebuilding the chunk: PASS
fixture-vulkan-final: PASS: Vulkan material / depth / clipping checks
```

**Editor:** `.build/job2-editor-tests.txt`: 7 unittest checks, OK. `tools/level-editor/test_map_payload.py`: `level-editor map payload export: PASS`, including the new physics export round trip.

**Edges:** `.build/job2/edge-check.txt`: `edges ok` for 34–74.

**Prop update profile:** `.build/job2/physics-final/perf-profile.txt` (leaf timers enabled):

```text
== resting150: level 0 at (3.5, 4.5) feet 0, yaw 0, pitch -15; 180 measured frames after 120 warm-up; no wake; spawned props 150/150
   clutter                      0.051     0.041     0.079     0.091     0.120 ms
== awake40: level 0 at (3.5, 4.5) feet 0, yaw 0, pitch -15; 180 measured frames after 60 warm-up; no wake; spawned props 40/40
   clutter                      0.165     0.109     0.498     0.677     0.683 ms
```

150 resting props: mean 0.051 ms / p95 0.079 ms, under the 0.2 ms update budget. The 40-prop shove scene: p95 0.498 ms, under 1.5 ms. These are update timings, not whole-frame or incremental render-cost claims.

The rendered profile is in `.build/job2/perf-gpu/perf-profile.txt`; it predates the final sleeping-update optimization. It includes the full world render, not an isolated prop-only render delta. The <0.5 ms incremental resting-prop render budget is therefore not established by that measurement. The Vulkan suite separately verifies cache reuse, stable pixels, no main chunk rebuild on pane/fixture-part breaks, and the pane-break CPU scene spike threshold.

### Campaign audit ledger

| Map | Verdict | Glass-only reachability | Locked-state glass reachability |
|---|---|---|---|
| 0 | PASS: 0 placement problems | 0 cells | 0 cells |
| 1 | PASS: 0 placement problems | 0 cells | 0 cells |
| 2 | PASS: 0 placement problems | 0 cells | 0 cells |
| 3 | FAIL: 4 placement problems | 0 cells | 0 cells |
| 4 | PASS: 0 placement problems | no glazing | no glazing |
| 5 | PASS: 0 placement problems | no glazing | no glazing |
| 6 | PASS: 0 placement problems | no glazing | no glazing |
| 7 | PASS: 0 placement problems | 0 cells | 0 cells |
| 8 | PASS: 0 placement problems | 0 cells | 0 cells |
| 9 | PASS: 0 placement problems | 0 cells | 0 cells |
| 10 | PASS: 0 placement problems | 0 cells | 0 cells |
| 11 | PASS: 0 placement problems | no glazing | no glazing |
| 12 | PASS: 0 placement problems | no glazing | no glazing |
| 13 | PASS: 0 placement problems | 0 cells | 0 cells |
| 14 | PASS: 0 placement problems | no glazing | no glazing |
| 15 | FAIL: 1 placement problems | 0 cells | 0 cells |
| 16 | FAIL: 1 placement problems | 0 cells | 0 cells |
| 17 | PASS: 0 placement problems | no glazing | no glazing |
| 18 | PASS: 0 placement problems | no glazing | no glazing |
| 19 | PASS: 0 placement problems | no glazing | no glazing |
| 20 | FAIL: 5 placement problems | 0 cells | 0 cells |
| 21 | FAIL: 7 placement problems | 0 cells | 0 cells |
| 22 | PASS: 0 placement problems | no glazing | no glazing |
| 23 | PASS: 0 placement problems | no glazing | no glazing |
| 24 | PASS: 0 placement problems | no glazing | no glazing |
| 25 | PASS: 0 placement problems | no glazing | no glazing |
| 26 | PASS: 0 placement problems | no glazing | no glazing |
| 27 | PASS: 0 placement problems | no glazing | no glazing |
| 28 | PASS: 0 placement problems | no glazing | no glazing |
| 29 | PASS: 0 placement problems | no glazing | no glazing |
| 30 | PASS: 0 placement problems | no glazing | no glazing |
| 31 | PASS: 0 placement problems | no glazing | no glazing |
| 32 | PASS: 0 placement problems | 0 cells | 0 cells |
| 33 | PASS: 0 placement problems | no glazing | no glazing |
| 34 | PASS: 0 placement problems | 0 cells | 0 cells |
| 35 | PASS: 0 placement problems | no glazing | no glazing |
| 36 | PASS: 0 placement problems | no glazing | no glazing |
| 37 | PASS: 0 placement problems | no glazing | no glazing |
| 38 | PASS: 0 placement problems | no glazing | no glazing |
| 39 | PASS: 0 placement problems | no glazing | no glazing |
| 40 | PASS: 0 placement problems | no glazing | no glazing |
| 41 | PASS: 0 placement problems | 0 cells | 0 cells |
| 42 | FAIL: 3 placement problems | 0 cells | 0 cells |
| 43 | PASS: 0 placement problems | no glazing | no glazing |
| 44 | PASS: 0 placement problems | no glazing | no glazing |
| 45 | PASS: 0 placement problems | no glazing | no glazing |
| 46 | PASS: 0 placement problems | no glazing | no glazing |
| 47 | FAIL: 5 placement problems | 0 cells | 0 cells |
| 48 | PASS: 0 placement problems | no glazing | no glazing |
| 49 | PASS: 0 placement problems | 0 cells | 0 cells |
| 50 | PASS: 0 placement problems | no glazing | no glazing |
| 51 | PASS: 0 placement problems | no glazing | no glazing |
| 52 | PASS: 0 placement problems | 0 cells | 0 cells |
| 53 | FAIL: 25 placement problems | 0 cells | 0 cells |
| 54 | FAIL: 8 placement problems | 0 cells | 0 cells |
| 55 | PASS: 0 placement problems | 0 cells | 0 cells |
| 56 | PASS: 0 placement problems | no glazing | no glazing |
| 57 | FAIL: 9 placement problems | 0 cells | 0 cells |
| 58 | FAIL: 1 placement problems | 0 cells | 0 cells |
| 59 | PASS: 0 placement problems | 0 cells | 0 cells |
| 60 | PASS: 0 placement problems | 0 cells | 0 cells |
| 61 | FAIL: 1 placement problems | 0 cells | 0 cells |
| 62 | PASS: 0 placement problems | no glazing | no glazing |
| 63 | FAIL: 1 placement problems | 0 cells | 0 cells |
| 64 | PASS: 0 placement problems | 0 cells | 0 cells |
| 65 | PASS: 0 placement problems | 0 cells | 0 cells |
| 66 | PASS: 0 placement problems | no glazing | no glazing |
| 67 | PASS: 0 placement problems | no glazing | no glazing |
| 68 | FAIL: 32 placement problems | 30 cells | 30 cells |
| 69 | FAIL: 18 placement problems | 27 cells | 27 cells |
| 70 | FAIL: 181 placement problems | 16 cells | 16 cells |
| 71 | FAIL: 69 placement problems | 0 cells | 0 cells |
| 72 | FAIL: 60 placement problems | 2 cells | 2 cells |
| 73 | FAIL: 37 placement problems | 0 cells | 0 cells |
| 74 | PASS: 0 placement problems | no glazing | no glazing |
| 179 | PASS: 0 placement problems | no glazing | no glazing |

Evidence: `.build/job2/review-final/audits`. 19 maps currently fail placement validation; the baseline audits of all 0–74 and 179 passed before conversion.

### Outstanding placement failures

These are not suppressed or relabeled as passes. The table below gives every current failing diagnostic; the corresponding raw audit file retains its reach map and border data. A separate exception or coordinate-correction decision is required before A can be completed and committed.

#### Map 3: FAIL: 4 placement problems

```text
FRAMELESS breakable[0]
FRAMELESS breakable[1]
FRAMELESS breakable[2]
FRAMELESS breakable[3]
```

#### Map 15: FAIL: 1 placement problems

```text
HANGING prop model 34 at 2.30,8.35
```

#### Map 16: FAIL: 1 placement problems

```text
HANGING prop model 21 at 21.90,10.90
```

#### Map 20: FAIL: 5 placement problems

```text
HANGING prop model 28 at 3.20,1.72
HANGING prop model 28 at 6.00,1.72
HANGING prop model 28 at 8.80,1.72
FRAMELESS breakable[0]
```

#### Map 21: FAIL: 7 placement problems

```text
FLOATING prop model 38 at 4.30,5.30
HANGING prop model 38 at 4.30,5.30
FLOATING prop model 38 at 4.30,7.50
HANGING prop model 38 at 4.30,7.50
FLOATING prop model 38 at 4.30,9.70
HANGING prop model 38 at 4.30,9.70
```

#### Map 42: FAIL: 3 placement problems

```text
FLOATING prop model 33 at 15.25,6.20
FLOATING prop model 34 at 13.70,6.05
FLOATING prop model 38 at 13.40,6.35
```

#### Map 47: FAIL: 5 placement problems

```text
FLOATING prop model 38 at 14.80,5.45
FLOATING prop model 38 at 15.50,5.45
FLOATING prop model 230 at 1.30,21.10
FLOATING prop model 230 at 1.30,21.60
FLOATING prop model 230 at 1.30,22.10
```

#### Map 53: FAIL: 25 placement problems

```text
HANGING prop model 251 at 4.30,1.50
HANGING prop model 251 at 7.60,1.50
BURIED prop model 38 at 1.22,20.10
OVERLAP prop model 38 at 1.22,20.10 with solid fixture
OVERLAP prop model 38 at 1.22,20.10
BURIED prop model 38 at 1.22,20.28
OVERLAP prop model 38 at 1.22,20.28 with solid fixture
OVERLAP prop model 38 at 1.22,20.28
BURIED prop model 38 at 1.22,20.46
OVERLAP prop model 38 at 1.22,20.46 with solid fixture
OVERLAP prop model 38 at 1.22,20.46
BURIED prop model 38 at 1.22,20.64
OVERLAP prop model 38 at 1.22,20.64 with solid fixture
BURIED prop model 38 at 1.22,20.10
OVERLAP prop model 38 at 1.22,20.10 with solid fixture
OVERLAP prop model 38 at 1.22,20.10
BURIED prop model 38 at 1.22,20.28
OVERLAP prop model 38 at 1.22,20.28 with solid fixture
OVERLAP prop model 38 at 1.22,20.28
BURIED prop model 38 at 1.22,20.46
OVERLAP prop model 38 at 1.22,20.46 with solid fixture
OVERLAP prop model 38 at 1.22,20.46
BURIED prop model 38 at 1.22,20.64
OVERLAP prop model 38 at 1.22,20.64 with solid fixture
```

#### Map 54: FAIL: 8 placement problems

```text
BURIED prop model 251 at 20.90,17.05
HANGING prop model 251 at 22.60,20.70
FLOATING prop model 21 at 12.10,5.75
BURIED prop model 251 at 4.70,21.80
HANGING prop model 251 at 4.70,21.80
BURIED prop model 251 at 6.00,22.00
HANGING prop model 251 at 6.00,22.00
```

#### Map 57: FAIL: 9 placement problems

```text
BURIED prop model 21 at 15.60,1.35
OVERLAP prop model 21 at 15.60,1.35 with solid fixture
BURIED prop model 21 at 16.40,1.35
OVERLAP prop model 21 at 16.40,1.35 with solid fixture
BURIED prop model 21 at 17.70,1.35
OVERLAP prop model 21 at 17.70,1.35 with solid fixture
BURIED prop model 21 at 18.50,1.35
OVERLAP prop model 21 at 18.50,1.35 with solid fixture
```

#### Map 58: FAIL: 1 placement problems

```text
HANGING prop model 21 at 2.30,21.95
```

#### Map 61: FAIL: 1 placement problems

```text
FRAMELESS breakable[0]
```

#### Map 63: FAIL: 1 placement problems

```text
HANGING prop model 38 at 22.60,7.55
```

#### Map 68: FAIL: 32 placement problems

```text
HANGING prop model 30 at 4.95,1.22
HANGING prop model 30 at 5.50,1.22
HANGING prop model 30 at 6.05,1.22
OVERLAP prop model 33 at 8.35,2.75
HANGING prop model 339 at 8.04,2.72
OVERLAP prop model 347 at 3.60,10.15
OVERLAP prop model 347 at 3.60,10.15
OVERLAP prop model 346 at 4.00,12.60
OVERLAP prop model 346 at 4.80,12.60
HANGING prop model 346 at 5.60,12.60
BURIED prop model 346 at 6.35,14.20
HANGING prop model 346 at 6.35,14.20
BURIED prop model 347 at 6.35,14.40
HANGING prop model 347 at 6.35,14.40
BURIED prop model 346 at 6.35,16.50
HANGING prop model 346 at 6.35,16.50
BURIED prop model 347 at 6.35,16.70
HANGING prop model 347 at 6.35,16.70
HANGING prop model 350 at 4.20,15.40
OVERLAP prop model 350 at 4.20,15.40
HANGING prop model 339 at 4.25,15.80
BURIED prop model 59 at 14.75,12.00
HANGING prop model 203 at 8.40,17.55
HANGING prop model 203 at 9.15,17.55
HANGING prop model 203 at 14.85,17.55
OVERLAP prop model 203 at 14.85,17.55
HANGING prop model 203 at 15.60,17.55
HANGING prop model 203 at 16.35,17.55
BURIED prop model 338 at 9.36,15.63
OVERLAP prop model 347 at 7.30,13.70
BURIED prop model 203 at 22.60,19.70
```

#### Map 69: FAIL: 18 placement problems

```text
OVERLAP prop model 347 at 13.70,6.45
HANGING prop model 346 at 13.72,6.95
BURIED prop model 347 at 8.12,8.45
BURIED prop model 338 at 3.53,6.31
HANGING prop model 338 at 3.53,6.31
BURIED prop model 338 at 4.45,6.26
HANGING prop model 338 at 4.45,6.26
BURIED prop model 338 at 5.44,6.44
HANGING prop model 338 at 5.44,6.44
HANGING prop model 346 at 15.87,8.20
HANGING prop model 346 at 17.60,1.27
HANGING prop model 348 at 16.90,21.75
BURIED prop model 87 at 1.35,15.00
OVERLAP prop model 87 at 1.35,15.00
OVERLAP prop model 87 at 1.35,15.65
BURIED prop model 87 at 1.35,16.30
HANGING prop model 346 at 4.85,18.88
```

#### Map 70: FAIL: 181 placement problems

```text
OVERLAP prop model 346 at 12.55,14.70
HANGING prop model 346 at 12.55,14.75
HANGING prop model 346 at 13.38,14.80
HANGING prop model 350 at 18.70,4.00
HANGING prop model 339 at 14.40,20.15
HANGING prop model 339 at 4.70,4.42
BURIED prop model 340 at 1.30,3.10
OVERLAP prop model 340 at 1.30,3.10 with solid fixture
BURIED prop model 340 at 1.31,3.20
OVERLAP prop model 340 at 1.31,3.20 with solid fixture
BURIED prop model 340 at 1.32,3.30
OVERLAP prop model 340 at 1.32,3.30 with solid fixture
BURIED prop model 340 at 1.30,3.40
OVERLAP prop model 340 at 1.30,3.40 with solid fixture
BURIED prop model 340 at 1.31,3.50
OVERLAP prop model 340 at 1.31,3.50 with solid fixture
BURIED prop model 340 at 1.32,3.60
OVERLAP prop model 340 at 1.32,3.60 with solid fixture
BURIED prop model 340 at 1.32,3.90
OVERLAP prop model 340 at 1.32,3.90 with solid fixture
BURIED prop model 340 at 1.30,4.00
OVERLAP prop model 340 at 1.30,4.00 with solid fixture
BURIED prop model 340 at 1.31,4.10
OVERLAP prop model 340 at 1.31,4.10 with solid fixture
BURIED prop model 340 at 1.32,4.20
OVERLAP prop model 340 at 1.32,4.20 with solid fixture
BURIED prop model 340 at 1.30,4.30
OVERLAP prop model 340 at 1.30,4.30 with solid fixture
BURIED prop model 340 at 1.31,4.40
OVERLAP prop model 340 at 1.31,4.40 with solid fixture
BURIED prop model 340 at 1.32,4.50
OVERLAP prop model 340 at 1.32,4.50 with solid fixture
BURIED prop model 340 at 1.31,4.70
OVERLAP prop model 340 at 1.31,4.70 with solid fixture
BURIED prop model 340 at 1.32,4.80
OVERLAP prop model 340 at 1.32,4.80 with solid fixture
BURIED prop model 340 at 1.30,4.90
OVERLAP prop model 340 at 1.30,4.90 with solid fixture
BURIED prop model 340 at 1.31,3.10
OVERLAP prop model 340 at 1.31,3.10 with solid fixture
BURIED prop model 340 at 1.32,3.20
OVERLAP prop model 340 at 1.32,3.20 with solid fixture
BURIED prop model 340 at 1.30,3.30
OVERLAP prop model 340 at 1.30,3.30 with solid fixture
BURIED prop model 340 at 1.31,3.40
OVERLAP prop model 340 at 1.31,3.40 with solid fixture
BURIED prop model 340 at 1.32,3.50
OVERLAP prop model 340 at 1.32,3.50 with solid fixture
BURIED prop model 340 at 1.30,3.60
OVERLAP prop model 340 at 1.30,3.60 with solid fixture
BURIED prop model 340 at 1.31,3.70
OVERLAP prop model 340 at 1.31,3.70 with solid fixture
BURIED prop model 340 at 1.32,3.80
OVERLAP prop model 340 at 1.32,3.80 with solid fixture
BURIED prop model 340 at 1.30,4.20
OVERLAP prop model 340 at 1.30,4.20 with solid fixture
BURIED prop model 340 at 1.31,4.30
OVERLAP prop model 340 at 1.31,4.30 with solid fixture
BURIED prop model 340 at 1.32,4.40
OVERLAP prop model 340 at 1.32,4.40 with solid fixture
BURIED prop model 340 at 1.30,4.50
OVERLAP prop model 340 at 1.30,4.50 with solid fixture
BURIED prop model 340 at 1.31,4.60
OVERLAP prop model 340 at 1.31,4.60 with solid fixture
BURIED prop model 340 at 1.32,4.70
OVERLAP prop model 340 at 1.32,4.70 with solid fixture
BURIED prop model 340 at 1.30,3.20
OVERLAP prop model 340 at 1.30,3.20 with solid fixture
BURIED prop model 340 at 1.31,3.30
OVERLAP prop model 340 at 1.31,3.30 with solid fixture
BURIED prop model 340 at 1.32,3.40
OVERLAP prop model 340 at 1.32,3.40 with solid fixture
BURIED prop model 340 at 1.30,3.50
OVERLAP prop model 340 at 1.30,3.50 with solid fixture
BURIED prop model 340 at 1.31,3.60
OVERLAP prop model 340 at 1.31,3.60 with solid fixture
BURIED prop model 340 at 1.32,3.70
OVERLAP prop model 340 at 1.32,3.70 with solid fixture
BURIED prop model 340 at 1.30,3.80
OVERLAP prop model 340 at 1.30,3.80 with solid fixture
BURIED prop model 340 at 1.31,3.90
OVERLAP prop model 340 at 1.31,3.90 with solid fixture
BURIED prop model 340 at 1.32,4.00
OVERLAP prop model 340 at 1.32,4.00 with solid fixture
BURIED prop model 340 at 1.30,4.10
OVERLAP prop model 340 at 1.30,4.10 with solid fixture
BURIED prop model 340 at 1.30,4.40
OVERLAP prop model 340 at 1.30,4.40 with solid fixture
BURIED prop model 340 at 1.31,4.50
OVERLAP prop model 340 at 1.31,4.50 with solid fixture
BURIED prop model 340 at 1.32,4.60
OVERLAP prop model 340 at 1.32,4.60 with solid fixture
BURIED prop model 340 at 1.30,4.70
OVERLAP prop model 340 at 1.30,4.70 with solid fixture
BURIED prop model 340 at 1.31,4.80
OVERLAP prop model 340 at 1.31,4.80 with solid fixture
BURIED prop model 340 at 1.32,4.90
OVERLAP prop model 340 at 1.32,4.90 with solid fixture
BURIED prop model 340 at 1.30,3.10
OVERLAP prop model 340 at 1.30,3.10 with solid fixture
BURIED prop model 340 at 1.31,3.20
OVERLAP prop model 340 at 1.31,3.20 with solid fixture
BURIED prop model 340 at 1.32,3.30
OVERLAP prop model 340 at 1.32,3.30 with solid fixture
BURIED prop model 340 at 1.30,3.40
OVERLAP prop model 340 at 1.30,3.40 with solid fixture
BURIED prop model 340 at 1.31,3.50
OVERLAP prop model 340 at 1.31,3.50 with solid fixture
BURIED prop model 340 at 1.31,4.10
OVERLAP prop model 340 at 1.31,4.10 with solid fixture
BURIED prop model 340 at 1.32,4.20
OVERLAP prop model 340 at 1.32,4.20 with solid fixture
BURIED prop model 340 at 1.30,4.30
OVERLAP prop model 340 at 1.30,4.30 with solid fixture
BURIED prop model 340 at 1.31,4.40
OVERLAP prop model 340 at 1.31,4.40 with solid fixture
BURIED prop model 340 at 1.32,4.50
OVERLAP prop model 340 at 1.32,4.50 with solid fixture
BURIED prop model 340 at 1.30,4.60
OVERLAP prop model 340 at 1.30,4.60 with solid fixture
BURIED prop model 340 at 1.31,4.70
OVERLAP prop model 340 at 1.31,4.70 with solid fixture
BURIED prop model 340 at 1.32,4.80
OVERLAP prop model 340 at 1.32,4.80 with solid fixture
BURIED prop model 340 at 1.31,4.86
OVERLAP prop model 340 at 1.31,4.86 with solid fixture
HANGING prop model 339 at 9.95,4.95
HANGING prop model 346 at 9.32,6.22
BURIED prop model 346 at 8.75,9.62
BURIED prop model 346 at 7.92,9.60
HANGING prop model 346 at 7.92,9.60
OVERLAP prop model 347 at 10.05,12.95
OVERLAP prop model 33 at 10.15,18.08
OVERLAP prop model 347 at 9.30,16.60
HANGING prop model 346 at 8.42,18.48
BURIED prop model 340 at 1.30,2.30
OVERLAP prop model 340 at 1.30,2.30 with solid fixture
BURIED prop model 340 at 1.31,2.40
OVERLAP prop model 340 at 1.31,2.40 with solid fixture
BURIED prop model 340 at 1.32,2.50
OVERLAP prop model 340 at 1.32,2.50 with solid fixture
BURIED prop model 340 at 1.30,2.60
OVERLAP prop model 340 at 1.30,2.60 with solid fixture
BURIED prop model 340 at 1.31,3.60
OVERLAP prop model 340 at 1.31,3.60 with solid fixture
BURIED prop model 340 at 1.32,3.70
OVERLAP prop model 340 at 1.32,3.70 with solid fixture
BURIED prop model 340 at 1.30,3.80
OVERLAP prop model 340 at 1.30,3.80 with solid fixture
BURIED prop model 340 at 1.31,3.90
OVERLAP prop model 340 at 1.31,3.90 with solid fixture
BURIED prop model 340 at 1.32,4.00
OVERLAP prop model 340 at 1.32,4.00 with solid fixture
BURIED prop model 340 at 1.30,4.00
OVERLAP prop model 340 at 1.30,4.00 with solid fixture
BURIED prop model 340 at 1.31,4.10
OVERLAP prop model 340 at 1.31,4.10 with solid fixture
BURIED prop model 340 at 1.32,2.30
OVERLAP prop model 340 at 1.32,2.30 with solid fixture
BURIED prop model 340 at 1.30,2.40
OVERLAP prop model 340 at 1.30,2.40 with solid fixture
BURIED prop model 340 at 1.31,2.50
OVERLAP prop model 340 at 1.31,2.50 with solid fixture
BURIED prop model 340 at 1.30,3.30
OVERLAP prop model 340 at 1.30,3.30 with solid fixture
BURIED prop model 340 at 1.31,3.40
OVERLAP prop model 340 at 1.31,3.40 with solid fixture
BURIED prop model 340 at 1.32,2.66
OVERLAP prop model 340 at 1.32,2.66 with solid fixture
BURIED prop model 340 at 1.31,2.75
OVERLAP prop model 340 at 1.31,2.75 with solid fixture
OVERLAP prop model 340 at 1.85,2.00
OVERLAP prop model 340 at 1.85,2.00
OVERLAP prop model 340 at 2.05,1.85
OVERLAP prop model 340 at 1.80,4.55
OVERLAP prop model 340 at 2.60,4.70
OVERLAP prop model 339 at 2.25,4.35
OVERLAP prop model 27 at 3.45,3.90
BURIED prop model 338 at 4.32,2.75
HANGING prop model 338 at 4.32,2.75
```

#### Map 71: FAIL: 69 placement problems

```text
HANGING prop model 339 at 3.85,1.95
BURIED prop model 34 at 1.75,3.05
OVERLAP prop model 34 at 1.75,3.05 with solid fixture
BURIED prop model 340 at 6.60,3.35
HANGING prop model 340 at 6.60,3.35
OVERLAP prop model 340 at 6.60,3.35 with solid fixture
OVERLAP prop model 194 at 9.60,8.45
HANGING prop model 339 at 9.65,8.30
BURIED prop model 186 at 11.50,7.70
HANGING prop model 186 at 11.50,7.70
OVERLAP prop model 186 at 11.50,7.70 with solid fixture
OVERLAP prop model 26 at 11.10,3.95
BURIED prop model 346 at 13.20,1.27
OVERLAP prop model 346 at 13.20,1.27 with solid fixture
BURIED prop model 346 at 13.70,1.27
OVERLAP prop model 346 at 13.70,1.27 with solid fixture
BURIED prop model 346 at 14.20,1.27
OVERLAP prop model 346 at 14.20,1.27 with solid fixture
BURIED prop model 346 at 15.65,1.27
OVERLAP prop model 346 at 15.65,1.27 with solid fixture
BURIED prop model 346 at 16.15,1.27
OVERLAP prop model 346 at 16.15,1.27 with solid fixture
BURIED prop model 346 at 16.95,2.90
HANGING prop model 346 at 16.95,2.90
OVERLAP prop model 346 at 16.95,2.90 with solid fixture
OVERLAP prop model 346 at 16.95,2.90
BURIED prop model 346 at 16.95,3.40
HANGING prop model 346 at 16.95,3.40
OVERLAP prop model 346 at 16.95,3.40 with solid fixture
OVERLAP prop model 346 at 16.95,3.40
BURIED prop model 346 at 16.95,3.90
HANGING prop model 346 at 16.95,3.90
OVERLAP prop model 346 at 16.95,3.90 with solid fixture
OVERLAP prop model 349 at 14.10,1.85
OVERLAP prop model 346 at 14.85,2.75
OVERLAP prop model 346 at 15.70,3.60
BURIED prop model 347 at 16.00,4.55
HANGING prop model 346 at 22.58,6.30
HANGING prop model 339 at 19.10,13.75
HANGING prop model 346 at 20.30,12.50
OVERLAP prop model 27 at 1.55,14.48
OVERLAP prop model 28 at 1.55,14.66
HANGING prop model 191 at 1.20,14.55
HANGING prop model 34 at 2.25,14.75
BURIED prop model 234 at 6.79,15.00
BURIED prop model 234 at 6.79,20.60
BURIED prop model 239 at 10.60,16.25
BURIED prop model 191 at 14.78,13.42
OVERLAP prop model 230 at 20.40,16.50
HANGING prop model 339 at 20.20,16.25
BURIED prop model 346 at 15.10,22.73
HANGING prop model 346 at 15.10,22.73
OVERLAP prop model 346 at 15.10,22.73 with solid fixture
BURIED prop model 346 at 16.10,22.73
HANGING prop model 346 at 16.10,22.73
OVERLAP prop model 346 at 16.10,22.73 with solid fixture
BURIED prop model 346 at 14.45,21.00
HANGING prop model 346 at 14.45,21.00
BURIED prop model 346 at 14.45,22.00
HANGING prop model 346 at 14.45,22.00
BURIED prop model 346 at 14.45,20.95
HANGING prop model 346 at 14.45,20.95
OVERLAP prop model 346 at 14.45,20.95
BURIED prop model 346 at 14.45,21.45
HANGING prop model 346 at 14.45,21.45
BURIED prop model 346 at 14.45,22.20
HANGING prop model 346 at 14.45,22.20
OVERLAP prop model 346 at 14.45,22.20 with solid fixture
```

#### Map 72: FAIL: 60 placement problems

```text
OVERLAP prop model 232 at 14.75,9.60
HANGING prop model 232 at 2.05,8.60
OVERLAP prop model 339 at 2.55,8.50
OVERLAP prop model 349 at 2.88,8.42
BURIED prop model 349 at 2.95,8.66
HANGING prop model 349 at 2.95,8.66
BURIED prop model 26 at 1.65,7.45
HANGING prop model 191 at 1.30,6.22
HANGING prop model 339 at 1.38,5.20
OVERLAP prop model 339 at 1.38,5.20
BURIED prop model 346 at 1.60,4.67
HANGING prop model 346 at 1.60,4.67
BURIED prop model 346 at 2.45,4.67
HANGING prop model 346 at 2.45,4.67
BURIED prop model 346 at 5.85,1.27
HANGING prop model 346 at 5.85,1.27
BURIED prop model 346 at 6.70,1.27
HANGING prop model 346 at 6.70,1.27
BURIED prop model 346 at 8.00,1.27
HANGING prop model 346 at 8.00,1.27
BURIED prop model 346 at 8.85,1.27
HANGING prop model 346 at 8.85,1.27
BURIED prop model 346 at 5.85,1.27
HANGING prop model 346 at 5.85,1.27
BURIED prop model 346 at 6.75,1.27
HANGING prop model 346 at 6.75,1.27
BURIED prop model 346 at 8.00,1.27
HANGING prop model 346 at 8.00,1.27
BURIED prop model 346 at 8.85,1.27
HANGING prop model 346 at 8.85,1.27
BURIED prop model 346 at 6.00,1.27
HANGING prop model 346 at 6.00,1.27
BURIED prop model 346 at 8.60,1.27
HANGING prop model 346 at 8.60,1.27
BURIED prop model 346 at 7.70,6.35
HANGING prop model 346 at 7.70,6.35
BURIED prop model 346 at 11.32,3.40
HANGING prop model 346 at 11.32,3.40
BURIED prop model 346 at 16.35,4.10
HANGING prop model 346 at 16.35,4.10
BURIED prop model 346 at 16.35,4.90
HANGING prop model 346 at 16.35,4.90
BURIED prop model 18 at 16.30,17.20
BURIED prop model 18 at 16.30,17.80
BURIED prop model 338 at 7.66,19.13
BURIED prop model 339 at 15.24,18.69
HANGING prop model 339 at 15.24,18.69
OVERLAP prop model 339 at 15.24,18.69 with solid fixture
BURIED prop model 338 at 13.72,18.26
HANGING prop model 338 at 13.72,18.26
BURIED prop model 339 at 1.25,14.75
HANGING prop model 339 at 1.25,14.75
BURIED prop model 339 at 1.25,16.65
HANGING prop model 339 at 1.25,16.65
BURIED prop model 346 at 1.27,20.85
HANGING prop model 346 at 1.27,20.85
BURIED prop model 346 at 1.27,21.85
HANGING prop model 346 at 1.27,21.85
OVERLAP prop model 347 at 21.18,22.40
```

#### Map 73: FAIL: 37 placement problems

```text
BURIED prop model 338 at 12.84,19.10
HANGING prop model 338 at 12.84,19.10
BURIED prop model 251 at 7.00,16.05
BURIED prop model 346 at 9.53,20.95
HANGING prop model 346 at 9.53,20.95
BURIED prop model 346 at 9.53,21.85
HANGING prop model 346 at 9.53,21.85
HANGING prop model 191 at 15.95,2.20
HANGING prop model 191 at 19.75,2.20
BURIED prop model 338 at 22.09,3.52
HANGING prop model 338 at 22.09,3.52
BURIED prop model 338 at 22.39,3.49
HANGING prop model 338 at 22.39,3.49
BURIED prop model 338 at 17.79,4.68
HANGING prop model 338 at 17.79,4.68
OVERLAP prop model 26 at 16.30,7.75
HANGING prop model 339 at 16.05,8.45
BURIED prop model 338 at 16.13,8.18
HANGING prop model 338 at 16.13,8.18
BURIED prop model 348 at 15.60,9.18
BURIED prop model 348 at 16.10,9.18
BURIED prop model 348 at 16.90,9.18
HANGING prop model 346 at 21.31,16.31
OVERLAP prop model 346 at 21.31,16.31
HANGING prop model 346 at 21.89,16.31
OVERLAP prop model 346 at 21.89,16.31
OVERLAP prop model 346 at 21.89,16.31
HANGING prop model 346 at 21.31,16.89
OVERLAP prop model 346 at 21.31,16.89
HANGING prop model 346 at 21.89,16.89
HANGING prop model 346 at 21.31,16.31
OVERLAP prop model 346 at 21.31,16.31
BURIED prop model 18 at 17.50,22.05
OVERLAP prop model 18 at 17.50,22.05 with solid fixture
BURIED prop model 18 at 18.15,21.30
OVERLAP prop model 18 at 18.15,21.30 with solid fixture
```

## Render artifacts

- `.build/job2/campaign-contact-sheet.png`: desk and glazing views before and two seconds after shotgun fire, maps 47, 52, 53, 64 and 68–73. The final capture uses fixed observer framing; some protected panes intentionally remain intact and crack. These are review artifacts while placement failures remain.
- `.build/job2/engine-comparison.png`: ten matched before/after views of 71/72, without post-processing. The before EXE was built in this worktree after temporarily reverting only the three rendering faults; fixed sources were restored. The plant-room blast view visibly exposes the legacy yellow paint in the before image and removes it afterward.
- `.build/job2/physics-inspection-sheet.png` and `.build/job2/fixture-part-comparison.png`: desktop impacts, thrown monitor, protected-glass stages/litter and bounded fixture-part destruction.
- Source PPMs, PNG thumbnails and the copied executables remain in their individual render directories.

## Gaps and handoff

1. **A is not green and is not committed.** The pending decision is whether to allow small placement corrections or retain unsafe items as fixtures. Exact source coordinates were preserved rather than silently moving authored content.
2. **E is not a complete all-green sign-off** while those placement failures remain. Functional suites and graphics checks passing do not replace that audit requirement.
3. The prop-only incremental rendering budget is not proven by the full-scene profiler; update and cache/spike evidence are reported separately above.
4. Physical VR headset/hand interaction was not manually exercised; tracked-hand behavior has an automated simulation check.
5. No purchased asset was created, changed, moved or deleted. Existing library assets were referenced/read only. No restricted source drive was opened, no other worktree was edited and nothing was pushed.

Latest built EXE SHA-256: `b995c05f631ac9a18c35842286c712f00b32787265304f183e25ab622ddd3d07`; 47,410,176 bytes, below 67,108,864.
