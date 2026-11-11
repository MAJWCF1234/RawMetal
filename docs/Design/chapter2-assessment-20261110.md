# Chapter 2 assessment, 2026-11-10

An independent, read-only assessment of every Chapter 2 map (campaign chunks 68-109), made before the completion pass of the same day. One assessor per district read the builders, ran the audit and placement check, rendered arrival-state views and rated each map; an eighth read the chapter as a whole. Ratings: **blockout** (route and boxes), **dressed** (a real place, thin gameplay or visible faults), **complete** (place, route, an authored beat, working light, nothing visibly broken).

## Summary

Of 42 maps: 17 complete, 23 dressed, 2 block-out. Every map passed the audit, the placement check and the campaign map test; no map is a placeholder.

## The chapter as a whole

Chapter 2 is a continuous, correctly gated route from 68 to 109 on paper, but it is not a chapter yet: it cannot be entered in normal play and it has no ending.

What holds: all 42 maps are authored builders (none are placeholders), all 42 audits show 0 FAIL, every planned edge is standable at its plan height, and --campaign-map-test passes. Each of the seven gates (proc_clearance, lab_chem_seal, med_override, med_blackout/med_morgue_release, cont_power, cont_barricade, core_shutdown) has its setter reachable before the gate, and I found no bypass by reading the geometry.

What does not:
- Entry: chapter2_open has no setter, and the tests assert the seal.
- Exit: 109 opens into placeholder chunk 110; Chapter 3 does not exist and nothing stops the player there.
- Evidence: authored depth and test depth both stop around chunk 82. 83-109 have no behavioural test, single-press gates, caption-only voices and radio orders with no effect.
- Fragility: the unpaired door at 81 N and Ward C's relocking maglock doors are untested risks on the critical path.

Honest label today: a complete, walkable block-out of the route, with 68-82 polished and tested, missing its chapter shell (entry, ending, route tests, voice, music). I judged flow from source, audits and the map test only; I did not look at renders or walk the route in play, so dressing and lighting quality are for the per-district assessors.

**Entry.** Not enterable in an ordinary playthrough. Chunk 67 (makeEnergyResearchGreatDoor, N:/rawmetal/src/world/CampaignChapterPerimeterB.cpp:801-861): event ere_finale_cleared (requireEnemiesClear) sets ere_finale_clear and saves; terminal "GREAT DOOR / LOCAL CONTROL" at (18,20.6) then sets ere_great_door, which unlocks 67's S transfer door (9-15 @ -12). The other leaf is chunk 68's door 0 (makeResearchIntake, N:/rawmetal/src/world/CampaignChapterResearchFront.cpp:158): N entry door 9-15 with requireState chapter2_open and notice sign CampaignChunkCount+2 ("ENERGY RESEARCH / SEALED", SoftwareRenderer.cpp:500). No map, terminal or script sets chapter2_open; grep finds it only in that line, in CampaignStateNames.inc and in the test. Game::useDoor/updateStreaming (N:/rawmetal/src/game/Streaming.cpp:32,69) refuse to mirror an opening through a locked partner, so opening 67's leaf leaves 68's shut. Ways in today: console UNLOCK while facing the door (N:/rawmetal/src/game/Console.cpp:23-31 sets the door's requireState), MAP 68, or developer mode. Second seal, for tools only: Chapter2Released=false and FirstSealedChunk=68 in N:/rawmetal/src/world/CampaignExport.h:17-19 (campaignChunkSealed), which hides chunks >=68 from the Campaign Builder list/export in a public release (CampaignExport.cpp:695,744,771). CampaignMapTest.cpp:209-213 asserts the seal (three checks, all PASS in my run), so opening the chapter means changing that test too. The plan (tools/campaign/plan.py chunk 68) has no state on this door; the seal exists only in the builder.

**Exit.** Geometrically the chapter ends at chunk 109 (makeExtractionInterface, N:/rawmetal/src/world/CampaignChapterDeepResearch.cpp:800-930): a lab slab, a void 8 m deep, one bridge, LAB 4's two lanterns on the far bank, and an open S seam 8-16 @ -15. The last gated beat is 107's core_shutdown; the last scripted beat is 108, where the Investigator gives orders through the booth glass and leaves by a lift the player cannot use (dr_lift_called is never set), with a checkpoint when he is gone. 109 itself has no script event, no checkpoint, no title card and no gate: one terminal, two Transformed and a Mutant.

The hand-over does not exist. Chunk 110 Mine Access is placeholderChunk(110,...) in N:/rawmetal/src/world/CampaignChapterMinehead.cpp (21 lines); so are 111-178. CampaignKit.h:707-725 builds each as a bare room with four lamps and a zone named "<NAME> / UNDER CONSTRUCTION". Because 109's S side is an open seam, 110 is streamed in and visible from the Extraction Interface (Streaming.cpp:62). The seam passes edge_check, so the player can walk straight into it. There is no end-of-chapter mechanism in the code (no end card, no Chapter 3 seal state, no second threshold in campaignChunkSealed). Flipping Chapter2Released would also list and export the placeholder chunks 110-178.

**Tests.** Run by me: `python tools/dev/agent.py test asFlow --campaign-map-test` exited 0; N:/rawmetal/.build/agents/asFlow/freight-district-test.txt has 596 PASS lines and no FAIL (exe sha1 a724a2f9cf01, BUILD OK without compiling). `python tools/campaign/edge_check.py <audits> 68 109` printed "edges ok". I did not regenerate audits: I used the 42 map-audit files the sibling assessors wrote today at 16:51-16:53 under .build/agents/as*/ from the same shared exe. All 42 have 0 FAIL lines; the only WARNs are "props exceeds 120 per chunk" in 68, 70, 71, 72, 73, 81, 82 (146 to 379).

What CampaignMapTest (N:/rawmetal/src/game/CampaignMapTest.cpp:203-520) covers in 68-109:
- 68: the seal (3 checks) and the radio sweep timer.
- 69: the wounded private is on truce with a talk node.
- 70: the Investigator scene (sight lines, glass stops shots, flinch, he leaves, records opens).
- 72: the triage patient sits up.
- 74-78: 9 mm pickups reachable, creature standing room, pharmacy time lock and shutter, Ward C lamp circuits, dark zones, wake states and blackout timing, Surgery transfer and mortuary release with real E presses, Morgue door, dark store and subject 40.
- 79-82: eight swing doors walked, four hunt-through-door checks, 81's S door on lab_chem_seal, and the purge console pressed with E.
- All chunks 34-178: loads and the spawn fits.

Not covered:
- 71 and 73 beats, and everything in 83-109 beyond spawn fit: no behavioural test at all for Lab 4, the Energy Test Complex, Containment or Deep Research.
- Gates never exercised from their real setter: proc_clearance, med_override (tests inject it), 84's airlock, res_lab_unsealed, cont_power (console, 95 door, 97 bulkhead), cont_barricade, core_shutdown, the 108 scene.
- No Chapter 2 seam is walked, no save/load is taken on Chapter 2 state, and there is no end-to-end run.

Other suites (read, not run):
- chapter-route (testChapterRoute) walks only N:/rawmetal/src/game/ChapterEdges.inc, which tools/campaign/edges_inc.py generates for chunks 34-67 only (73 lines); it has no Chapter 2 edge.
- chapter-sequence (testChapterSequences) is 7 checks, all in Power and Machine Hall (39-42).
- The export round trip lists 68, 74, 78, 81, 82, 90, 102, 107 under dev tools (CampaignExport.cpp:672).
- The placement checker was not run by me.

The map audit floods with every door open, so "0 FAIL" and "edges ok" prove geometry and edge heights, not gating or order.

**Softlock and stall risks**

- No way in: chapter2_open has no setter, so a normal save stops at 68's door 0 after the Chapter 1 finale.
- No way out: 109's S seam is open into placeholder rooms 110-178 (Minehead onward are all placeholderChunk). If the chapter were unsealed as built, the player walks grey rooms until chunk 123's S door on mine_drill_power, which nothing sets. There is no end beat and no Chapter 3 seal (campaignChunkSealed has one threshold, 68).
- Ward C exits depend on persisted door leaves. Recovery (20-21.2, y 8.9) and mortuary (11.4-12.6, y 19.1) in 76 require med_blackout==1; once 77's transfer sets it back to 0 they are locked again and stay passable only because their open state is saved (Saves.cpp:104-108) and a locked door cannot be toggled (Movement.cpp:320). Anything that closes one (a CloseDoor action, a reset, a later map edit that reorders 76's doors against an old save) strands the player between 77, the ward and 78. Tested in one session only.
- The blackout is the single trigger for the whole second half, at OBS 6's bed. The locked maglock doors show only the generic "BULKHEAD / LOCKED" hint (Movement.cpp:246). A player who does not step to her bed has nowhere to go; this is a discoverability stall, not a hard lock.
- 81 N is an unpaired entry door (plan: seam). If E cannot reach it from 79's side of the seam the critical path stops at 79 -> 81. edge_check passes because the audit floods with every door open. I did not verify this in play.
- 77 ordering: medd_ward_transfer is set only by event medd_transfer_arm while med_blackout==1 and the player is in box (16.6,7.4)-(23,15.6). The desk at (18.25,7.78) is inside that box, so it arms on approach. Low risk, but the mortuary release depends on it.
- Skippable content rather than a break: the Lab 4 service door delivers the player to 76's W seam a few metres from the OBS 6 trigger, so 73 Imaging, 74 Pharmacy, 75 Ward C North and the "return to the clinic with the disk" beat are all optional. Fully optional chunks: 72, 73, 74, 75, 80, 84, 85, 86, 96, 97, 99, 100, 101, 105, and one of 88/89.
- Sequence breaks: none found by reading. Every gate's side route is closed (84 airlock on lab_chem_seal, 97 bulkhead on cont_power, 83/85 back doors on res_lab_unsealed, 76 maglocks). Not proven: no tool floods with locked doors shut. The audits report 20-22 cells reachable only through breakable glass in 68, 69, 82 and 83 (2 in 72); I did not inspect those pockets individually.

**Plan against what is built**

- Edges: a scripted comparison of every openEdge call in the eleven Chapter 2 files against plan.py finds two real differences in 42 chunks. 68 N carries chapter2_open (plan: no state). 81 N is a door (plan and 79 S: seam).
- Chunk 110: the plan and doc describe Mine Access as a staging depot; built is a placeholder, as are all of 110-178.
- Observation Ring 89: plan says "a stair down into the pit for the brave (pickups, an anomaly core)". No stair exists in CampaignChapterObservationRing.cpp (the ladder at line 171 is a fixture), and audits for 87-90 reach only -12 to -11 (-9.5 in 90). The -28 pit is scenery.
- docs/Design/lab4-field-measurement-plan-20261104.md promises one new creature (Pointer: R-46 in 84's pan, R-47 on the disk in 83) and a lure mechanic with walking lamps. CreatureKind has no Pointer (WorldDefinition.h:34) and 83 and 84 have no creature spawn. The readables, the tape, the walking-lamp terminals, anl_fm_bus and anl_l4_gutter are all built and describe an animal that never appears; the disk is simply picked up.
- Design doc, Investigator in Containment: truces are to be revoked over the radio and squads to turn. Built: cona_hub_radio, cona_b_radio and conb_ec_radio are captions only; their *_done states are read by nothing. The hub is ordered to "Detain her" and stays on truce. Real stance flips exist only in 71 (medf_screen_order) and 91 (et_shield_order).
- Design doc: every line voiced. N:/rawmetal/src/audio/NpcVoiceCatalog.h has Chapter 2 keys only for 68-71 (inv_intake_*, inv_admin_*, rf_intake_lead_*, proc_*, inv_screen_*, md_screen_a_*) and none for anl_, et_, rg_, cona_, conb_ or dr_. From 72 on, Investigator radio, squad dialogue and the 108 scene are caption-only (Sound::Empty cues).
- Music: plan.py NEW_CUES names Research and Clinic cues but WIRED_CUES is empty and MusicCue has neither. Chapter 2 plays Chapter 1 loops: Subliminal on 68-70, 79-93, 104-109 and DeathClose on 71-78, 94-103.
- Story spine step 5 ("the player returns to the clinic with the disk") versus the district brief ("Lab 4's service door leads straight into Ward C"): both are built, and the service door makes the clinic return optional.
- 108: the Investigator promises the cage will come back; the lift door state dr_lift_called has no setter and Chapter 3's cage lift is planned in a different chunk (112).
- Matches worth noting: 70's card index shows LAB 4's line with the card gone (ResearchFront.cpp:1024-1026), 71's board line, "FOLLOW THE LAMPS" in 94, 100 and 105, and 77's transfer is one-way where the plan said toggle (a deliberate tightening, tested).
- Title cards exist for RESEARCH FRONT, DEPTHWORKS MEDICAL, WARD C (75 and 76), LAB 4, ENERGY TEST COMPLEX (85 and 90), CONTAINMENT and DEEP RESEARCH. The Analysis Laboratories district has none at its entry (79).

**Needed before it is a complete chapter**

- Build the entry. Give chapter2_open a setter tied to ere_great_door (or drop the requireState on 68 door 0), replace notice sign 2 "ENERGY RESEARCH / SEALED", set Chapter2Released, and rewrite CampaignMapTest.cpp:209-213, which currently asserts the seal.
- Build the exit. Either author chunk 110 (at least) or end the chapter at 109 with a real beat (checkpoint, card or door) and a Chapter 3 seal: a state-locked boundary at 109 S / 110 N plus a second threshold in campaignChunkSealed so placeholders 110-178 stay hidden when Chapter 2 is released. Today 109 opens into an "UNDER CONSTRUCTION" room that is visible from the bridge.
- Test the route. Extend tools/campaign/edges_inc.py from 34-67 to 68-109 so testChapterRoute walks every Chapter 2 edge, including 81's one-sided door. Add a gate-chain test that presses each real setter with E and checks its gate: 70 pass terminal -> 69 door, Hale's disk -> service door and 71 door, 84 airlock, 85 seal, 93 routing -> 95 door and 97 bulkhead, 103 barricade, 107 isolation. Add a locked-door flood per gated chunk to prove no bypass, and a save/load after 77's transfer to prove 76's recovery and mortuary doors stay passable.
- Fix the two fragile gates. Make 79 S / 81 N a proper pair (transfer plus entry door in plan and maps) or return 81 N to a seam. Key 76's recovery and mortuary doors to a latched state (medw_dark_done or medd_power_back) instead of med_blackout==1 so they cannot relock.
- Decide Lab 4 and Field Measurement: build the Pointer creature and the lamp lure from the 2026-11-04 plan, or cut R-46/R-47 from the readables and tape. As built, 83's story beat has no threat on the disk and 84 is a creature-less spur.
- Voice the back two thirds: about 20 Investigator radio lines, the squad dialogue nodes in 90, 95, 98 and 103, and the 108 scene. None are in NpcVoiceCatalog.h.
- Make the Investigator's Containment orders change something: drive cona_hub_stance, cona_b_stance and conb_ec_stance from the *_radio_done states, as the design decided on 2026-10-29.
- Wire the Research and Clinic music cues, or accept Chapter 1 loops and remove them from plan.py.
- Smaller: build or strike the ring pit descent in 89; give Analysis a district title card; put the reason on Ward C's maglocked doors; check performance on the chunks over the 120-prop budget (73 has 379); pay off or cut the 108 lift line.

**Critical path**

| From | To | Gate | Opened by | Risk |
|---|---|---|---|---|
| 67 | 68 | 67 S transfer door on ere_great_door; 68 door 0 (N 9-15) on chapter2_open | ere_great_door: terminal at 67 (18,20.6) after ere_finale_clear. chapter2_open: nothing in the game sets it (console UNLOCK / MAP 68 / dev mode only) | Hard block by design. Tests assert it stays sealed. |
| 68 | 69 | none: S seam 10-14 @ -12 | n/a. Beat: Investigator radio order sets rf_intake_sweep 13 s after the vestibule (tested) | none found |
| 69 | 70 | none: W seam 9-14 (spoke to the clearance desk) | n/a. In 70 the pass terminal (CampaignChapterResearchFront.cpp:937-939) sets proc_clearance, prerequisite rf_admin_gone (Investigator scene, about 28 s, checkpoint when the far door shuts) | Scene is tested; pressing the pass terminal is not tested. |
| 69 | 79 | 69 door 0, S transfer 10-14, requireState proc_clearance; pairs with 79 N entry door | proc_clearance from 70, reachable before the gate (70 is 69's west seam) | No test checks that the terminal unlocks this door or walks the seam. |
| 79 | 81 | 79 S is a seam; 81 door 0 is an unlocked N entry door (CampaignChapterAnalysisLabs.cpp:93). Plan says seam on both sides | E on the door from inside chunk 81 | Only unpaired boundary door in the chapter. E only finds doors of the chunk the player is in (Movement.cpp:198), so it must be opened from the first ~0.3 m of 81. Not walked by any test; I could not verify it in play. |
| 81 | 82 | none: W seam 9-14 (spoke to the purge) | n/a. Purge console in 82 (CampaignChapterAnalysisLabs.cpp:408) sets lab_chem_seal, no prerequisite (tested with E) | none found |
| 81 | 83 | 81 door 1, S transfer, on lab_chem_seal (pairs with 83 N entry door 0). Alternative: 82 S -> 84 N seam, then 84's airlock swing door (19.4-20.6, y 8.9) also on lab_chem_seal, 84 E -> 83 W | lab_chem_seal from 82, which is passed before either gate | 81's door is tested; 84's airlock is not. |
| 83 | 76 | Lab 4 service door, swing door (19.4-20.6, y 14.9) on med_override (CampaignChapterAnalysis.cpp:721), then corridor to E seam 15-19 -> 76 W. Same state opens 71 door 0 (S) -> 73 -> 75 -> 76 | med_override: paper "WARD C OVERRIDE / R. HALE" on Hale's body at (20.5,14.4), right at the door (Analysis.cpp:805) | Untested (tests inject med_override). The planned R-47 lure is not built, so the disk is free once past the two-man Diagnostics detail. 83's S back door (10-14, y 19.1) stays shut on res_lab_unsealed. |
| 76 | 77 | Recovery door (20-21.2, y 8.9), requireState med_blackout==1 (maglock) | Stepping into OBS 6's box (6.1-9.7, 20.5-22.9): checkpoint, 2.6 s, then event medw_blackout_fall sets med_blackout=1 and OpenDoor's OBS 5, recovery and mortuary (CampaignChapterMedicalWard.cpp:565-573) | Single trigger for every way on. After power is restored the door is logically locked again and is passable only because its open leaf is persisted. See softlocks. |
| 77 | 78 | Back through 76: mortuary door (11.4-12.6, y 19.1) on med_blackout==1, then 76 S seam -> 78 N landing | Same blackout script. In 77: transfer desk (18.25,7.78) needs med_blackout, sets medd_transfer_thrown -> med_blackout=0, medd_power_back=1; strongroom console (19.8,1.75) needs medd_ward_transfer, sets med_morgue_release (CampaignChapterMedicalDeep.cpp:330-388) | Well tested in one session with real E presses; not across save/load or chunk unload. |
| 78 | 94 | 78 door 0, S transfer 10-14 @ -15, on med_morgue_release; pairs with 94 N entry door | Strongroom console in 77, reachable before the gate | Door lock/unlock tested; the seam is not walked. |
| 94 | 95 | none: S seam (94 E -> 100 Isolation Wing is an optional story spur) | n/a | none found |
| 95 | 90 | none: gallery stair to -12, W seam 9-14 @ -12 -> 90 E. 95 door 0 (S, cont_power) is shown locked here | n/a. Intake detail on truce (cona_hub_stance) | none found |
| 90 | 87 | none: ring walkway at -12 via 88 or 89 (seams 1-23) | n/a | none found. The pit below is unreachable (reached heights -12..-11 only). |
| 87 | 91 | none: W seam 9-14 | n/a. Shielding detail turns Hostile after its radio order (et_shield_order) | none found |
| 91 | 93 | none: 91 S -> 92 N (-12), 92 S -> 93 N (-16) | n/a. In 93: "OUTPUT ROUTING / CONVERSION" console at (20,1.75) on the -12 mezzanine (stair 16-18, y 9-15.16) sets cont_power, no prerequisite (CampaignChapterEnergyTestLabs.cpp:714-715) | Zero tests in 85-93. Optional from here: 85's seal console sets res_lab_unsealed (opens 85 door 0 and 83's back door, a loop). |
| 95 | 98 | 95 door 0, S transfer, on cont_power; pairs with 98 N entry door. Bypass 96 -> 97 -> 98 is closed by 97's bulkhead (10.6-13.4, y 11) on the same state | cont_power from 93, reachable first through 95's west gallery | Untested. Bulkhead closure checked by reading the walls (slot spur y 6.6-8.4 and plenum y 12-20 do not connect), not by a locked-door flood. |
| 98 | 102 | none: W seam 9-14 | n/a | none found |
| 102 | 103 | none: S seam 10-14 @ -15 | n/a | none found |
| 103 | 104 | 103 door 0, S transfer, on cont_barricade; pairs with 104 N entry door | Terminal "EMERGENCY BARRICADE / SOUTH" at (22.2,10.6) in the east control room, no prerequisite; raising it spawns Transformed at (11,3) and (14,4.2) (CampaignChapterContainmentB.cpp:597-607) | Untested. Containment's own gate is one E press. |
| 104 | 107 | none: 104 S -> 106 N, 106 S -> 107 N gallery @ -15 (105 archive is an optional spur) | n/a | none found |
| 107 | 108 | 107 door 0, S transfer at gallery level, on core_shutdown; pairs with 108 N entry door | Terminal "CORE FEED / ISOLATION" at (17.6,16.9) on the pit floor (-25), past periodic arc hazards (35 dps) that the same state switches off (CampaignChapterDeepResearch.cpp:598-604) | Untested. |
| 108 | 109 | none: S seam 9-15. The lift door (16-23, y 6) is on dr_lift_called, which nothing sets | n/a. Investigator scene dr_inv_scene starts at box (9,1)-(15,4); checkpoint when he is gone | His line "The cage will come back up for you" pays off nowhere. |
| 109 | 110 | none: one bridge (x 7-10.6) over the seam void, S seam 8-16 @ -15 | n/a | Leads into placeholderChunk(110), zone "MINE ACCESS / UNDER CONSTRUCTION". No end gate. |

## Research Front (68-70)

This district is well past block-out: 68 and 70 are authored, tested maps with real set pieces (the radio sweep, the Investigator behind glass, the clearance terminal), and all three pass audit, check and the campaign test. 69 works as a hub and its gate is correct, but its side rooms are thin and its one character beat shows a standing, armed man saying he cannot lift his gun. The largest issue is not in the builders: door 0 of chunk 68 is sealed on chapter2_open, which no normal playthrough sets, so nobody reaches any of this yet. The most valuable work is to lift that seal on purpose, fix the private's pose, cut the prop counts in 70 and 68, and then play the two guard encounters, which I could only read in code.

**Top fixes**

- Decide chapter2_open: set it from chunk 67's great door (ere_great_door flow) or remove the requireState at ResearchFront.cpp:158, and rewrite the assertions at CampaignMapTest.cpp:209-213. Until then chunks 68-109 are unreachable in play.
- 69: give rf_proc_private at (1.9,6.75) a seated or slumped pose (or rewrite proc_hello), and make rf_proc_heard / rf_proc_hint do something or delete them.
- 70: cut props from 260 to under 120, starting with the roughly 87 individual office_file_binder props and 30 id cards; 68: trim 146 to under 120.
- Play-test what was only read in code: 68's two sweep routes, alarm and PA/film distractions, and 70 when the player leaves for 69 before rf_admin_gone.
- 69: put one idea in each side room, starting with Screening (14.6-23, 19.4-23) and Suit Return, and break the uniform cold-white lighting.
- Replace or retint the 23 rusted psx_locker fixtures across 68 and 69; they fight the clean corporate front.
- 68: light or swap the three dark paintings that read as black squares, and either add a switch for rf_intake_theatre_lights or drop the two dead lamps.
- 70: owner call on the two slogan sign plates against the signage rule; brighten the near-black stair at (21-23, 5-8.8).

| Chunk | Map | Rating | Effort | Beats |
|---|---|---|---|---|
| 68 | RESEARCH INTAKE | complete | small | Radio sequence rf_intake_radio starts in the vestibule box and at 13 s sets rf_intake_sweep, releasing two of four Cleanup guards (lead at the island, medic patrol, rusher and engineer on authored sweep routes through both side wings). rf_intake_alert drives rf_intake_alarm (20 s alarm, two red strobes) and breaks the exhibit glass part. Two distraction terminals: INTAKE PA (rf_intake_page, aiRadi |
| 69 | PERSONNEL PROCESSING | dressed | small | Hub gate: S door on proc_clearance (set in chunk 70), with two red wall lamps inverted on the state and a white tube that comes on when cleared, plus the LABORATORIES / CLEARANCE ONLY plate. One truce NPC: rf_proc_private at (1.9,6.75), stance rf_proc_stance set to Truce by ScriptEvent rf_proc_truce on entering the link; a four-node voiced dialogue (proc_hello, proc_who, proc_squad, proc_admin) th |
| 70 | RESEARCH ADMINISTRATION | complete | small | Set piece: rf_admin_scene (30 s, triggered in box 17-24 x 9-14) plays the Investigator behind the records glass; at 21 s rf_admin_leaving opens far door 2, rf_admin_exit then sets rf_admin_gone, which shuts the far door, saves a checkpoint and unlocks records door 1. Gate: clearance terminal at (21.6,1.45) sets proc_clearance, prerequisite rf_admin_gone. Two armed guards: CleanupHeavy at the corri |

### 68 RESEARCH INTAKE

makeResearchIntake: the checkpoint behind the great door. Interlock vestibule (8.8-15.2, 1-3.15), a three-booth screening line (y 5.8-7.4), security post and kit room west, badge office east, holding pen with cell bars, effects store, staff corridor, welcome theatre with the film screen, visitor lounge, concourse with a reception island, warm-lit exit lobby south. Lab panel walls, clinic tile floor, about 35 lamps with a deliberate cold/warm split. Reads as a specific place in the renders.

Route: N 9-15 @-12 (door 0, gated on chapter2_open) to S 10-14 @-12 (open seam). Three internal routes from the approach: centre booth lane 2 along the yellow line; staff side (post, kit room, swing door at 1.4-2.4/6.5, corridor, exit lobby); visitor side (badge office, swing door 17.8-18.8/6.5, theatre, door 21-22/13.1, lounge). Audit: 5691 reachable cells, border cells match the plan on both edges, PASS.

Audit: PASS: 0 placement problems. WARN 146 props exceeds 120 per chunk. check: PASS. Guard behaviour, the alarm and stealth balance were not played, only read in code.

Faults:

- **major**, N edge door 0 (9-15, y 0), line 158: requireState chapter2_open is set by nothing in a normal playthrough (CampaignMapTest.cpp:209-213 asserts it stays sealed; only console UNLOCK or MAP 68 opens it). The plan lists this edge as a plain door. It is a deliberate release seal, not a builder defect, but it is the one thing keeping players out of all of Chapter 2.
- **minor**, whole map: 146 props against the 120 budget (audit WARN): scattered item_id_card and item_dosimeter props account for most of the excess.
- **minor**, art_painting_5 at (17.16,16.4) and (8.4,19.96); art_painting_6 at (22.985,16.3): Read as flat black squares from across the room in spot and sweep frames; the dark landscape only resolves at under 1 m. The lounge painting has no picture light at all.
- **minor**, kit room lockers (1.45, 3.7/4.6/5.5), lounge lockers (17.85, 14.6/15.5): psx_locker is heavily rusted and perforated; it clashes with the clean lab-panel corporate front (visible in sv_68_2).
- **minor**, booth piers, y 6.05-6.8: Six pwr_vent_grille and three lab_id_scanner fixtures show as featureless black rectangles under the 1.3-intensity lane tube (sv_68_0, sv_68_4). Could not tell from the frames whether that is dark texture or the model's side; the facing checker passes.
- **minor**, theatre house lamps (19.6,8.4) and (21.4,11.6): Switched on rf_intake_theatre_lights, which nothing sets: there is no switch, so they can never light. Dead state.
- **minor**, south link, x 10 and 13.97, y 20-24: Walls are plaster (mat 21) under 3400 K; 69's side of the seam is lab panel under 6000 K, although the builder comment says 69 carries the plaster on. A material change at the seam is visible in a sweep frame (256 px, moderate confidence).

Missing for complete:

- A decision on chapter2_open: set it from chunk 67's great door flow or drop the requireState, and update the test at CampaignMapTest.cpp:209-213
- A play pass of the sweep encounter (not verified here): both sweep routes, the alarm, and the PA and film distractions pulling guards
- Trim about 26 props to get under the audit budget
- Lift or replace the three dark paintings; clean lockers

### 69 PERSONNEL PROCESSING

makePersonnelProcessing: the hub. Link from Intake into a crossing hall (8.6-15.4, 4-14) with three ticket gates under a lit header and a red-lit clearance vestibule south; W arm to Administration, E arm to Medical; washroom, locker room, suit store, dress-out, tier office, screening room and suit return around it. The hall axis to the red door reads well (sv_69_0). The side rooms are grey panel boxes with a handful of props each under uniform 5000-6200 K overheads.

Route: N 10-14 @-12 (open) into the hall; W 9-14 and E 9-14 open seams; S 10-14 door 0 gated on proc_clearance. Three staff loops through the side rooms. Audit: 6089 reachable cells, all four edges match the plan, PASS.

Audit: PASS: 0 placement problems, no warnings. check: PASS. Campaign test confirms the private is on a truce and has dialogue.

Faults:

- **major**, locker room, rf_proc_private at (1.9,6.75): The 'wounded' private stands upright holding his pistol two-handed at low ready (sv_69_3) while his first line is that he cannot lift it and the builder comment has him sitting it out. The map's only character beat visibly contradicts itself; the builder notes no sitting clip exists.
- **minor**, dialogue replies, lines 774 and 778: rf_proc_heard and rf_proc_hint are set and read by nothing anywhere in src. The conversation changes nothing downstream.
- **minor**, side rooms: Screening (14.6-23, 19.4-23), washroom, suit store, suit return: Thin. Screening is a nurse desk, two chairs and a wheelchair against a blank 8 m panel wall (sv_69_1). Five of nine rooms offer only a pickup or a note; the three loops have no payoff.
- **minor**, crossing hall (8.6-15.4, 4-11): About 50 square metres of open tile with two benches, a cart and paper drift; reads empty in sv_69_4.
- **minor**, locker room (13 lockers at y 4.25 and 8.35), dress-out (5 at x 22.55): 18 rusted psx_locker fixtures in what is written as a clean change room.
- **minor**, whole map: Lighting is even cold white in every room; the only accents are the red clearance lamps and three flickering tubes. Legible but flat.

Missing for complete:

- A wounded pose for the private (seated or slumped against the lockers) or a rewrite of his lines to fit a standing man
- A consumer for rf_proc_heard / rf_proc_hint, or remove them
- One idea per side room: at least Screening and Suit Return need something to find or do
- Some light variation: for example the dress-out and suit return in a different temperature from the public hall

### 70 RESEARCH ADMINISTRATION

makeResearchAdministration: a two-storey atrium (12-23, 5-16, ceiling -6) with a gallery deck at -9 over the clearance counter, records office behind protective glass and a barred hatch on the west face, lounge under a bulkhead south, boardroom and copy room north-west, the director's suite and anteroom upstairs, a sealed lift lobby behind records. Uses lab_exec_desk, lab_boardroom_chair, office_bookcase with binders, lab_anomaly_relic in a lit case. Reads as a real executive floor in sv_70_0, sv_70_4 and the sweep.

Route: Single edge E 9-14 @-12 (open seam from 69); in and back out. Stair (21-23, 5-8.8, 16 steps) to the gallery and suite. Audit: 8241 reachable cells, edge matches the plan, PASS. The audit counts the lift lobby (4.4-9, 19.2-23) as reachable; in play its door never opens for the player.

Audit: PASS: 0 placement problems. WARN 260 props exceeds 120 per chunk. check: PASS. Not verified: what happens if the player walks back to 69 mid-scene, and what the clearance terminal shows before rf_admin_gone.

Faults:

- **minor**, whole map: 260 props, more than twice the 120 budget (audit WARN). About 87 are individual office_file_binder props on the two bookcases at (1.27,4.0) and (1.27,3.2) and the copy-room shelf; about 30 more are item_id_card.
- **minor**, sign plates at (12.02,13.2) and (22.86,11.5): Both are slogans ('YOUR WORK POWERS THE WORLD', '1,204 DAYS, NO LOST-TIME INCIDENT'). The design doc's signage rule excludes slogans, and the first repeats the theatre screen in 68. The audit does not flag it; the district brief does ask for propaganda, so this needs an owner call.
- **minor**, stair (21-23, 5-8.8): Treads and stringers render near-black against the lit atrium and read as a dark mass (sv_70_0, sweep).
- **minor**, suite window (x 11.8-12, y 5-7.8, z -9) and gallery rail: Material 20 frames look mottled and corroded in sv_70_3, out of key with a clean executive floor.
- **minor**, far door 2 (6.2-7.4, y 19.1): Gated on rf_admin_far_door, which nothing sets; only the script opens it. No plate or light tells the player why it is shut after records opens.

Missing for complete:

- Get props under budget: bake the binder rows into the bookcase or use far fewer, larger pieces
- Confirm no stall if the player leaves for 69 during the 25 s scene (untested)
- Owner decision on the two slogan plates
- Light or recolour the stair so it reads

## Depthworks Medical, front (71-74)

Chunks 71-74 are not block-outs: each is a dense, specific clinic department with pack-model dressing, deliberate lighting, an authored beat in code, a clean audit and check, plan-true edges and a passing map test. I rate all four complete on static evidence, with Imaging (73) the closest to the line: it is three times over the prop budget and its lightbox set piece is too dim to read from the door. None of the scripted chains was exercised in play; I only rendered lit, blackout and override states. The most valuable work is one played pass through the four chains plus a handful of small fixes, not more building.

**Top fixes**

- Play each scripted chain once end to end, since none was exercised here: 71 truce, radio, hostile and the hold-wake on return; 72 turn, rise, hush, blackout, breaker; 73 count, wake, radio; 74 keyed, 20 s, open, slam, staff door.
- 71: medf_screen_b_spared is set by the 'Go. Back the way I came.' reply and read nowhere. Gate medf_screen_order on it (or send guard B out the W seam), or remove the reply.
- 73: cut loose props from 379 toward 120 by baking the film-library binders (shelves at x 14.47-18.5, y 15.25-17.4) into fixtures or merged rows.
- 73: make the med_xray_viewer_* banks read as lit lightboxes (reading room 15.25-22.25 at y 1.09, X-ray 1, X-ray 2), and replace viewer 13 (chest) with a clear skull film to match LAB 4's 'SKULL AP: CLEAR' note.
- 74: give the cage gate (3.6-4.6, y 3.5) a grille leaf and a time-lock prompt instead of the rusted leaf and 'BULKHEAD / LOCKED', and check the staff door (7.6-8.5, y 8.9), which uses the same helper.
- 74: light or cue the jammed shutter gap (16-19, y 10.9) from the corridor side so the prone crawl reads; the corridor tube at (15.2,12.5) and the dead one at 19 leave it nearly black.
- 71: move the bare lamp at (2.6,1.15) off the waiting-room wall to remove the jagged blotch; consider starting md_screen_radio from a box nearer the vestibule than the W seam (1-3 x 9-14) so there is time to talk to guard B. Trim 71 and 72 floor scatter (189 and 172 props) toward budget.
- 73: dress the reception hall (14.2-23 x 9-14) and the reading room's east wall, and add perch proxies on the library shelf tops and X-ray equipment (also the gas cylinders in 72's plant room).

| Chunk | Map | Rating | Effort | Beats |
|---|---|---|---|---|
| 71 | MEDICAL SCREENING | complete | small | Gate: S door on med_override, with a red lamp that swaps to white and a TRANSFER INTERLOCK RELEASED header when set (seen in render). Encounter: two Cleanup guards (md_screen_a at the barrel, md_screen_b sharpshooter patrolling the spine) on medf_screen_stance; Truce on entry, Hostile when the md_screen_radio sequence finishes at 14 s (md_screen_radio_done), back to Truce if md_screen_a_dead. Dial |
| 72 | MEDICAL / TRIAGE | complete | small | Scripted scare chain: looking at the shot nurse (actor 0, sightActor) or reading the BAND DESK / ISSUE LOG sets md_triage_turn, then md_triage_rise: the Worker in bay 1 comes up onto his knees facing the office window (md_triage_risen). Passing his bay fires md_triage_hush_go: his whispered line, then aisle lamps die far to near (md_triage_dark_1/_2/_3, md_triage_dark), a DarkZone covers the bays  |
| 73 | MEDICAL / IMAGING | complete | medium | Encounter: entering the counter suite (1-6.6 x 15.2-23) starts md_imaging_count, accelerating ticks for 3 s, then md_imaging_wake wakes one Transformed (subject nine) inside the open chamber at (2.6,21). Afterwards the dead detail man's walkie plays md_imaging_radio (the Investigator calling a detail that does not answer). Story: X-RAY 1 exposure log, radiology readings terminal, LAB 4's clean-fil |
| 74 | MEDICAL / PHARMACY | complete | small | Puzzle: CD CAGE / TIME LOCK pad at (6.9,3.56) sets medf_cage_keyed, then medf_cage_timer: 20 s of ticks while knocking from the cold room builds to hammering and stops; medf_cage_open releases the gate (3.6-4.6, y 3.5); medf_cage_slam gives one slam and a shake. Reward in the cage: two health kits and 9 mm. Approach knock medf_knock near the cold-room door. No combat by design. Story: sedation sta |

### 71 MEDICAL SCREENING

makeMedicalScreening: the clinic front as 14 named rooms on a teal/clinic-tile spine. Lobby with reception screen, waiting room, four screening cubicles, counting room, dosimeter store, records with a burn barrel, vestibule, tier-3 hold, transfer passage to the Ward C door, nurses' station, linen, staff room and WC. Dressed with med_/lab_/office pack models throughout; half the spine is deliberately dark.

Route: W seam 9-14 (from 69) east along the spine to E seam 9-14 (into 72); S door 10-14 gated on med_override (to 73). All three edges match the plan table. Audit reaches every border cell and 5828 cells; staff-side loops (waiting room to technician corridor to records; transfer to linen to staff room to nurses' station) are open.

Audit: PASS: 0 placement problems. WARN 189 props exceeds 120 per chunk. 29 lamps, 1 sign plate. check PASS.

Faults:

- **major**, dialogue medf_b_hello, reply 'Go. Back the way I came.': It sets medf_screen_b_spared, which nothing in src reads (single occurrence). Sparing the watcher is a no-op: he still turns Hostile on md_screen_radio_done.
- **minor**, md_screen_radio sequence, trigger box (1-3, 9-14) at the W seam: The 14 s truce clock starts on the first step into the chunk while guard B stands at (20.6,11.5), about 18 m away, for his first 6 s. Whether there is time to reach him and talk was not verified in play.
- **minor**, waiting room north wall, call display at (2.6,1.06): The bare 9000 K lamp at (2.6,1.15) sits 9 cm off the wall and throws a hard, jagged bright blotch around the display (a71_3waiting.png).
- **minor**, whole map: 189 loose props against the 120 budget (audit WARN); mostly paper sheets, ID cards, dosimeters and cartons.
- **minor**, cubicle and room doors: Interact prompt reads 'CLOSE SERVICE DOOR' on clinic doors (engine default label; same in 72-74).

Missing for complete:

- Make medf_screen_b_spared do something (gate medf_screen_order on it, or have B walk out the W seam) or drop the reply
- Play-verify the truce, radio, hostile timing and the hold-wake ambush when returning from 73 with med_override
- Pull the call-display lamp off the wall
- Trim floor scatter toward the 120-prop budget

### 72 MEDICAL / TRIAGE

makeMedicalTriage: a dead-end ward off 71. Queue rail and band-office hatch, eight curtained bays on one aisle ending at a barred, red-lit observation hold, plus resus, decon, sluice, clean supply, service corridor with the lighting breaker, staff room, records/band print, linen, overflow day-room with mattresses on the floor, and a plant room. The aisle framed on the red hold lamp is a strong landmark from the seam.

Route: W seam 9-14 only (matches plan); in and back out. Audit reaches 5841 cells and all side rooms. The gurney at (6.55,12.25) narrows the aisle so the blackout trigger box (5.2-7.6 x 10.2-11.8) cannot be bypassed on the way out.

Audit: PASS: 0 placement problems. WARN 172 props exceeds 120 per chunk. INFO 2 cells reachable only through glass. 31 lamps (budget is about 30), 1 sign plate. check PASS.

Faults:

- **minor**, whole map: 172 loose props against the 120 budget and 31 lamp records against about 30.
- **minor**, audit INFO, location not printed: 2 cells are reachable only through glass (and in locked state); a glazing proxy leaks somewhere, likely a band-office or hold-office pane. Not a FAIL; not located.
- **minor**, plant room cylinders (22.75, 20.3-21.5) and sluice shelf (16.1,3.6): 41 crawl-only perch cells: the player can get on top of the gas cylinders and shelf under the ceiling (a spot view rendered from there).

Missing for complete:

- Play-verify the full chain once: turn, rise, hush, blackout, bay 7 wake, breaker restore, radio (only the lit and blackout lighting states were rendered)
- Trim props toward budget
- Find and close the 2-cell glass leak

### 73 MEDICAL / IMAGING

makeMedicalImaging: a 4 m transfer corridor between Screening and Ward C. West side: two X-ray rooms on a shared lead-glass control booth, a darkroom behind a light trap (red safelight), and the whole-body counter suite with a steel chamber. East side: reading room with a 15-viewer bank, booking, reception open to the Pharmacy seam, film library and radiographers' room. A real, specific place, but the weakest of the four: its headline image is dim and it is far over prop budget.

Route: N door 10-14 (no state this side; 71's side holds med_override), S seam 10-14 to Ward C (75), E seam 9-14 to Pharmacy (74). All match the plan. Audit reaches all 13 border cells and 6112 cells. Spawn (12,2.2) is at the N door.

Audit: PASS: 0 placement problems. WARN 379 props exceeds 120 per chunk. 27 lamps, 2 sign plates. check PASS.

Faults:

- **major**, film library shelves, x 14.47-18.5, y 15.25-17.4: 379 loose props against a 120 budget, more than three times over. Most are individual office_file_binder props (10 shelves x 3 boards) spawned as clutter.
- **minor**, reading-room viewer bank (15.25-22.25, y 1.09); X-ray 1 (4.6-5.1,1.05); X-ray 2 (5.2-6.2,14.95): med_xray_viewer_* are not self-lit. From the doorway the bank reads as a row of dark picture frames; the crystalline skulls only read within about 1.5 m (a73_7reading.png against b1_73viewers).
- **minor**, viewer index 13 above the LAB 4 note at (21.75,1.0): The note says 'SKULL AP: CLEAR' but the viewer placed there is med_xray_viewer_chest.
- **minor**, X-ray 1 table/tube stand/chest stand, X-ray 2 fluoroscopy tower, darkroom processor and benches, counter chamber: Built from wall primitives in pale-steel material rather than pack models. They read as the right equipment but boxy and dark; the hanging films are flat black rectangles.
- **minor**, reception (14.2-23 x 9-14) and the reading room's east half: Sparse: two benches, a cooler, a plant and a wheelchair in a 9 x 5 m hall; a long bare wall east of the reading desks.
- **minor**, library shelf tops (first at 17.6,14.6), fluoroscopy tower (5.45,11.62), wet-bench shelf (1.0-1.26, 2.6-5.4), X-ray 1 collimator (6.25,3.05): About 130 crawl-only perch cells; the player can climb onto equipment under the ceiling.

Missing for complete:

- Bring props to budget: bake the library binders into fixtures or merged rows
- Make the lightboxes read as lit (emissive face or a tight lamp per group) and put a clear skull film over LAB 4's note
- Dress reception and the reading room's east wall
- Add perch proxies on the tall equipment
- Play-verify the subject-nine wake and radio

### 74 MEDICAL / PHARMACY

makeMedicalPharmacy: a dead end off Imaging's reception using the north 60% of the chunk (rows 14-23 and the east strip are solid by design). Collection hall with two glazed hatches, dispensary with bench, trolley bays and fridges, a controlled-drugs cage on a time lock, a sealed walk-in cold room, the night pharmacist's office, goods-in store, and a porters' corridor ending at a dead service lift.

Route: W seam 9-14 only (matches plan). Hall, then porters' corridor east, under the shutter jammed 0.78 m open at (16-19, y 10.9), which needs prone (0.58 m hull; kneel is 1.05 m), then goods in, dispensary, cage; the staff door (7.6-8.5, y 8.9) releases from inside via medf_staff_release. Audit reaches 2955 cells including the cage and goods in. Not verified whether the audit took the shutter crawl or the staff door to get there.

Audit: PASS: 0 placement problems. No WARN (120 props, 19 lamps, 0 sign plates). check PASS.

Faults:

- **minor**, cage gate (3.6-4.6, y 3.5): Renders as a rusted, blood-streaked bulkhead leaf with the prompt 'BULKHEAD / LOCKED' in a clean dispensary (b2_74gate). It does not read as a grille gate or tell the player it is on a time lock. The staff door (7.6-8.5, y 8.9) is built with the same m74Door helper and probably looks the same; that one was not rendered.
- **minor**, jammed shutter seen from the corridor (16-19, y 10.9): The only way forward is the darkest thing in the corridor: the tube at (15.2,12.5) is 0.3 with 0.5 flicker and the next one at 19 is dead. The gap and the spill beyond are visible, but nothing cues going prone (a74_4shutter). Not verified in play.
- **minor**, shutter leaf: Reads as a dark panelled wall rather than a roller shutter; slat joints are barely visible in either direction (b3_74shutterin).

Missing for complete:

- Give the cage gate a grille leaf and a time-lock prompt; check the staff door's leaf and prompt
- Put a low light or prop cue at the shutter gap so the crawl reads
- Play-verify the 20 s lock sequence, the slam, and the staff-door release from inside

## Depthworks Medical, deep wing (75-78)

This is the most finished stretch I could find fault with: all four maps are real, densely dressed places with a working, engine-tested blackout, transfer and release chain, clean audits and checks, and edges that match the plan; none is a block-out. It is not safe to ship as it stands, because Ward C South writes its checkpoint in the wrong order: dying in the dark ward and reloading leaves the blackout unable to start and the route to Surgery and the Morgue locked. A second softlock comes from the maglocked doors, which a player can shut in the dark and never reopen once power is back. Both are small code fixes found by reading, not by running; after them the remaining work is cosmetic, chiefly the stacked-box 'sheets' on the Morgue's corpses.

**Top fixes**

- 76, MedicalWard.cpp:569: move Checkpoint to the end of the medw_blackout action list (or make the event non-once, since it is already gated on medw_dark_done==0). As written the autosave latches the event with its states unset and a reload kills the blackout and the main route.
- 76, MedicalWard.cpp:500 and 502 (and 492): lock the recovery, mortuary and OBS 5 doors on a latched state such as medw_fail_done instead of med_blackout, so a door shut in the dark cannot lock for good after the transfer in 77.
- CampaignMapTest.cpp, after line 431: add two cases, a save and reload at the OBS 6 checkpoint, and 'recovery door shut in the dark, then power restored', so both softlocks stay fixed.
- 78 and 77, MedicalDeep.cpp:27-40 (ddCorpse): replace the five-box stepped 'sheet' with one low draped slab or a sheet model; it reads as stacked bricks on six corpses in the Morgue and on subject 31 in Surgery's recovery.
- 76, MedicalWard.cpp:553: shrink the four 'finger' smears in OBS 6 (now 0.18 x 0.4 m each, reading as vent slots) to finger size or remove them.
- 77 MedicalDeep.cpp:471-472 and 76 MedicalWard.cpp:691-692: the sluice and linen 'bags' are plain dark cubes in dim corners; swap for pack props or shape them, and raise those two lamps a little.
- 78 MedicalDeep.cpp:758 and 75 MedicalWard.cpp:323: the washroom stalls read as flat navy slabs; add a door seam and handle or light their fronts.
- Cap the perches the audit lists as INFO: bed-head shelves in 75/76, shelf and cabinet tops and the scrub doorway head in 77 (8.4,4.4), the scale pan and counters in 78. Also correct .build/agents/briefs/MedicalDeep.txt, which is a copy of the MedicalWard brief with the wrong key and state prefix.

| Chunk | Map | Rating | Effort | Beats |
|---|---|---|---|---|
| 75 | MEDICAL / WARD C NORTH | complete | small | No gate of its own; this is the teaching half of the blackout set piece. Doors: seclusion 1 (x4.4-5.3 y10.5) and seclusion 2 (y12.5) locked until med_blackout; booth and drug room free. Three dormant Transformed: seclusion 1 at 3.45,9.65 (wakeState medw_wake_n, set by event medw_n_dark_walk when the player is in the aisle with med_blackout=1), seclusion 2 at 2.7,14.85 and bay E2 at 15.6,15.47 (bot |
| 76 | MEDICAL / WARD C SOUTH | dressed | small | The blackout. Event medw_blackout (box 6.1-9.7 x 20.5-22.9, requires medw_dark_done==0) does Checkpoint, sets medw_dark_done and medw_fail; sequence medw_fail -> medw_fail_done at 2.6 s; event medw_blackout_fall then plays the power-down, shakes, sets med_blackout=1 and medw_dark_clock, and opens OBS 5 (door 3), recovery (door 4) and mortuary (door 6), all three locked on med_blackout. Four Transf |
| 77 | MEDICAL / SURGERY | complete | small | Two-step gate, no enemies (the safe lit island). Entering the office in the dark fires medd_transfer_arm (sets medd_ward_transfer, shuts strongroom door 0, which requires med_blackout==0). Terminal WARD C / SUPPLY TRANSFER at 18.25,7.78 (requires med_blackout) sets medd_transfer_thrown; event medd_transfer_do then clears med_blackout and sets medd_power_back. The strongroom opens; terminal MORTUAR |
| 78 | MEDICAL / MORGUE | dressed | small | One Mutant (subject 40) in the island bay at 5.55,9.2, wakeState medd_morgue_dark. Lit visit: a knocking loop (medd_knock) in the east aisle, then walking the west aisle at 3-5 x 11.4-13.6 fires medd_store_pop: the store tubes blow in sequence, a jamming dark zone covers 1-9 x 1-14.8, drawer 9's red lamp comes on and he gets up behind the player. Visit during the blackout: the whole map is dark bu |

### 75 MEDICAL / WARD C NORTH

makeMedicalWardCNorth: the north half of the tier-3 observation ward. Imaging corridor in, admissions (property lockers, counter, scale, gowns) west, the clerk's door-control booth with a CCTV shelf east, then ten curtained bed bays on a 10 m aisle with bed-head shelves, monitors, drips and straps. Off the aisle: two padded seclusion cells with an observation ante, the patients' bathroom, a glazed nurses' station, drug room and charge nurse's office. Six spot views all read as a specific, lit place; admissions, booth, drug room and bathroom interior were not seen in images (no sweep spent here).

Route: N seam x10-14 @-12 -> held-open ward doors at y5.8-6 (x10.8-13.2) -> aisle south -> S seam. The plan seam is x2-22 but the real opening is x7-17; both 75 and 76 wall the rest consistently. Audit: 5672 reachable cells, everything required reachable.

Audit: map 75: PASS: 0 placement problems. Check PASS. Seven INFO PERCH notes: bed-head shelves at z -10.6 are crawl-reachable.

Faults:

- **minor**, Bathroom, med_washroom_stall at 1.87,18.8 and 1.87,21.0 (MedicalWard.cpp:323): Up close the stall is a featureless dark-navy slab; spot view #1 (1.4,22.4) is a single flat colour. The pack texture is near-flat navy and the south face is in shadow, so it is not a missing texture, but it reads as untextured.
- **minor**, Charge nurse's office, office_file_cabinet_open at 19.7,18.15 (line 411): Reads as a blank white box with a paper on top in spot view #2.
- **minor**, Seclusion doors 1 and 2 (lines 225, 227): They only unlock on med_blackout; nothing in 75 swings them open, so the 'maglocks fail open' fiction is not enacted here. Whether a Transformed can push a shut swing door was not verified; if not, both seclusion patients stay in their cells.
- **minor**, Bed-head shelves along x7 and x17 (mwShelf): Seven crawl-only perches at 1.4 m above the beds: possible safe spots above the patients.
- **minor**, Whole chunk in the dark: 75 is off the critical path after the blackout (the route from OBS 6 runs east and south), so its three patients are only met by a player who walks north. By design, but the chunk's own encounter is optional.

Missing for complete:

- Nothing structural. Polish only: give the bathroom stalls a readable front, replace the white box cabinet, and decide whether the seclusion doors should swing open on blackout.
- A sweep of 75 to confirm the four rooms not covered by spot views.

### 76 MEDICAL / WARD C SOUTH

makeMedicalWardCSouth: the ward's far half and the set piece of the district. Seven more bays, the night desk with the dead Cleanup guard and 'bed 12' standing over him, OBS 5 (padded, maglocked on its patient), OBS 6 (LAB 4's room: cut straps, cut amber band, chart, bare footprints out to the mortuary corridor), treatment, sluice, linen, the service corridor from Lab 4, recovery, the theatre transfer bay behind wired glass, and a staff room. Spot views and the sweep read as a real, specific, well-lit place. It is held back from complete by a save-order bug that kills the main route.

Route: W seam y15-19 (from Lab 4, chunk 83) or N seam x2-22 (from 75) -> ward end -> OBS 6 at 6-9.8,19.2-23 (blackout) -> north up the aisle -> recovery doorway at x17, y7.75-8.75 -> recovery door (x20-21.2, y8.9) -> transfer bay -> E seam y9-14 to Surgery; later, ward end -> mortuary door (x11.4-12.6, y19.1) -> S seam x10-14 to the Morgue. Audit: 6378 reachable cells. The engine test confirms the bay and mortuary corridor are unreachable while lit and reachable once unlocked.

Audit: map 76: PASS: 0 placement problems. Check PASS. Nine INFO PERCH notes on bed-head shelves. Engine test lines for the blackout timing, wake states and door release all PASS.

Faults:

- **blocker**, MedicalWard.cpp:569, event medw_blackout: The action list is {Checkpoint, set medw_dark_done, set medw_fail}. Game::updateScripts (Script.cpp:170-171) latches a once-event in m_firedEvents before running its actions, and saveCheckpoint (Saves.cpp:318) encodes the save immediately, including m_firedEvents (Saves.cpp:88). So the autosave holds the event as fired with medw_dark_done=0 and medw_fail=0. Dying in the dark ward and loading that checkpoint gives a lit ward where the blackout can never start, and the recovery and mortuary maglocks never release: the route to Surgery and the Morgue is dead. The window closes only when the player reaches 77 or 78 (chunk-entry autosave). Found by reading the code; not run, and CampaignMapTest has no reload case.
- **major**, Recovery door (line 500), mortuary door (line 502), OBS 5 door (line 492): They require med_blackout==1, and a locked door cannot be used (Movement.cpp:320). In the dark the player can shut them (E / CLOSE SERVICE DOOR). Shut the recovery door behind you against the patients, throw the transfer in 77, and it is locked for good: the bay, staff room and Surgery have no other exit. Shut the mortuary door in the dark and the Morgue is unreachable after power returns. The engine test only covers doors left open. Code reading, not run.
- **minor**, OBS 6 east wall, x9.79, y20.0-20.93, z1.1-1.5 (line 553): The 'four fingers' smears are four strips 0.18 m wide by 0.4 m tall; in the sweep they read as four dark slots or a vent. The comment calls the wall glass but it is solid.
- **minor**, Linen room, bags at 3.95-5.45,19.75-20.95 (lines 691-692): Plain boxes in mat 23 under a 0.3 lamp; spot view #1 shows near-black cubes in a dark corner.
- **minor**, Maglocked clinic doors: The locked hint is the generic 'BULKHEAD / LOCKED' on a swinging ward door.
- **minor**, OBS 6 trigger: The power goes 2.6 s after stepping in whether or not the chart is read; whether an open readable pauses the timer was not verified.

Missing for complete:

- Fix the checkpoint: put Checkpoint last in the medw_blackout action list (as CampaignChapterDeepResearch.cpp:773 does) or make the event non-once so its medw_dark_done==0 gate re-arms it after a reload; then prove it with a save and reload at OBS 6.
- Key the recovery and mortuary doors on a latched state (for example medw_fail_done) instead of med_blackout, so a door shut in the dark cannot lock behind the player.
- Resize or drop the finger smears in OBS 6; shape or replace the linen bags.

### 77 MEDICAL / SURGERY

makeMedicalSurgery: Theatre 1 and the rooms that ran it. Transfer bay at the red line, theatre corridor with bumper rails, staff changing and staff room, scrub and lay-up, the white-tiled theatre (built operating table with cut straps and blood, two surgical lamps on arms under a laminar canopy, anaesthetic machine, x-ray viewers, swab board), the director's office behind observation glass (executive desk, rug, bookcase, transfer panel), his records strongroom, recovery with subject 31, and the disposal corridor to sluice, specimens and the porters' hold. Spot views and sweep frames show a convincing place with the theatre as a bright beacon.

Route: W seam y9-14 @-12 only (dead end, out and back): transfer bay -> theatre corridor -> theatre through pinned-back double doors at x8.6, y10.4-12.6 -> door 2 (x14-15.2, y15.7) -> disposal corridor -> office door 1 (x17.4-18.4, y15.7) -> strongroom door 0 (x21.4-22.4, y7.3) -> back west. Audit: 6244 reachable cells, all required controls reachable.

Audit: map 77: PASS: 0 placement problems. Check PASS. Engine test '77 flow' passes every step: refused while lit, armed in the dark, restored, release works.

Faults:

- **minor**, Recovery, subject 31 on the bed at 2.0,15.6 (ddCorpse, MedicalDeep.cpp:27-40): The sheet over the feet is a three-tier stack of boxes; same fault as the Morgue, one instance here in a side room.
- **minor**, Sluice, bags at 10.9-11.5,18.7-19.6 (lines 471-472): Four plain black cubes (mat 8) on the floor; spot view #3 reads as block-out primitives in a dim corner.
- **minor**, Shelf and cabinet tops; scrub doorway head: The audit reaches standing spots at z -10.2 to -10.45 on 1.8 m shelves (1.4,2.4; 16.1,1.4; 22.4,22.4) and the strongroom control cabinet, and crawl cells on the scrub doorway head at 8.4,4.4 (z -9.7) and on the pinned theatre door leaf. Harmless with no enemies here, but untidy.
- **minor**, State naming: medd_ward_transfer is set when the transfer is armed, not when it is thrown. It works only because the strongroom door also needs med_blackout==0. Confusing for the next author.
- **minor**, Sluice, specimens, porters' hold, strongroom: Dim (0.35-0.45 lamps); the porters' hold is the sparsest room in the chunk. The pinned theatre door's vision panel is a flat blue rectangle.

Missing for complete:

- Nothing blocking. Replace the stepped sheet on subject 31 and the black cube bags; cap the shelf and door-head perches.
- The per-file brief MedicalDeep.txt is a copy of MedicalWard.txt (key MedicalWard, prefix medw_) while this file uses medd_; correct it before anyone else edits here.

### 78 MEDICAL / MORGUE

makeMedicalMorgue: Mortuary 2, a floor below the ward. Concrete stair hall with the dead body lift, receiving (scale, trolleys, book-in window), the body store (two drawer walls and an island, drawer 9 open from the inside, a pulled bay, hoist rail), post-mortem room with three pack autopsy tables, the quarantine lock with red/green interlock lamps and the dead night porter at the door, plus office, plant room, night porter's room and suit-up room. The store aisle, office and receiving look good. It is held at dressed by the stepped box 'sheets' on every corpse, which dominate the post-mortem room.

Route: N seam x10-14 @-12 onto the landing deck -> 16-step stair (x10-12, y3-7.5) down to -15 -> receiving -> doorway x9, y9.4-11 -> body store, round the island and down the west drawer aisle -> door 1 (x3.4-4.6, y14.9) -> post-mortem -> doorway x9.6, y19.4-20.8 -> quarantine lock -> S door x10-14 @-15 on med_morgue_release. Audit: 5074 reachable cells; edges match the plan.

Audit: map 78: PASS: 0 placement problems. Check PASS. Engine test lines for the store pop, subject 40 waking and leaving his bay, relight and the release all PASS.

Faults:

- **major**, ddCorpse (MedicalDeep.cpp:27-40): three autopsy tables at 5.0, y16.9/19.3/21.7, plus the trolleys at 13.9,1.95, 8.1,13.4 and 13.4,13.85: The 'sheet pulled over shins and feet' is five stacked boxes in mat 39. In the lit spot view of the post-mortem room each corpse carries a three-tier ziggurat of pale blocks on its legs; it reads as stacked bricks, not cloth, and it is the first thing seen on entering the room. Six instances in this map.
- **minor**, Suit-up room, med_washroom_stall at 18.4 and 20.6, y22.23 (line 758): Both stalls face the room as one flat navy wall with no visible door detail; spot view #2 and sweep frames at 19.4,21.1 are a single flat colour.
- **minor**, Body store after the pop: medd_morgue_dark is only cleared once, by medd_morgue_relight on medd_power_back. A player who triggers the pop after the transfer but before fetching the release must cross a permanently black, torch-jamming store both ways. Harsh; may be intended.
- **minor**, Plant room 16-23 x 1-6.6: The dimmest and least dressed room (two pumps, a fan unit, a cabinet); mostly bare wall in the sweep.
- **minor**, Counter and fixture tops: The audit reaches standing spots on the post-mortem counter (z -14.1), the hanging scale pan at 9.4,18.4 (z -13.67) and the scrub sink at 1.4,20.1 (z -13.87).

Missing for complete:

- Replace the stepped box sheet in ddCorpse with one low draped slab or a sheet model, here and on the trolleys.
- Give the suit-up stalls a readable front (door seam, handle, or better light on them).
- Decide whether the store should relight when the player returns after the pop.

## Analysis Laboratories (79-84)

79-82 are complete: dense, specific places with a working gate chain (purge console in 82 sets lab_chem_seal, releasing 81's south door and 84's airlock), four placed creatures, tested doors and hunts, clean audits. 83 and 84, the district's payoff, are dressed but not finished: their geometry, readables and sequences follow the Lab 4 plan of 2026-11-04, but the Pointer creature and its light mechanic were never built (no such CreatureKind in the engine), so the rig chain anl_fm_bus -> anl_l4_rig and the anl_fm_west console switch lamps that nothing reacts to, and both maps carry about half the dressing of their neighbours. 83 still plays as a route (two guards, the disk, the service door, her drafts); 84 has no gameplay beyond reading. The most valuable work is the Pointer decision (build it or cut it and rewrite), then dressing 83's hall, corridor and Array Power and 84's north band, airlock and Chamber 3, then tests for both maps.

**Top fixes**

- Decide the Pointer. Build it per lab4-field-measurement-plan-20261104.md sections 8-9 (CreatureKind::Pointer, light-seeking AI, bake from the purchased Nightmare-Creature pack, save version bump) and spawn R-47 at (18.4,12.4) in 83 and R-46 dormant at (4.9,20.3) in 84; or cut it and rewrite the rig terminal (83 at 17,5.55), the west-walk console (84 at 13.4,5.55), the tape and the notes so nothing is inert or promised.
- 84: dress the kit room, techs' office (east end empty), the 22 m gallery, the airlock (18-23, 9-14) and Chamber 3 with pack props, and give each room its own light instead of one flat tube.
- 83: dress the array hall's west and east thirds, Array Power (1-9.8, 15-23) and the service corridor to Ward C (14.2-23, 15-19), which is currently an empty panelled box.
- Add campaign tests for 83 and 84: disk pickup sets med_override and releases the service door; south door holds on res_lab_unsealed; 84's airlock holds on lab_chem_seal; anl_fm_bus enables the Lab 4 rig terminal.
- Give the purge a payoff: a ScriptEvent or TimedSequence on lab_chem_seal in 82 and 81 (fans spin up, PA, the door releasing) so the district's one puzzle does not resolve silently.
- 81: reconcile the N edge with the plan (CampaignChapterAnalysisLabs.cpp:93 builds a door, plan says seam) and bring props and lamps toward budget (81: 235 props, 42 lamps; 82: 162 props, 35 lamps).
- Replace placeholder cue audio in 83 and 84 (Sound::Metal1 for Dunne's tape, RadioSquelch for the Investigator, Sound::Exit for the drafts) with the plan's voice lines or a fitting tape/radio bed.
- Visual tidy: water in 80's cold plant and 84's pan reads as a bright swimming pool; 80's stair head shows black sawtooth sides; 83's pen roof is an opaque slab; 82's PPE rack is a black silhouette; 81's flammables cabinet reads as wood.

| Chunk | Map | Rating | Effort | Beats |
|---|---|---|---|---|
| 79 | MATERIALS INTAKE | complete | none | No gate and no gating state (only anl_intake_clock and two hum timers, anl_crate4_hum and anl_cage_hum). Encounter: 1 Mutant at (21.5,15.8), awake at spawn, outside the cage. Story: terminal INTAKE / MANIFEST at (1.25,6.4); note-terminal NIGHT PORTER / RECEIVING at (18.9,2.75); taped notes RECEIVING ORDER, MANIFEST 0211-49, CRATE 2, CRATE 5. Set pieces: crate 4 warm on the hoist hook at (8.05,11.1 |
| 80 | SAMPLE STORAGE | complete | small | No gate, no gating state. Encounter: 1 Warden at (21.3,20.4) in the -80 room, awake at spawn. Hazard: Anomaly meltwater (3.8-9.4, 16.4-21.6) at 3 dps with anl_cold_hum. Story: terminal COLD STORE / CONTROL at (22.75,8); taped notes FREEZER 6 / ALARM LOG, FREEZER 9 / RACK CARD (LAB 4's draws), RELEASED TO I.I. / 03:05, -80 ROOM / DOOR, COLD PLANT / DO NOT MOP. Set piece: freezer 6 at (14,22.55), un |
| 81 | ANALYTICAL CHEMISTRY | complete | small | Gate: S door (door 1) requires lab_chem_seal, set by the purge console in 82; six lbBeacon lamps blink until it is set; terminal CHEMICAL LOCKDOWN / SOUTH DOOR at (9.45,21.6) sends the player west. Encounter: 1 Transformed (Moran) at (5,20) in the solvent store. Story: terminal ASSAY / R-41 IN SOLUTION at (16.8,5.5); notes SUIT COUNT / 02:30, HOOD 3 / SASH DOWN, HOOD 1 / CELL 0447 RESIDUE (cells h |
| 82 | INSTRUMENTATION | complete | small | Puzzle control: terminal FUME PURGE / ANALYTICAL CHEMISTRY at (18,7) sets lab_chem_seal (opens 81's S door and 84's airlock). Encounter: 1 Transformed (night tech) at (13,9) on the floor. Set pieces: stand 3 tripped with loose lead, soot and red emergency lamp; discharge rig strobe (1.7 s period); reject cage with a green-glowing cell. Notes: STAND LOG / NIGHT, DISCHARGE RIG / STROBE, REJECTS / DO |
| 83 | ENERGY DIAGNOSTICS / LAB 4 | dressed | large | Gates: service door requires med_override, set by the paper-terminal WARD C OVERRIDE / R. HALE at (20.5,14.4); south door requires res_lab_unsealed (set in 85). Encounter: 2 Cleanup guards, squad anl_diag_detail (Assault starting (4,3.4); Sharpshooter patrolling the hall, drops sidearm). Hazard: Anomaly pool (11.8-14.8, 11.8-14.8) at 3 dps. Sequences: anl_diag_radio on entering the vestibule (7 ca |
| 84 | FIELD MEASUREMENT | dressed | medium | Gate: airlock door requires lab_chem_seal (from 82). Controls: FIELD LAMP BUS / CHAMBER 3 PANEL at (22.3,21) sets anl_fm_bus (feeds 83's rig); FIELD LAMPS / WEST WALK at (13.4,5.55) toggles anl_fm_west (four stakes at x 3.2). Set piece: terminal FIELD RECORDER / TAPE 46-11 at Dunne's desk (9.5,7.6) runs anl_fm_tape, nine captioned cues, ending in anl_fm_desk (hand lamp stutter). Hazard: Anomaly po |

### 79 MATERIALS INTAKE

makeMaterialsIntake (CampaignChapterAnalysis.cpp:81). A high-bay dock hall for specimen crates from Deep Extraction: tiled receiving lobby with decon arch, glazed manifest booth (NW), porters' office (NE), gantry hoist with crate 4 left on the hook, booking bay, racking and forklift (SW), quarantine cage (SE) with crate 6 opened from inside, teal passage south. Reads as a specific working place in every view (130 fixtures, 60 props, mercury/sodium pendants, one dead).

Route: N door 10-14 -> lobby -> dock hall -> W seam 9-14 to Sample Storage, S seam 10-14 to Chemistry. Audit PASS: all three borders open, 5038 reachable cells, nothing required is unreachable.

Audit: PASS: 0 placement problems. check PASS. No WARN.

Faults:

- **minor**, Mutant spawn (21.5,15.8), CampaignChapterAnalysis.cpp:292: Awake from load (wakeState 0) with no trigger or reveal; the cage story (porter went to look at 02:31) has no staged moment. Behaviour in play not verified (shots hide creatures).
- **minor**, S edge 10-14 / seam with 81: This side is a seam as planned, but 81 opens its matching N edge as a door (see 81). Visual result at the seam not verified in play.

### 80 SAMPLE STORAGE

makeSampleStorage (CampaignChapterAnalysis.cpp:342). Two-level cold store: a control room on a deck at -12 with the freezer mimic board, a stair down west to -15, then freezer hall with sample bench, glazed specimen room (I.I.'s stripped cabinet), cold plant with humming meltwater, dry aisle with freezer 6 still running, and the -80 room. Cold blue failing tubes, red plant beacons, warm freezer-6 glow; reads as a real place.

Route: Single edge: E seam 9-14 at -12 -> control room -> stair (12.5-17, 9-11.5) -> loop through freezer hall, cold plant, dry aisle -> back out the same way. Audit PASS: 5718 reachable cells on both levels.

Audit: PASS: 0 placement problems. check PASS. No WARN.

Faults:

- **minor**, whole chunk (dead-end spoke): Nothing here is required: no state, no key item, only lore and 4 pickups, while a 320-HP Warden is loose in it. A player has no reason to come down and no reward for surviving it.
- **minor**, cold plant pool (3.6-9.6, 16.2-21.8), sv_80_1: The meltwater renders as a bright cyan, rippling 6 x 5.6 m pool with white highlights; it reads as a swimming pool, not a skin of water on a plant-room floor.
- **minor**, stair head at (17,9) and (17,11.5), sv_80_0: Both sides of the flight show as black sawtooth silhouettes against the lit well.
- **minor**, -80 room door (15.66-16.76, 19.8): Brief and plan ask for a vault door bulged outward; built as a door swung open. Warden has no wake trigger for the 'back shelf has moved' note.
- **minor**, lights: 33 lamps against the roughly 30 budget.

### 81 ANALYTICAL CHEMISTRY

makeAnalyticalChemistry (CampaignChapterAnalysisLabs.cpp:86). Wet lab under chemical lockdown: vestibule with suit room (W) and tiled decon stalls (E), the main lab with four bench islands and reagent spines, three fume hoods on the east wall (hood 3 violet, sash down), instrument wall, block-built solvent and spill stores, and the lockdown lobby to the held south door. Dense and specific (82 fixtures, 235 props), red lockdown beacons as the only colour.

Route: N edge 10-14 -> vestibule -> lab -> W seam 9-14 to Instrumentation (purge) -> back -> lockdown lobby -> S door 10-14 to Lab 4. Audit PASS: 5991 reachable cells, all borders open.

Audit: PASS: 0 placement problems, with WARN 235 props exceeds 120 per chunk. check PASS.

Faults:

- **minor**, N edge, CampaignChapterAnalysisLabs.cpp:93: openEdge(m,{'N',10.f,14.f,f,true,0}) builds a door; plan.py and the design table list 81 N as a seam (79 S is a seam). Edge kind does not match the plan contract. Campaign test passes; effect on streaming and the view from 79 not verified.
- **minor**, whole chunk: 235 loose props (audit WARN, budget 120) and 42 lamps (budget about 30).
- **minor**, lab, hood 3 hazard at CampaignChapterAnalysisLabs.cpp:265: The brief's toxic hazard pockets are absent: the only Hazard has damagePerSecond=0 behind the sash. The foul air is haze only, so the lockdown is a locked door plus one console press and the suits and decon are scenery.
- **minor**, solvent store (8.36-8.94, 17.9-19.1), sv_81_1: The flammables cabinet (structure material 18) reads as a brown wooden wardrobe, not a yellow steel cabinet.

### 82 INSTRUMENTATION

makeInstrumentation (CampaignChapterAnalysisLabs.cpp:384). Cell proving floor: four test stands along the north wall with live test leads, the discharge rig and strobe on a pad mid-floor, the reject cage (W), a glazed control room (E) with the purge board, cell store (NE), back corridor, locker room (SE), south corridor on to Field Measurement. Reads as a real lab floor (92 fixtures, 162 props).

Route: E seam 9-14 -> control room -> S door to back corridor -> doorway at (17, 15.2-16.6) -> test floor -> S seam 6-10 to Field Measurement. Audit PASS: 5427 reachable cells, both borders open.

Audit: PASS: 0 placement problems, with WARN 162 props exceeds 120 per chunk; INFO PERCH 4 crawl cells on the camera shelf (22.5,6.7)-(22.9,8.3) and 18 near (1.38,15.38); INFO 20 cells reachable only through glass. check PASS.

Faults:

- **minor**, purge console (18,7) and 81: The district's one puzzle resolves silently: no ScriptEvent or TimedSequence is keyed on lab_chem_seal in either builder (no fan spin-up, PA or shake); only 81's beacons switch off, a room away.
- **minor**, whole chunk: 162 loose props (audit WARN, budget 120); 35 lamps.
- **minor**, south corridor PPE rack (9.72,18.9), sv_82_2: The metal shelf renders as a near-black silhouette; hard hats and dosimeters on it barely read.
- **minor**, control room camera shelf (22.5,6.7)-(22.9,8.3) z -10.3: Audit reports it as a perch a crouched player can get onto.

### 83 ENERGY DIAGNOSTICS / LAB 4

makeEnergyDiagnosticsLab4 (CampaignChapterAnalysis.cpp:615). The story beat: vestibule, packing room (NW), analysts' office (NE), a dark array hall with two rows of switch cabinets and a humming pool, Lab 4 behind protective glass (her lit office, drive rack, cut-cell bench in violet, the pen, Hale's body by the service door), Array Power (SW), service corridor to Ward C, sealed south passage. It is a real place with working gates and story, and by the letter it clears the complete bar; I rate it dressed because its approved centrepiece (the Pointer, R-47) does not exist, the rig control it built for that is inert, and it carries about half the dressing of 79-82 (48 fixtures, 64 props).

Route: N door 10-14 (or W seam 9-14 from 84's airlock) -> vestibule -> array hall -> Lab 4 by its doorway (15, 6-7.2) or the prone-only cable pass (21.4-22.4, y 4.8) -> take the disk -> service door (19.4-20.6, y 14.9) -> corridor -> E seam 15-19 to Ward C. S seam 10-14 behind a door at y 19.1. Audit PASS: 5865 reachable cells, all four borders open (audit opens doors).

Audit: PASS: 0 placement problems; INFO 20 cells reachable only through glass. check PASS. No WARN. No 83-specific check in the campaign test.

Faults:

- **major**, Lab 4 (15.2-22.9, 5-14.8); WorldDefinition.h:34: R-47 is not in the map and cannot be: CreatureKind ends at Transformed, there is no Pointer kind, lightSight AI or light_moving state anywhere in src. The plan approved 2026-11-04 puts it at (18.4,12.4) guarding the disk. As built the disk is simply picked up; the drafts' guttering lamp (anl_l4_gutter) and the bench rig (anl_l4_rig, four stakes at y 10) only switch lamps; the readables warn about a thing that never appears.
- **major**, array hall (1-15, 5-14.8), Array Power (1-9.8, 15-23), service corridor (14.2-23, 15-19); sv_83_1, sv_83_2, sv_83_3: Box rooms: the 14 x 9.8 m hall holds eight cabinets in its middle and bare floor either side; Array Power is three cabinets on one wall; the corridor 'they brought her down' is an empty panelled box with two floor lines, an extinguisher and a red lamp.
- **minor**, radio, drafts and door-radio cues (lines 644-652, 757-769, 819-824): Captions over placeholder sounds (Sound::RadioSquelch, Sound::Exit at pitch 1.4, Sound::Machine); the plan's voice sets (inv_diag_*, inv_l4_*, anl_diag_a_*) do not exist in src.
- **minor**, the pen (15.2-17.9, 12.1-14.8), sweep point 38: Its roof is an opaque black slab (structure at f+2.4, material 8), not bars; from inside the top half of the view is solid black.
- **minor**, Lab 4 east wall, y 10-14.8, sweep point 40: A flat violet-lit panel wall with one notice board; the room's centre is empty floor apart from the four stakes.
- **minor**, CampaignMapTest.cpp: Nothing tests the disk pickup setting med_override, the service door releasing, or the south door holding on res_lab_unsealed; these are verified by reading the source only.

Missing for complete:

- Decide the Pointer: build it (new CreatureKind, light-seeking AI, bake from the purchased Nightmare-Creature pack, save version) and spawn R-47, or cut it and rewrite the rig terminal and the readables so no control is inert and no text promises it
- Dress the array hall's west and east thirds, Array Power and the service corridor with pack props and a light idea each
- A route test: disk -> med_override -> service door; south door on res_lab_unsealed; rig prerequisite anl_fm_bus
- Voice or a proper placeholder for the three sequences (LAB 4 stays unvoiced per plan)

### 84 FIELD MEASUREMENT

makeFieldMeasurement (CampaignChapterAnalysis.cpp:862). The calibration pan: a flooded basin under a five-metre roof with a railed steel catwalk ring, a reference stand under a violet lamp, Dunne's body on the walk, lamp stakes up the west edge; a glazed gallery north of it with Dunne's desk and the lamp console; kit room and techs' office off the entry corridor; an amber airlock east; Chamber 3 with the lamp bus. The pan is a strong, specific set piece. The rest is thin, and the map is a classroom for a creature that is not there.

Route: N seam 6-10 from 82 -> corridor (kit room W, techs' office E) -> gallery -> pan door (15.4-16.6, y 8.9, unlocked) -> catwalk round the pan -> Chamber 3 door (19.6-20.8, y 15.5) -> bus panel; airlock door (19.4-20.6, y 8.9) -> E seam 9-14 into 83's hall. Audit PASS: 6674 reachable cells, both borders open.

Audit: PASS: 0 placement problems. check PASS. No WARN. No 84-specific check in the campaign test.

Faults:

- **major**, the pan and gallery; plan section 3 and 10: R-46 is absent (plan: Pointer dormant on Dunne at (4.9,20.3), waking on anl_fm_wake) and the engine has no such creature. With zero enemies and the only hazard fully bypassed by the railed catwalk, play here is reading, one tape and one switch; the west-walk console moves lamps that nothing follows.
- **major**, kit room (1-5.8, 1-4.8), techs' office (10.2-22.9, 1-4.8), gallery (1-22.9, 5-8.8), airlock (18-23, 9-14), Chamber 3 (18-22.9, 15.6-22.9); sw84_a.png, sw84_b.png: Bare raw-concrete boxes under one flat tube each: the office's east end (x > 21) is dark and empty, the 22 m gallery has one desk and one console, the airlock is an empty amber box with floor lines, Chamber 3 is near-black with a cart, a bench and two switches. 49 fixtures and 37 props in the whole chunk.
- **minor**, pan water (5.6-14.4, 11.6-20.6), sv_84_2 and sv_84_5: Renders bright cyan with white highlights like a lit swimming pool; the builder's own notes call for black water that gives no light back.
- **minor**, catwalk guard rails, top z -10.28: Audit counts 684 standable cells on the rail tops and spot view 4 was taken standing on the rail at (14.9,13.4). Whether a player can actually get up there and walk the rails over the pool was not verified.
- **minor**, tape cues (lines 925-934): Dunne's lines play as captions over Sound::Metal1, a metal clank per line; no voice or tape sound.

Missing for complete:

- R-46 and the light mechanic (same engine work as 83), or a rewrite of the consoles, tape and notes if the Pointer is cut
- Dress the kit room, techs' office, gallery ends, airlock and Chamber 3 to the 79-82 standard
- A test for the airlock gate and the bus -> rig chain
- Tape audio

## Energy Test Complex (85-93)

This district is not a block-out, whatever the two file headers still say: all nine maps are dressed with pack props and deliberate light, the edges match the plan, every audit passes, and the objective chain works on paper (90 truce and hint -> ring -> 91 -> 92 -> 93 cont_power, with 85's res_lab_unsealed loop to Lab 4). Two maps are complete (85, 90). Three are one small fix away: 91's dosimetry post can only be entered crouched because solid soot slabs stand in its doorway, 93's column hazard sits inside sealed cages, and 92's hazards are all off the route. The real gap is the Observation Ring as a place to play: its galleries are empty rooms in all four chunks, 87 and 88 have no beat, and 89 is a dark dead end without the pit descent the brief asks for, so the chapter's signature space is look-only. The most valuable work is the three small fixes first, then one decision on the pit (build the descent in 89 or cut it and dress the galleries).

**Top fixes**

- 91 makeShieldingCorridor: clear the dosimetry post doorway (swing door 16.4-17.6, y 8.95). Mark every soot slab non-solid and move figure 2 (x=16.5) and figure 3's west-thrown bands off x 16.3-17.7; re-audit until the post is no longer 'c' in map-audit-91.txt.
- 93 makeConversionLab: make live columns 2 and 4 dangerous on the lane (extend each etArc box about 1 m west of the cage, or open the cage's lane side), and add a ScriptEvent on cont_power (cue, sound, shake, checkpoint) so the chapter's key switch lands.
- 89 makeObservationRingSouthWest: decide the pit. Either build the descent to -28 the lead notes ask for (route down, hazards on the pit clock, reward at the cradle, way back) or cut it in the brief; in both cases relight the crew room (1-7.8, 16.2-23) and add a readable for Molloy.
- Ring galleries, rgQuadrant (87-90): the 7 m deep west/north galleries are empty in every chunk. Dress them per quadrant or reduce their depth; this is shared code, so one pass fixes four maps.
- Replace model::transformer (the generic blank box) with pack transformer models in 86 at (2.4,9) and 92 at (5.5,19.8) and (18.5,19.8).
- 86 makeHighEnergyTestBay: light the outward faces of the four capacitor towers (they render black from the perimeter), and give the bay floor a reason to be visited; trim the 47 lights toward the 30 budget.
- 92 makeExperimentalPower: put a hazard or task on the stair-foot-to-exit line (for example the bay 5 fault arcing across the exit); today the route is an 11 m straight walk.
- 87 and 88: add one beat each (the brief's squad-versus-creatures fight seen across the pit fits 88; a control or encounter at pit control fits 87). Light 91's store and 85's two south rooms, and update both file headers that still say 'blocked out'.

| Chunk | Map | Rating | Effort | Beats |
|---|---|---|---|---|
| 85 | RESONANCE LABORATORY | complete | small | Gate: PASSAGE SEAL terminal at (13.6,4.9) sets res_lab_unsealed, opening door 0 (10.5-13.5, y=3) and the matching door in 83 (CampaignChapterAnalysis.cpp:827); red beacon swaps to green on the state. Encounter: 1 Transformed at (19.2,15.7), 'the volunteer'. Anomaly hazard box in the booth (0 dps) plus hum. Readables: SEAL 01:59, SITTINGS PROGRAMME 1981, CELL 0001, SITTING 212, VOLUNTEERS ROTA. Pic |
| 86 | HIGH-ENERGY TEST BAY | dressed | small | No gate, no state. SHOT LOG / BAY 2 terminal at (22.4,4.2), lore only. Bay floor: 2 Transformed at (6.2,15) and (9.6,2.6); timed Electricity boxes round the chamber (6-11, 6-11; 35 dps, 0.8 s of every 3.2 s) and at tower 3 (2.6-4.4, 13.1-14.9); posed LabTech body at (11.6,12.3) with meter and Ammo. Note 'BANKS / DO NOT GO DOWN' teaches the count-three rule. Pickups: Ammo on the south gallery (2.2, |
| 87 | OBSERVATION RING / NORTH-WEST | dressed | medium | No gate, no encounter. RING TEST 31 / LOG terminal at (6.6,4.8), lore. Note PIT CONTROL 02:12 (do not shine anything into the pit). Ammo at (6.5,6.6). Pit hazards (Electricity 40 dps periodic, Anomaly) are below the rim and unreachable. No signs. |
| 88 | OBSERVATION RING / NORTH-EAST | dressed | small | None that plays: no creature, no terminal, no pickup. One letter (VISITORS' GALLERY 1984) and two lectern sign plates. |
| 89 | OBSERVATION RING / SOUTH-WEST | dressed | large | 1 Transformed at (11,19.5) on the walk ('Molloy'). Health at (6.4,17.2). Set pieces: broken bridge (hinge at x 15.9-16.1, y 1.3-3.1), hoist and cage. No terminal, no readable, no state. |
| 90 | OBSERVATION RING / SOUTH-EAST | complete | small | Truce encounter: 3 Cleanup guards (sergeant at (19.5,12), medic at (18.8,20.4), engineer patrolling (8.6,12.1)-(11,4)), squad rg_nine. A chunk-wide ScriptEvent rg_nine_truce sets rg_nine_stance to Truce on entry. Sergeant dialogue rg_nine_hello with branches rg_nine_who / rg_nine_west / rg_nine_power, which tells the player to restore the containment feed (the cont_power objective). Radio exchange |
| 91 | SHIELDING CORRIDOR | dressed | small | Encounter: 2 Cleanup guards, squad et_shield (assault patrolling inside the post (17,6.8)-(20,6); sharpshooter on the corridor and bend). Radio exchange on timer et_shield_radio (box 21-24, 9-14); event et_shield_order then sets et_shield_stance to Hostile. 1 Transformed in the store at (5.5,21). Readables: BULKHEAD 2 / 02:15, I.I. SHIELDING DETAIL order. HandgunAmmo (15.4,8), Health (13.6,22). Th |
| 92 | EXPERIMENTAL POWER | dressed | small | No gate, no state. PLANT BUS / DIRECT FEED terminal on the landing (8.4,2.72), lore. Hazards: Electricity at bay 3 on the west wall (1-3, 11.2-13.8), bay 5 on the south wall (about 15.3-17.1, 21.25-22.2), and the pool (18.4-22.6, 1.4-5.8) with water volume and hum. 1 Transformed at (19.8,10.6). Note BAY 3 / LEAVE IT. Ammo (4.6,6.4), Health (13.6,21.2). 2 signs. |
| 93 | CONVERSION LAB | dressed | small | Gate state: OUTPUT ROUTING terminal at (20,1.75) on the mezzanine sets cont_power, which opens Containment Intake's south door (95) and the door in ContainmentA line 571; the bank's two red beacons die and a green lamp lights on the state. Encounter: 2 Transformed at (14.2,20.2) and (2.2,14.8). Hazards: Electricity in the two live cages (40 dps, 0.45 s of 2.2 s) and along the south bank (3.2-12.2, |

### 85 RESONANCE LABORATORY

makeResonanceLaboratory. The 1981 sittings lab: a lead-glass booth in the middle with R-01 on a plinth under a lopsided resonator (pylon, coil arm, reservoir tank, sheared second arm), recording desk facing the glass, archive NW, barred sample vault NE (cell 0001), waiting room SW, suit room SE, sealed passage north. Tiled, green dado, cold tubes with violet from the booth. Reads as a specific place in every frame rendered.

Route: S 10-14 @-12 (from ring 88) -> corridor -> lab -> N passage 10-14 @-12 (to Lab 4, chunk 83) behind door 0 at y=3; W 9-14 @-12 to the Test Bay (86). Edges match the plan. Audit: 5969 reachable cells, 5920 standing, nothing required unreachable.

Audit: PASS: 0 placement problems

Faults:

- **minor**, Waiting room (1-9.8, 17.1-23) and suit room (14.2-23, 17.1-23); sv_85_1, sv_85_2: Dim and sparse: one 0.45 tube each, furniture reads as near-black silhouettes, walls bare above the dado.
- **minor**, Seal console (13.6,4.9): The taped note warns not to release if the booth lamp goes red, but releasing the seal has no consequence or cue: door and lamp swap only. A missed payoff, not a bug.
- **minor**, Booth interior, sample vault, archive: Not verified from inside: no spot view landed in them. Seen only through glass and doorways.

Missing for complete:

- Nothing required. Optional: a cue or scripted reaction when res_lab_unsealed is set, and more light in the two south rooms.

### 86 HIGH-ENERGY TEST BAY

makeHighEnergyTestBay. A 24 m tall concrete hall, floor at -20, east and south galleries at -12. Capacitor stack from the shot chamber to the roof, four capacitor towers, a hung gantry ring, cross-catwalk at gallery height, control room behind glass on the east gallery, crew lockers and suits on the south gallery, sodium high-bays. The stack and gantry are the best single image in the district.

Route: E 9-14 @-12 (from 85) -> east gallery -> south gallery -> S 10-14 @-12 (to ring 87). Optional: stair (15-17, y 4.96-17, 43 steps) from the south gallery down to the bay floor; control room via door at y=8. Edges match. Audit reaches everything.

Audit: PASS: 0 placement problems (INFO PERCH: 7 gantry structures at z -9.72..-9.60 carry crawl-only reachable cells)

Faults:

- **major**, Capacitor towers at (3.5,3.5), (13.5,3.5), (3.5,14), (13.5,14); sv_86_1, sv_86_3: They render as flat black cylinders from the bay perimeter and the stair foot. Each foot glow sits 1.3 m toward the chamber, so the faces a player sees are unlit.
- **major**, model::transformer at (2.4,9); right edge of sv_86_1: The old generic transformer box: a blank pale slab with a finned top. It is a stand-in where pack transformer models exist (pwr_step_transformer).
- **minor**, Required route E -> S: About 20 m of gallery; the player never has to engage the bay. The floor (creatures, arcs, body) pays one Ammo pickup, and the cross-catwalk (deck 8-10, y 1-17) dead-ends at the north wall with nothing on it.
- **minor**, Above the control-room slab (17-23, 1-8, z -8.6 to +4); sv_86_2: Reads as a pitch-black void beside lit sodium walls.
- **minor**, Whole chunk: 47 lights against the ~30 budget in common.txt (16 on the stack cans).
- **minor**, Gantry ring at z -9.6: Audit reaches 620 cells on it (crawl perches off the catwalk rails): a player can get onto decoration hung on rods. Not tested in play.

Missing for complete:

- Light the outward faces of the four towers.
- Replace the generic transformer with a pack model.
- Give the bay floor a reason: a required item or control, or a worthwhile reward at the chamber or the body, so the count-three hazard is actually played.
- Put something at the end of the cross-catwalk or stop it at the stack.

### 87 OBSERVATION RING / NORTH-WEST

makeObservationRingNorthWest (rgQuadrant seed 0). Ring hub: the rim walk at -12 round the pit corner, a glazed pit-control bay pushed onto the walk, open-bay galleries with rails, pit control desk in the NW corner room, an instrument boom out to the mast. The hall above (sodium washers, pilasters, violet pit, mast) is deliberate and reads well. The galleries behind the glass are empty rooms.

Route: N 10-14 @-12 (from 86), W 9-14 @-12 (to 91), E 1-23 and S 1-23 @-12 open to 88 and 89. Edges match; seams stitch cleanly in the renders. Audit: 6902 cells, all standing.

Audit: PASS: 0 placement problems

Faults:

- **major**, West gallery (1-7.8, 8-24) and north gallery east of the bay; sv_87_1, sv_87_3: Empty 7 m deep grated rooms: bare concrete, one tube, at most one bench. 15 fixtures in the whole chunk. They read as boxes.
- **minor**, Rim walk between gallery face (8) and rail (15.9); sv_87_2: 8 m of bare grating with nothing on it but rail lamps.
- **minor**, Whole chunk: The hub where three routes meet has no event, encounter or interaction beyond one lore terminal.

Missing for complete:

- Dress the two galleries (instrument racks, observers' desks, cable runs, something abandoned mid-task) or cut their depth.
- One beat at the hub: a scripted moment seen across the pit, an encounter, or a control that does something to the pit apparatus.

### 88 OBSERVATION RING / NORTH-EAST

makeObservationRingNorthEast (seed 1, mirrored x). The visitors' side: couch-and-boardroom-chairs room in the NE corner, a carpet runner along the rim with brass posts and rope, viewing benches (one overturned), two exhibit lecterns, three interpretation boards with spots, display cabinets, dropped passes, hats and brochures, dropped emergency shutters over the gallery glass, a collapsed viewing balcony over the pit. The most dressed ring quadrant.

Route: N 10-14 @-12 (to 85), W 1-23 and S 1-23 @-12 open to 87 and 90. Edges match. Audit: 6738 cells, 6727 standing.

Audit: PASS: 0 placement problems

Faults:

- **major**, Whole chunk: No gameplay: nothing to fight, find, switch or take. It is a walk-through exhibit on the way to 85.
- **minor**, North and east galleries behind the shutters and glass; sv_88_0, sv_88_3, sv_88_4: Dark, empty rooms apart from the visitors' corner.
- **minor**, Whole chunk: 33 lights, slightly over the ~30 budget.
- **minor**, Collapsed balcony (18-21, 15.9-18.4): Not verified: no rendered view showed it.

Missing for complete:

- One authored beat: the brief's cleanup-squad-versus-creatures fight seen across the pit would fit here, or an encounter in the visitors' room, or a pickup worth the detour.
- Some dressing or light in the two galleries.

### 89 OBSERVATION RING / SOUTH-WEST

makeObservationRingSouthWest (seed 2, mirrored y). The pit crew's quarter: crew room in the SW corner (hung suits, four lockers, bench, scrub sink, shower tiles and drain, hat shelf), the hoist gantry with a man-cage hung halfway down the pit, a winch station on the walk, fixture ladders on the lining, a broken observation bridge hanging into the pit behind a chained gate. This is the weakest map in the district: five of six spot views show only empty grating and dark concrete.

Route: N 1-23 and E 1-23 @-12, both to other ring quadrants: a dead-end quarter nobody has to enter. Edges match. Audit: 6570 cells, 6561 standing; reached heights are -12, -11.5 and -11 only, so the pit floor at -28 is not reachable.

Audit: PASS: 0 placement problems

Faults:

- **major**, The pit (16-24, 0-8, floor -28): The lead designer's notes ask for an optional stair down into the pit here 'with rewards and danger'. There is none: the ladders are fixtures, the bridge is broken, the pit is look-only from all four chunks.
- **major**, Crew room (1-7.8, 16.2-23); sweep-89 crew-room rows, sv_89_1: Close to black. Two lamps only (0.4 with flicker 0.3 at (4,19.6); 0.35 at (6.9,20.6)); suits, lockers and bench are barely readable and several sweep frames are plain dark concrete.
- **major**, West and south galleries and the rim walk; sv_89_0, 2, 3, 4, 5: Empty grating and bare concrete in every view.
- **minor**, Whole chunk: No reason to come here: one Health pickup and one creature, no text, nothing that pays off the PIT CONTROL note in 87 about Molloy.
- **minor**, Broken bridge, hoist cage: Seen only in sweep thumbnails; could not judge them at detail.

Missing for complete:

- The pit descent (stair or working ladder or the hoist cage to -28, hazards on the pit's clock, a reward at the cradle, a way back up); or, if that is cut, say so in the brief and give the quarter a smaller reason to visit.
- Relight the crew room.
- Dress the galleries.
- A readable or terminal for Molloy.

### 90 OBSERVATION RING / SOUTH-EAST

makeObservationRingSouthEast (seed 3, mirrored x and y). The district's front door from Containment Intake. Detail Nine's camp: a scrap barricade round the door with a talking gap, burn barrels, bedrolls, crates, a generator and lamp in the SE corner room, and a no-man's-land on the south arm between Nine's line and the survivors' line with a shared brazier. The containment feed trunk duct hangs over the pit. Reads as a specific, lived-in place with warm fire light against the sodium hall.

Route: E 9-14 @-12 (from Containment Intake 95), N 1-23 and W 1-23 @-12 open to 88 and 89. Edges match. Audit: 6516 cells, 6481 standing. This is the only way into the district on a first visit.

Audit: PASS: 0 placement problems

Faults:

- **minor**, Survivors at (2.25,10.2), (2.3,13.7), (2.25,15.3); sv_90_0: They blow out to yellow-white under the brazier lamp (1.35 intensity, 2.4 m away).
- **minor**, Dialogue states rg_nine_hint and rg_nine_feed: Set by the dialogue, read nowhere in src/world or src/game. The hint has no objective or journal payoff.
- **minor**, Sergeant's post (19.5,12): He stands in the 1.9 m slot between the crate stacks directly behind the barricade gap, so walking straight through is probably body-blocked. The way round is north of y 7.8. Not tested in play.
- **minor**, Truce behaviour: Not verified: whether the truce breaks on a raised weapon as the builder comment says is engine behaviour I did not test.

Missing for complete:

- Nothing required. Worth doing: tone the brazier down on the survivors, and make the feed hint land (objective text or a consumer for rg_nine_feed).

### 91 SHIELDING CORRIDOR

makeShieldingCorridor. A lead-faced corridor west from the ring with two concrete bulkheads, striped frames and green-tinged work lamps; four men burnt onto the north wall as soot figures; a dosimetry post behind glass on the north side; a bend south through a blast door; a shielding store off the bend. The corridor itself reads as a real, distinct place (sv_91_0). The post and the store do not hold up.

Route: E 9-14 @-12 (from ring 87) -> corridor west -> bend -> door at y=18 -> S 10-14 @-12 (to 92). Edges match. Audit: 2790 reachable cells but only 2363 standing: the whole dosimetry post is marked 'c' (crouch-only) in map-audit-91.txt.

Audit: PASS: 0 placement problems (INFO PERCH on the soot slab 16.26,9.06-16.74,9.07; the post is reachable only crouched)

Faults:

- **major**, Dosimetry post door (swing door 16.4-17.6, y=8.95): Soot figures 2 (x=16.5) and 3 (x=17.8) and their west-thrown soot bands are solid slabs at y 9.06-9.075, standing 6 cm in front of the post's doorway where there is no lead wall behind them (the lead panels skip the door; the figures do not). Bands span x 16.5-17.56 at z -11.05..-10.8 and x 16.9-17.6 at z -11.85..-11.2. The audit confirms a standing player cannot enter the post; the ammo, the I.I. order and the assault guard are behind it. Black slabs hang in an open doorway.
- **major**, Shielding store (2-9, 15-22); sv_91_1, sweep-91 store rows: Near-black: one 0.35 tube with flicker 0.5. The lead stacks are plain boxes on pallets. Reads as a dark concrete box.
- **minor**, North wall soot figures at x 15.6, 16.5, 17.8, 20.9: The room's one idea. I could not make out figures in any rendered frame (sweep thumbnails show dark smears only). Not verified as legible.
- **minor**, Blast door at y=18: Described as the bend's blast door but it is a plain door with no state, cost or moment.
- **minor**, Corridor east end (x 19-23) and south wall: Very dark in the sweep; the first metres in from the ring are the least lit part of the route.

Missing for complete:

- Clear the post doorway: mark the soot slabs non-solid and move figure 2 and figure 3's bands onto lead, off x 16.3-17.7.
- Light and dress the store.
- Confirm by a proper shot that the four figures read as men, and relight them if not.

### 92 EXPERIMENTAL POWER

makeExperimentalPower. A 10 m tall switch hall: entry landing at -12 with a routing desk, stair down to -16, a six-metre main board on the west wall, a nine-metre breaker wall across the whole south side framing the exit (three tiers of knife switches, ammeters, trunks to the roof, bay 5 arcing), two transformers on pads, a flooded live corner NE, cable reels under the landing. The breaker wall from the landing (sv_92_0) is a strong first view.

Route: N 10-14 @-12 (from 91) -> landing -> stair (10-14, y 6-12.16, 22 steps) -> hall -> S 10-14 @-16 (to 93). Edges match. Audit: 6842 cells, 6752 standing.

Audit: PASS: 0 placement problems

Faults:

- **major**, model::transformer at (5.5,19.8) and (18.5,19.8); sv_92_1, sv_92_3: The generic transformer box: sv_92_1 is filled by a featureless pale slab with a finned top. They stand either side of the exit, in front of the best wall in the map.
- **minor**, Required route: stair foot (10-14, 12.16) to S exit (10-14, 23): A straight 11 m walk. None of the three arc hazards touches it and nothing gates it; the hall is scenery to pass through with one creature off to the east.
- **minor**, Under the landing and stair; sv_92_5: Black: the view from (10,13) is an unlit stair underside.

Missing for complete:

- Replace both transformers with pack models.
- Put the hall's danger on the route or give it a task: for example the fault arcing across the exit so the player must time it, or a breaker to pull before the exit is safe.

### 93 CONVERSION LAB

makeConversionLab. The district's objective room: four conversion columns down the hall, two dead and dark, two live in open glass cages with a violet charge that fires every 2.2 s; waist-high manifolds forcing a zig-zag; a filling belt with charged cells; a live bank on the south wall; control room on a mezzanine at -12 up a stair. The live cages in violet light read well (sv_93_1, sv_93_4). One fix from complete.

Route: N 10-14 @-16 (from 92) -> lane between the columns -> stair (16-18, y 9-15.16) -> control room door at y 8.8-9 -> routing board; back out the same edge. Dead end by plan. Edge matches. Audit: 6690 cells, 6621 standing; mezzanine and terminal reached.

Audit: PASS: 0 placement problems

Faults:

- **major**, Live columns 2 (11.5,8.6) and 4 (11.5,16.8): The lane's hazard cannot hurt. Each etArc box equals its cage footprint (10.1-12.9 by 7.2-10.0 and 15.4-18.2), and the cage is closed on four sides by glass panes over a solid kick plinth; the audit map marks those cells 'o' (nobody can stand there). The 'cover, then a lit bend' beat in the builder comment is visual only. Inferred from geometry and the audit map, not tested in play.
- **minor**, cont_power terminal (20,1.75): Setting the chapter's key state has no event: no cue, sound, shake or checkpoint, only a lamp swap in a room the player is facing away from when leaving.
- **minor**, Dead columns 1 (4.5,5) and 3 (4.5,12.6); sv_93_1, sv_93_4: Render as black masses; each has only two 0.14 sight-glass glows.
- **minor**, SE floor (16-23, 15-23): Empty concrete under one dead high-bay.
- **minor**, Control room interior: Not verified: no rendered view from the mezzanine.

Missing for complete:

- Make the live columns reach the lane: extend each arc box about a metre onto the lane side, or open that side of the cage.
- A ScriptEvent on cont_power: bank cue, sound, shake, checkpoint.
- A little light on the dead columns.

## Containment (94-103)

Containment is past block-out as a route but not finished: two maps hold up as complete (96, 99), seven are dressed with faults, and 102 Breach Sector, the district's ground zero on the critical path, is still boxes. All ten pass the audit, the placement check and the campaign map test, and every edge matches the plan; the gates on cont_power (95, 97) and cont_barricade (103) are wired, though I did not play them. The weak point is art: resonance growth and crystal are box walls in materials 34 and 35 in 96, 97, 98 and 102, and LAB 4's wall writing in 100 is straight bars. The critical path is 94, 95, 98, 102, 103; the other five maps are optional and none sets or needs a state, and both source files still carry a BLOCK-OUT header.

**Top fixes**

- 102 Breach Sector: rebuild the crater. The rim is box walls in material 26 that render as stone blocks, the spire at (8.6,7.2) is cyan cuboids, and the crest fixtures at (1.7,4), (1.6,10.6), (1.7,16.2) read as dark blobs. Use the cave pack or terrain, add the collapsed decks the brief asks for, close the jump onto the NE block (18-23, 1-4) and the north rim top, and light or fill the SE pocket (19-23, 17-23).
- Replace box-built growth and crystal (caCrust/caMound/caKnot and the cb equivalents) with modelled growth: 98's aisle wall (9.5-14.5, 10-12.5) and W1 spire (3.4,3.6), 96's A4/A8 stack, 97's slot cell and B spur. One asset pass serves four maps.
- 100 Isolation Wing: lock doors 2 and 5-8 (plain swingDoors with non-solid bars today), replace the bar-and-paper 'handwriting' in Isolation 3 with real wall writing via the sign designer, and shrink or dress the six empty 5.9 x 8.3 m bunkers.
- Light the black faces seen in the renders: 95 gallery south face and SW pocket (1-6, 20-23); 96 service passage east wall (x 14.4, y 17.5-23); 97 slot spur and plenum; 98 vestibule west wall; 99 bay 4 with the forklift (19.2-23, 19-23); 103 strip (1-6, 6.5-7.8).
- 97 Containment Service: make the viewing slots work (now 1.2 x 0.3 m into a dark cell) and carry the junction's pipework and lamps into the south run, plenum and west spur.
- 103 Emergency Containment: give the cont_barricade terminal a prerequisite or a second step, build a barricade that visibly rises at the south door, and add the knocking the welded room's note promises.
- Give the optional maps (96, 97, 99, 100, 101) a mechanical payoff, for example a state from the theater or handling floor that the barricade release needs, so half the district is not skippable.
- Radio and truce: every Investigator exchange in 95, 98 and 103 is a caption-only cue (Sound::Empty) and each truce is set by a trigger box and never revoked, against the design doc's beat 3. Voice the lines and decide the truce logic; remove the BLOCK-OUT headers once done.

| Chunk | Map | Rating | Effort | Beats |
|---|---|---|---|---|
| 94 | QUARANTINE WARD | dressed | small | No gate of its own. One Transformed in boarded Q2 at (5.6,18.8). Terminals WARD ALARM / Q1 (10.5,14.15) and QUARANTINE / WARD STATE (14.5,7.7). Four taped notes, including LAB 4's FOLLOW THE LAMPS at (22.4,9.21). Set pieces: Bay 2 isolation tent with a drag trail to Q2's boards; three lit desk lamps down the east passage. Red beacons blink until cont_power. Clock cona_qw_clock. |
| 95 | CONTAINMENT INTAKE | dressed | small | Gate: S door requires cont_power (set in the Conversion Lab, CampaignChapterEnergyTestLabs.cpp); wallSign CONTAINMENT POWER: OFFLINE; red beacons until cont_power, green lamp after. Terminals CONTAINMENT FEED / STATUS (gallery 1.75,7.5) and DOOR B (16.1,22.2). Intake detail of three armed guards (cona_hub_heavy at 12,15.6; cona_hub_medic at 17.8,13.4 with talk cona_hub_hello; cona_hub_marks patrol |
| 96 | CONTAINMENT A | complete | small | No gate. Encounter: two hostile armed guards (cona_a_walk on a four-point patrol; cona_a_post sharpshooter at 19.6,15.4), no truce. Terminal CONTAINMENT A / CELLS (12.3,10.7). A2 occupant standing at the glass (looped actor at 9.2,6.05). A3 ore sample on a plinth with hum cona_a3_hum. Notes A4 / VACATED 02:40 and S2 / SEALED BY DETAIL A. |
| 97 | CONTAINMENT SERVICE | dressed | medium | Gate: bulkhead door requires cont_power; wallSign BULKHEAD HELD; two beacons. Steam hazard (8.7-9.7, 5.9-7.1), 12 dps, 1.8 s on in 6. One Transformed in the plenum at (21,16). Three viewing slots into a growth-filled cell with an anomaly hazard and hum cona_svc_hum. Dead Cleanup man with a pencil note at (1.01,8.2). Crew barrier and B SPUR / CLOSED note at (5,16). |
| 98 | CONTAINMENT B | dressed | medium | Route puzzle: the lamped detour through E1 and the booth. Three Transformed (W1 5.6,6.2; E1 19.2,5.4; south aisle 12,20.6). B detail of two guards (cona_b_lead with talk cona_b_hello; cona_b_doc), stance cona_b_stance set to Truce by event cona_b_truce (box 9.5-14.5 x 4-9). Dialogue cona_b_hello/_what/_come/_way. Radio cona_b_radio (four caption cues). Terminal CONTAINMENT B / CELL STATE (22.2,11) |
| 99 | SPECIMEN HANDLING | complete | small | No gate. Two Transformed on the floor at (13.9,16.8) and (10.1,21.4). Terminal CRANE 2 / LAST LIFTS (22.2,4.2). Set pieces: cage 7 on the hook with its door open; the hoisted crate over its cart. Anomaly pool (6-9.4, 12.8-14.6, 4 dps) with hum conb_sh_hum. One wallSign. |
| 100 | ISOLATION WING | dressed | medium | No combat. Terminals ISOLATION WING / OCCUPANCY (22.4,11.5) and ISOLATION 3 / INTERCOM (17.6,8.6). Three LAB 4 wall notes in Isolation 3 and the detail's order at (11.75,13.49). Ten lit desk lamps as the trail. Bunker 2 occupant at the viewport (looped actor at 8.65,14.15). Dead Cleanup sharpshooter in bunker 4. Door 3 authored open. |
| 101 | OBSERVATION THEATER | dressed | medium | No gate. One Transformed at (5,20). Terminal THEATER / DEMONSTRATION 7 (17.5,10.35). Set piece: subject (looped actor at 12,16) inside four breakable glass panes with an anomaly hazard (0 dps) and hum conb_th_hum; a fallen rig lamp still burning; two dead scientists on the tiers. |
| 102 | BREACH SECTOR | blockout | large | One Warden at (8,12.4), intended as an evade-only hunter. Anomaly fields: spire core 14 dps inside a 0 dps shimmer, four knots at 10 dps inside shimmer boxes. Fire hazard along the north wall (7-11, 2-2.5). Note SECTOR LOST / 03:05 at (22.98,6.2) stating the Warden's rules. Three bodies. Hum conb_br_hum. No gate, no state. |
| 103 | EMERGENCY CONTAINMENT | dressed | small | Gate: S door requires cont_barricade, set by terminal EMERGENCY BARRICADE / SOUTH (22.2,10.6); beacons until set, green lamp after; wallSign BARRICADE DOWN. Event conb_ec_raised spawns two Transformed at (11,3) and (14,4.2) with a shake. Emergency detail of two guards (conb_ec_heavy; conb_ec_lead with talk conb_ec_hello), stance conb_ec_stance set to Truce by event conb_ec_truce; one reply sets co |

### 94 QUARANTINE WARD

makeQuarantineWard: a clinic ward with a decon airlock off the Morgue, a central corridor with wired-glass wards Q1/Q2 to the west and a nurses' station and holding room to the east, and an east passage lit by LAB 4's desk lamps. Teal dado, clinic tile, ward beds, wheelchair, gurney and an isolation tent make it read as a specific place.

Route: N 10-14 @-15 (door, lock held on the Morgue side) -> airlock -> corridor -> S 10-14 @-15 to the hub; E 9-14 @-15 to the Isolation Wing. Edges match the plan. Audit: 5517 reachable cells, everything required reachable; Q2 is reachable only through the plaster hole from Q1 at (6.9-8.3, 14.4).

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **minor**, Holding room (14.2-23, 14-23): A 9 x 9 m room that is mostly bare floor: one body, one crate, three lockers, three sheeted corpses against the east wall. Reads as a box in sv_94_2.
- **minor**, Ward Q2 (1-9.8, 14.6-23): Large and sparse (three bed frames, a mattress, floor stains). From inside, the boarded doorway at (9.8, 19.8-21.2) is a flat dark rectangle.
- **minor**, Q2 creature / mandatory route: The only encounter sits behind the boards and the route never enters Q2, so the required path is a corridor walk with reading. The man 'who still says his name' has no cue or voice.

Missing for complete:

- One beat on the mandatory corridor (for example the Q2 occupant reacting at the boards as the player passes, with audio)
- Dress the holding room and Q2 to the density of the corridor and Q1

### 95 CONTAINMENT INTAKE

makeContainmentIntake: the district hub, a 7 m concrete intake hall with a west gallery at -12 leading to the Observation Ring, a two-tier crate barricade with a steel shield and floodlight facing door B, and a wrecked checkpoint gate at its east end.

Route: N 10-14 @-15 in; E 9-14 @-15 to Containment A; stair (6-10.5, 3.3-4.8) up to the gallery and W 9-14 @-12 to the ring; S 10-14 @-15 door on cont_power to Containment B. Edges match the plan. Audit: 7380 reachable cells, all exits and both terminals reachable.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **major**, Gallery mass south face (x 1-6, y 20) and the SW pocket (1-6, 20-23): Pitch black: no lamp reaches it, so in sv_95_1 a 5 x 3 m wall face fills the view as a black void beside the dead Cleanup man at (6.8,20.6).
- **minor**, North half of the hall (6-22, 1-10): Empty concrete floor: three crates, a hand cart, two columns. Reads as a box in sv_95_5.
- **minor**, Gallery deck (1-6, 3-20 @-12): A 5 x 17 m deck with one console and one workstation, dark at the south end (sv_95_4). The feed console that explains the whole detour is under-lit.
- **minor**, Truce / radio: The truce is set by walking into a trigger box at the north door, not by anything the player does, and is never revoked as the design doc's beat 3 describes. All four radio lines are cue(...Sound::Empty...), caption only.

Missing for complete:

- Light the SW pocket and the gallery's south face
- Dress the north half as a working intake (pallets, transfer airlock, scales) and the gallery as a control position
- Voice the radio exchange; decide whether the truce is earned or revoked

### 96 CONTAINMENT A

makeContainmentA: an intact two-storey cell block (A1-A4 on the floor, A5-A8 off a gantry) with stencilled numbers, a hazard band, ID scanners and a CRT monitoring island; two sealed south cells; the A4/A8 stack blown out with growth spilling down. The best-looking map in the district.

Route: W 9-14 @-15 from the hub -> observation floor -> S 10-14 @-15 by the service passage (9.6-14.4, 17.5-23) to Containment Service. Edges match the plan. Audit: 4339 reachable cells; A1-A3 and the south cells are correctly sealed, A4 is enterable.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **minor**, A4/A8 stack (18-23, 1-8): The growth is box walls in material 34 with cyan material-35 cubes for crystal; up close (sv_96_1, sv_96_3) it reads as stacked textured cubes.
- **minor**, Service passage east wall (x 14.4, y 17.5-23): Renders black in sv_96_2. The only light is one flickering tube at (12,20.5), intensity .42.
- **minor**, Upper cells A5-A7: Ward beds sit in cells with no stair or door, and the gantry has no access ladder.

Missing for complete:

- Polish only: modelled growth at A4/A8, a second lamp in the south passage, a ladder or stair to the gantry

### 97 CONTAINMENT SERVICE

makeContainmentService: 3.4 m service corridors. The valve junction NW (pump, two headers, three risers, gate valves, gauges, receiver vessel, weeping steam) is well dressed. The rest is bare concrete tube: the slot spur east, the plenum with two air handlers, the south run and the west spur to B.

Route: N 10-14 @-15 from A -> junction -> bulkhead (11.4-12.6, y 11) on cont_power -> W 15-19 @-15 to B or S 10-14 @-15 to the theater. Edges match the plan. Audit: 3559 reachable cells with doors open. Before power this is a dead end by design.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **major**, Slot spur (13.6-23, 6.6-8.4) and its cell: The map's one idea does not read. The slots are 1.2 m wide and 0.3 m tall (sill 1.35, head 1.65) and the spur is near black; sweep frames show a black wall with one small magenta patch.
- **major**, Plenum (13.6-23, 12-20): Almost unlit (one 1000 K lamp, intensity .3, flicker .4). The two pwr_air_handler models show a coarse, pixelated fan texture at corridor distance.
- **minor**, South run (10.6-13.4, 11-24) and west spur (1-10.4, 15.2-18.8): Plain concrete tubes with a wall lamp or two; none of the pipes, valves or caged lamps the brief asks for outside the junction.
- **minor**, Return pipe at (12.9,2): A 0.4 m pipe with its collision proxy stands 1 m from the spawn at (12,1.6) inside the 2.8 m run; it fills sv_97_0.

Missing for complete:

- Slots large and lit enough to show the cell, as the set piece
- Carry the junction's pipework and lamps down the south run, the plenum and the B spur
- A reason to come here: it only repeats the cont_power gate and returns nothing to the main path

### 98 CONTAINMENT B

makeContainmentB: the breached cell block. A central aisle with blown cell fronts, grown shut wall to wall by a stepped mass of growth; W1 holds a crystal spire; the cell control booth east holds the last two of Kessler's detail. On the critical path.

Route: N 10-14 @-15 door from the hub -> vestibule -> aisle blocked at y 10-12.5 -> hop over the heave (13.3-15.6, 4.5-6.45, 0.42 then 0.92 m, needs a jump) into E1 -> hole in E1's back wall (15.7-17.1, y 9.5) -> booth -> booth door (18-19.2, y 14.9) -> passage -> E 15-19 to service, S 10-14 to Specimen Handling, or the 1.4 m lane (y 12.6-14) W 9-14 to the Breach Sector. Edges match the plan. Audit: 4685 reachable cells.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **major**, Aisle wall (9.5-14.5, 10-12.5, up to f+5.2) and west passage mass (2.6-9.5, 9.25-12.4): The landmark of the map is a magenta ziggurat of four stacked boxes with cyan cubes on it (sv_98_0, sv_98_1). It reads as block-out geometry, not growth.
- **major**, W1 spire at (3.4,3.6): Five cyan cuboids on a stepped magenta base (sv_98_5); the same box construction as the Breach Sector spire.
- **minor**, Vestibule west wall (x 9.3-9.5, y 1-4.2): Renders black from the entry (left third of sv_98_0); the vestibule has one tube of intensity .4.
- **minor**, Booth (14.7-23, 9.7-14.8): Sparse: one control board, a crate, desk lamps on the floor, a bare panel wall (sv_98_2).
- **minor**, Radio / truce: Same as the hub: trigger-box truce, caption-only radio (Sound::Empty).

Missing for complete:

- Modelled growth and crystal in place of the box stacks (the same change serves 96, 97 and 102)
- Make the detour read: the heave and the hole need to look climbable and intended
- Dress the booth as a cell control room

### 99 SPECIMEN HANDLING

makeSpecimenHandling: a handling floor at -18 under a landing and gallery at -15. Holding cages west, gated bays south, crane beam with cage 7 hanging open at 3.8 m, and a transfer line (cage, floor rail, hoist with a strapped crate over a cart, autopsy table, lift behind bay 3) under sodium light.

Route: N 10-14 @-15 from B onto the landing -> gallery deck -> E 9-14 @-15 to the theater; stair (10-11.6, 6-10.5) down to the floor. Edges match the plan. Audit: 7361 reachable cells.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **minor**, Bay 4 (19.2-23, 19-23): Unlit: bay lamps exist only at x 4.1, 10.1 and 16.1, so the forklift at (21.1,20.8) is a black silhouette (sv_99_1).
- **minor**, Floor NW quadrant (1-8, 1-9) and the landing mass faces (8-17, y 6): Empty concrete and a dark 3 m wall face (sv_99_3).
- **minor**, Floor as a whole: The route never needs the floor; its only reward is one health pickup at (2.2,18.2), so both creatures and the transfer line are optional.

Missing for complete:

- Polish only: a lamp in bay 4, something in the NW quadrant, a reason to go down to the floor

### 100 ISOLATION WING

makeIsolationWing: one corridor with eight concrete bunkers. The corridor is strong: steel door surrounds, stencilled numbers, drop bars, a hard red lamp on door 2, LAB 4's desk lamps on the floor. The bunkers behind the doors are 5.9 x 8.3 m bare concrete boxes with a slab bed and a toilet each.

Route: W 9-14 @-15 from the Quarantine Ward's east passage -> corridor -> Isolation 3 (13.1-18.9, 1-9.3), back out W. Dead end by design. Edge matches the plan. Audit: 6496 reachable cells with doors open.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **major**, Doors 2 and 5-8 (swingDoor in the loop at lines 252-279): Drawn barred and, for door 2, chained with three restraint bars and a DO NOT OPEN note, but they are ordinary swingDoors with no requireState and the bars are solid=false. From code the player can open door 2 and walk through its bars to a passive actor; sweep frame pt 43 shows the bars hanging across the open doorway. Not tested in play.
- **major**, Isolation 3 back wall (13.5-18.2, y 1) and east wall (x 18.9, y 3-7.5): LAB 4's handwriting, the district's story beat, is five straight dark bars per wall plus paper sheets taped up and titled WRITTEN ON THE WALL, IN PEN. Nothing reads as writing (sweep pt 13, pt 21).
- **major**, Bunkers 1, 4, 5-8: Empty dark boxes far too big for isolation cells (sv_100_1, _2, _4): slab, toilet, one tube. Six of the eight rooms hold nothing.

Missing for complete:

- Lock the barred doors with a state that is never set, as 103's welded door does
- Real wall writing in Isolation 3 using the sign designer / SignArt, with the notes as the readable text
- Shrink or dress the bunkers so an open door shows a cell, not a void

### 101 OBSERVATION THEATER

makeObservationTheater: a landing and west gallery at -15, five raked tiers of benches behind a viewing glass blown out in three places, and a stage at -18 with a glass enclosure holding the subject under rig lamps. Reads as a theater, but the set piece and seating are built from boxes.

Route: N 10-14 @-15 from service -> landing -> west gallery -> W 9-14 @-15 to Specimen Handling; aisle stair (11.2-12.8, 4-8) down to the stage. Edges match the plan. Audit: 7359 reachable cells.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **major**, Containment 'cylinder' (10.3-13.7, 14.3-17.7): A square of four posts under a flat slab (sv_101_1, sv_101_2), not a cylinder and not a containment vessel. The panes are breakable but nothing is scripted to follow breaking them.
- **minor**, Tiers (4.1-23, 4-8): Benches are cbBench boxes, no seat models; the rake is dark and reads as steps from the stage.
- **minor**, Stage floor south and east (4-23, 19-23; 17-23, 11-19): Large empty concrete either side of the 9 x 6 m stage.
- **minor**, Gameplay: One creature in a corner; the subject does nothing. The map hands nothing back to the main path.

Missing for complete:

- A real containment vessel on the stage (the brief says cylinder) and a decided outcome if the glass breaks
- Seating from pack models, or at least bench ends and aisle lighting that read from the stage
- A payoff for coming here (state, item or shortcut)

### 102 BREACH SECTOR

makeBreachSector: ground zero. East and south ledges at -15 around a crater at -18, a west rim 11 m tall, a crystal spire at (8.6,7.2), anomaly fields and the Warden. The route, heights, hazards and lighting are authored, but every surface is a textured box: the rim is cuboids in a stone-block texture, the spire is cyan cuboids, the crust is a flat magenta floor. On the critical path.

Route: E 9-14 @-15 from B onto the east ledge -> slab stair (13.5-18, 10-11.6) down into the crater -> debris stair (7-8.6, 14.5-19) up to the south ledge -> S 10-14 @-15 to Emergency Containment. Edges match the plan. Audit: 6739 reachable cells, required route reachable, no trap reported.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **major**, Whole crater: rim walls (1-3, 1-19; 2.2-18, 1-2; 18-23, 1-4): Rock is flat-faced box walls in material 26, which renders as coursed stone blocks (sv_102_0, _2, _3). The crater reads as a stepped brick room, not a collapse.
- **major**, Spire at (8.6,7.2), eight walls in material 35 up to fl+8.4: The thing every ledge looks at is a cluster of cyan rectangular prisms on a magenta box (sv_102_1, sv_102_2).
- **major**, Rim crest fixtures at (1.7,4), (1.6,10.6), (1.7,16.2) z fl+11 and (6.2,1.5), (13,1.5): cave_stalagmite / cave_rock_column on the 1.2 m rim read as dark smooth blobs stuck high on the wall (sv_102_2, sv_102_0). cave_boulder_a at (4.3,3.6) renders near black (sv_102_1).
- **minor**, SE pocket (19-23, 17-23 @-18): A reachable, empty, pitch-black 4 x 6 m pit corner between the two ledges (sv_102_4 is entirely black).
- **minor**, NE block (18-23, 1-4, top -13.8) and north rim top (-13.5): Audit reaches both: a 1.2 m jump from the east ledge puts the player on the rim, 4.5 m over the crater (sv_102_3 was taken there). Probably unintended, and a safe perch from the Warden.
- **minor**, Warden note: The note's rules (frozen while watched, comes on reload and at light) were not checked against the Warden AI.

Missing for complete:

- Rebuild the crater from the cave pack (rock walls, slabs, boulders) or terrain so it reads as collapse
- A modelled spire and growth in place of the cuboids
- Collapsed decks and wreckage of the sector that was here (the brief asks for them; one catwalk section exists)
- Confirm the Warden plays as an evade moment and matches the note; close the rim perch

### 103 EMERGENCY CONTAINMENT

makeEmergencyContainment: the district exit. A hall crossed by Line A (shutter wall with a sealed lockdown door and a tear through panel 6), a welded room west, a glazed control room east, Line B of crates and barriers, and the south door with the Emergency detail in front of it.

Route: N 10-14 @-15 from the Breach Sector -> tear in Line A (19.6-21.1, y 6.2-6.5) -> control room door (19.6-20.8, y 13.1) -> west gap in Line B (x 1-6.4, y 15.5) -> S 10-14 @-15 door on cont_barricade to Deep Research. Edges match the plan. Audit: 6705 reachable cells with doors open.

Audit: PASS: 0 placement problems; placement check PASS.

Faults:

- **major**, Barricade terminal (22.2,10.6): No prerequisite: the text says raise on the Investigator's order only, and the player simply presses it. The district's exit gate is a walk to a console with no puzzle.
- **minor**, South door (10-14, y 22.4-23): The 'barricade lowered over the door' is a standard seam door under a sign; no barricade structure is built.
- **minor**, Welded room (1-6.2, 8-13): The note says it knocks and knows their names; no sound cue or script exists, and the dead medic inside can never be seen because the door never opens.
- **minor**, Slot between Line A and the welded room (1-6, 6.5-7.8): A 1.3 m dead-end strip, pitch black (sv_103_3).
- **minor**, Hall: Dim and sparse outside the two lines (sv_103_1, sv_103_5); the control room has one board, two cabinets and a display.

Missing for complete:

- A real release step for the barricade (hydraulic pressure, a second control, or the detail's consent) and a visible barricade that rises
- Knocking from the welded room
- Voice the radio; make the ally choice lead somewhere

## Deep Research (104-109)

Deep Research is a consistent, fully routed block-out with real authored ideas in every chunk, not a finished district: all six maps pass audit, check and the campaign map test, edges match the plan, but only the Bore Sample Archive (105) holds up as complete, and it is an optional dead end. The Deep Materials Lab (104) and Geophysics (106) have strong central halls let down by empty side rooms and unvoiced lines; the Energy Core hall (107), which carries the district's only gate (core_shutdown), is a near-empty shadowless hall round a concrete column; Deep Access Control (108) is a dark, bare box with the Investigator scene bolted on and a possible softlock behind the lift door; and the Extraction Interface (109), the chapter's last image, is box rock and box crystals with no ending. The most valuable work is the 107-108-109 run: make the core hall and its shutdown worth the build-up, make the Investigator scene safe and watchable, and give Chapter 2 an actual finale. None of the scripted beats was played in this pass, so they need one playthrough before anything is called done.

**Top fixes**

- 108: close the lift-room softlock risk. Door 1 (liftDoor, requires dr_lift_called, never set) is opened by script for about 4.4 s and then closed with a Checkpoint; keep the player out or give a way back, then play it to confirm.
- 108: relight the queue hall (tubes are range 6 at 5.7 m above the floor, so the floor is black) and restage the Investigator scene so it is not triggered 10 m in front of a hostile heavy with line of sight; make the cage and the Investigator actually leave view.
- 109: author the Chapter 2 finale (gate or timed bridge crossing, checkpoint, a LAB 4 or Investigator payoff, hand-off to 110); at present the chapter ends by walking off the map.
- 109: rebuild the rock and crystal from cave/crystal pack models instead of rectangular wall() boxes, break the lab panels where rock comes through, and light the mine_shield_ring at (12,9.6) and the stalagmite at (15.4,20.2), which render black.
- 107: dress the 30 m hall as plant (feed lines, valves, conduit, pit-floor machinery, gallery desks and staging; it has 12 fixtures in total) and give the core vessel a real body; restore shadows on the key lamps and bring 47 lamps down toward 30.
- 107: make the core_shutdown gate more than one terminal press: route the player through the timed arcs or implement the 'do not isolate under load' step the terminals already promise.
- 104, 106, 108: dress the nine side rooms that are bare panel boxes (104 prep room, night office, vestibule; 106 detail room, plotter room; 108 armoury, ready room, queue hall) with pack models and one motivated lamp each; add side fill in 104 so columns and guards are not black silhouettes.
- 106, 108: record the nine radio/Investigator/Cleanup lines that are currently Sound::Empty subtitles, and play the 106 truce (dr_geo_stance) once to confirm it reads and breaks correctly.

| Chunk | Map | Rating | Effort | Beats |
|---|---|---|---|---|
| 104 | DEEP MATERIALS LAB | dressed | small | No gate or puzzle, no states beyond the clock (dr_lab_clock) and guard damage/dead states. Encounter: 2 armed Cleanup guards, squad dr_lab (dr_lab_a patrols a 4-point loop round the cell, dr_lab_b sharpshooter posted at 21,11.5), plus 1 Transformed in the prep room (5.5,19.6). Set piece: D-7 in the hot cell with a decorative Anomaly hazard (unreachable). Story: terminal MATERIALS / NIGHT LOG at (2 |
| 105 | BORE SAMPLE ARCHIVE | complete | small | No gate or states. Encounter: 2 Transformed in the unlit racks at (8,5.5) and (13,17.3). Set piece: the lit 3,091 m ore core at the end of the aisle line, visible from the door. Story: terminal BORE 7 / CORE LOG at (17.2,9.6); LAB 4 sticky note FOLLOW THE LAMPS at (22.92,12.9) beside a lit scrap lantern. Pickups: ammo, health. |
| 106 | GEOPHYSICS | dressed | small | Truce encounter: entering the north lobby box (9,1)-(15,5) fires dr_geo_truce, setting dr_geo_stance to SquadStance::Truce for squad dr_geo (dr_geo_a at 10.6,20.6 and medic dr_geo_b at 13.6,21.6, both holding the south corridor). Radio set piece: box (8,14)-(16,17) starts timed sequence dr_geo_radio (finish state dr_geo_radio_done), four subtitle cues from the walkie on the pallet at (13.95,19.4): |
| 107 | ENERGY CORE RESEARCH | dressed | medium | The district's only gate: S door requires core_shutdown, set by terminal CORE FEED / ISOLATION at (17.6,16.9) on the pit floor. On shutdown: twelve violet lens glows go out, two periodic Electricity hazards round the core base (x8.6-15.4, y8.6-15.4, 35 dps, period 2.6 s) stop, three red wall lamps come on, the two red beacons at the S door go out. Encounter in the pit: 2 Transformed at (7.5,16.8)  |
| 108 | DEEP ACCESS CONTROL | blockout | medium | Scripted scene: entering box (9,1)-(15,4) starts dr_inv_scene (30 s, finish state dr_inv_leaving at 7.8 s). The Investigator actor stands at the booth glass (16.8,9.5), speaks, walks to the lift door (door 1 at x19.6-20.8, y6) which script opens on dr_inv_leaving, enters the cage at (20,3.6); dr_inv_exit then sets dr_inv_gone at 4.4 s, which closes the door and saves a Checkpoint. Five subtitle cu |
| 109 | EXTRACTION INTERFACE | dressed | large | No gate, no states, no checkpoint, no chapter-end event. Hazards: a periodic Electricity arc from a loose lead at (10.6-12.2, 7-8.2) on the west side of the ring approach (0.6 s in 2.4 s, 25 dps), an Anomaly pool round the drill head in the void. Encounter: Transformed on the bridge at (8.8,13.6), Transformed at (14.6,17.4), Mutant at (9.6,21). Story: terminal EXTRACTION INTERFACE / HEAD at (8.6,2 |

### 104 DEEP MATERIALS LAB

makeDeepMaterialsLab: a lab-panel hall (ceiling -9) with a four-sided glass hot cell at x8.5-15.5, y8.6-13.4 holding sample D-7 slung from a crane gantry, ringed by eight instrument stands; benches along the north wall, analysers west, mainframe east, observation desks either side of the cell. North strip is a vestibule (crew barrier, dead Cleanup man), suit room and post room; south strip is a prep room, a night office and the corridor on. 43 fixtures, 27 lamps.

Route: N 10-14 @-15 (door 0, from 103) into the vestibule, door 1 at x11.3-12.7 y5 into the lab, round the cell, then E 9-14 @-15 to the archive (105) or S 10-14 @-15 to Geophysics. Edges match the plan table; audit PASS with all three borders reached; the cell interior is correctly unreachable (glass holds).

Audit: PASS: 0 placement problems (6004 reachable cells); check PASS

Faults:

- **major**, Prep room (x1-8.8, y17-23), night office (x15.2-23, y17-23), vestibule (x6-18, y1-5); views sv_104_0, sv_104_1, sv_104_2: Read as bare panel boxes: from the room corners you see a door and four empty walls. Prep room has only the south counter and one cabinet, the office a desk and a records cabinet, the vestibule a barrier. No wall dressing, no ceiling fittings visible, flat dim light.
- **major**, Main lab, portal columns at (4.2,11) and (19.8,11), cell corner posts, both guards; view sv_104_5: Verticals render as pure black silhouettes in a lit room. All eight tubes are ceiling-mounted straight down (drTube at R-0.3), so vertical faces get only ambient 0.16. The column and the sharpshooter at (21,11.5) are featureless black cut-outs.
- **minor**, Hot cell interior; view sv_104_4: Instrument stands and their scopes/meters are near-black against the lit dado; D-7 itself is a tan box with cyan cubes on it. At 10 m it reads as a set piece, close to the glass it reads as stacked boxes.
- **minor**, Sign at wallSign(12,8.59,f+0.2) on the cell's north dado: A 2.4 m plate mounted 20 cm off the floor, below the glass sill; seen from the vestibule door it is a dark strip at ankle height (sv_104_0).
- **minor**, Analyser cabinets at (2.17,7.65) and (2.17,9.95); view sv_104_3: Side of lab_analyzer_cabinet is a flat featureless grey slab that fills the frame from the north-west corner.

Missing for complete:

- Dress the three side rooms so each has one idea (prep: slices, saw, sinks; office: the detail's kit, a made-up cot; vestibule: the Force's post) using pack models, and give each a lamp with a reason.
- Add low wall or bench-level fill in the main lab so columns, posts, stands and guards are lit from the side.
- Confirm in play that the two-guard fight works round the glass cell (not verified; guards have sight 13/15 and the vestibule door opens straight onto their patrol).
- Optional but cheap: a state-driven beat tied to D-7 (the terminal or note currently changes nothing).

### 105 BORE SAMPLE ARCHIVE

makeBoreSampleArchive: a tall (ceiling -7) dark concrete core store. Ten hand-built four-board racks of tagged core trays either side of a painted central aisle, roof trusses, pallets of boxed trays by the door, a hand cart; at the east end the logging table with bore 7's ore core under the one good pendant, a 2.5 m clamp-stand of crystal against a pale wall, depth chart, fridge and specimen cabinets. 1010 structures, 15 fixtures, 10 lamps.

Route: Dead end: in and out by W 9-14 @-15 from 104. Edge matches the plan; audit PASS, everything reachable.

Audit: PASS: 0 placement problems (7116 reachable cells); check PASS

Faults:

- **minor**, Logging table x17.8-21.8, y10.7-12.3: Audit has standing cells at z -13.95 on the table (spot view sv_105_5 was taken from on top of a core tray): the player can climb onto the table and stand on the cores.
- **minor**, Rack rows at y1.3, 3.9, 6.4 and 15.8, 18.4, 21.9: Racks away from the aisle are near-black silhouettes (only one 0.28 pendant at 10,4; the one at 10,19.5 is dead by design). Intended as flashlight territory, but trays and depth tags on the outer rows cannot be read at all without it.
- **minor**, East wall and the east third of the room: Bare concrete round a small table; south rack spacing is uneven (2.7 m gap between rows y18.4 and y21.9 against 1.7-1.8 m elsewhere). The brief asked for pack shelving (metal_shelf, lab_specimen_crate); the racks are built from structures instead, which reads fine in the dark.

Missing for complete:

- Nothing blocking. Verify in play that the two Transformed in the racks behave as an ambush and that the lantern-linked light follows its prop.
- Put an invisible proxy over the logging table so nobody stands on the cores.

### 106 GEOPHYSICS

makeGeophysics: a two-storey plotting hall (ceiling raised to -8) with a wall-scale geological section across the south wall (nine strata bands, a fault, fourteen station rays converging on a crystal node, lit violet), a wall of drum traces on the north wall, a core scanner on a raised steel platform at x8.3-15.7, y9-12.4, eight workstations and a row of drum recorders. Four side rooms under a 3.4 m soffit: chart store, server room, detail room, plotter room. 85 fixtures, 18 lamps.

Route: N 10-14 @-15 in from 104, straight through the hall under the section, S 10-14 @-15 down the south corridor (crew barrier at 11,18.6) to 107. Edges match the plan; audit PASS, both borders and all four side rooms reached.

Audit: PASS: 0 placement problems (6516 reachable cells); check PASS

Faults:

- **major**, Detail room (x1-8.8, y17-23) and plotter room (x15.2-23, y17-23); views sv_106_1, sv_106_2: Both are near-empty panel boxes. Detail room: one crate on a pallet, a bench, a hard hat and a floor lamp in a 7.8 x 6 m room. Plotter room: a photocopier, a body and loose paper. Nothing on any wall.
- **minor**, Scanner gantry at (12.3, 9.55-12.05); view sv_106_4: Gantry legs, beam and detector head are plain slabs with a rough concrete-looking texture and a cyan plate underneath; up close it reads as blocks, not an instrument. No pack model anywhere in the scanner except a scope and a field meter on top.
- **minor**, All four radio/guard lines (cue calls on dr_geo_radio): Authored with Sound::Empty, so as written they are subtitle text with no audio.
- **minor**, Truce trigger box (9,1)-(15,5): dr_geo_stance is only set to Truce by walking through the middle of the north lobby; nothing in the builder shows what the player sees to learn the rule (weapon down), and nothing was verified in play.

Missing for complete:

- Dress the detail room as a place two frightened men have been living in (bedrolls, ration boxes, ammo tins, their kit) and the plotter room with plotters, chart rolls and a light table from the packs.
- Rebuild the scanner from pack models or at least give it a steel material and a control console.
- Record and filter the four radio lines (STT-verify), and play the truce once to confirm it holds and breaks correctly.

### 107 ENERGY CORE RESEARCH

makeEnergyCoreResearch: a 30 m concrete hall (floor -25, galleries -15, ceiling +5). An octagonal core vessel 20+ m tall in the middle of a pit with crystal lens strips up each face, two ring gantries of pwr_catwalk_section, three manipulator arms, feed pipes; north and south galleries joined by an east ledge; two stair flights down the west wall to the pit; a steel stair up the east wall to a corner overlook at -10. Only 12 fixtures and 2 props in the whole chunk; 47 lamps.

Route: N 10-14 @-15 onto the north gallery; west stairs (x1.1-5, y6-13.6) down via the -20 landing to the pit floor; isolate the feed at the east wall; back up, along the east ledge (x19-23) to the south gallery and S 10-14 @-15 (door 0, requires core_shutdown). Edges match the plan; audit PASS, pit floor, galleries, overlook and both borders reached.

Audit: PASS: 0 placement problems (7491 reachable cells); check PASS

Faults:

- **major**, North gallery (y0-6), south gallery (y18-24), east ledge, pit floor; views sv_107_0, 107_2, 107_3, 107_5: The hall is almost undressed. Each gallery is a 22 x 6 m bare slab with a rail; the only furniture is one counter, a control board, a CRT and a chair at x14-18 on the north side. The pit floor has one switch cabinet, a knife switch and a gauge. No pipework, cable trays, tanks, pumps or plant feeding the 'primary machine' beyond two feed pipes.
- **major**, Whole chunk: `for(auto& l:m.lights)l.shadows=false;` and 47 lamps: Every lamp is shadowless (comment: 'the pit's sweep has to finish') and the count is well over the 30-lamp budget. The result is an evenly washed hall: the core does not throw the galleries into light and shade, and the machine has no modelling beyond its cyan strips.
- **major**, Core at (12,12); views sv_107_2, sv_107_3: The machine is a plain concrete-textured octagonal column with cyan crystal strips and two dark catwalk rings. The arms and feed pipes do not read from any gallery view. For the set piece the whole district builds to, it looks like a pillar.
- **minor**, Pit puzzle: The gate is one terminal press. The arc zone (x8.6-15.4, y8.6-15.4) never has to be crossed: stair foot at (3-5,6) to the terminal at (17.6,16.9) goes round it. The note promises 'it arcs at what moves, stand still' and the terminal says 'do not isolate under load' / 'LOAD 104%'; neither rule exists in the script.
- **minor**, Lower ring gantry at z -20 (catwalks at 8.4,12 etc.): Sits level with the stair landing 2.4 m away with no bridge and is non-solid decoration: it looks like a route and is not one.

Missing for complete:

- Dress the hall as plant: slurry feed lines and valves from the south pit face, conduit and cable trays up the walls to the rings, a control desk and lockers on the north gallery, staging on the south gallery, pumps and a sump on the pit floor.
- Give the core a body: forged bands, flanges, a base skirt and crown from pack models or steel materials, so it is not a concrete column.
- Restore shadows on the few lamps that matter (the four roof floods, the pit uplights) and cut the lamp count toward 30.
- Make the puzzle more than one press: either route the player through the timed arcs to the switch, or implement the 'under load' step (drop load at the north gallery console first).
- Play the shutdown to confirm the light change, the hazard stop and the door release (code-read only).

### 108 DEEP ACCESS CONTROL

makeDeepAccessControl: a queue hall (x1-16, y1-15) with six lockers, three hung suits, six bollards and a bench; a three-pylon badge-reader line at y15.1 and an open blast-door frame at y17.6-18.4; a glazed control booth (x16.2-23, y6.1-12.9); an armoury and a ready room in the south corners; and in the north-east an open lift shaft 9 m deep with a mine_lift cage, yellow headgear and violet growth coming up it. Floor is a slab built from structures over the shaft depth. 28 fixtures, 4 props, 22 lamps.

Route: N 10-14 @-15 (door 0, from 107 once core_shutdown is set), south across the hall, through a reader lane and the blast-door frame, S 9-15 @-15 to 109. Edges match the plan; audit PASS. The audit and sweep open the lift door, so they also count the lift room as reachable; in play that door (requires dr_lift_called, which nothing sets) is meant to stay shut.

Audit: PASS: 0 placement problems (6456 reachable cells); check PASS

Faults:

- **major**, Lift room x16.2-23, y4.5-5.9 behind door 1 (liftDoor): Possible softlock, not verified in play. The door opens 7.8 s into the scene and is closed by script about 4.4 s later, with a Checkpoint saved at that moment. doorLocked() in src/game/Movement.cpp is state-only and dr_lift_called is never set anywhere, so a player who runs round through the ready room and booth (about 32 m) and follows him in is sealed in with a save. The hall side of the shaft is closed by an invisible wall. If it reproduces this is a blocker.
- **major**, Queue hall floor, armoury, ready room; views sv_108_0, 108_1, 108_2, 108_3: The floor is near-black everywhere while the walls above are lit: the six hall tubes are drTube(...,0.6,range 6) mounted 5.7 m above the floor, so almost nothing lands on it. The heavy and the walker are black silhouettes.
- **major**, Armoury (x1-8.8, y15.2-23) and ready room (x15.2-23, y13.1-23): Empty boxes. Armoury: four lockers, one specimen cabinet and two pickups on the floor, no racks or benches. Ready room: a bench and a water cooler in an 8 x 10 m room. The queue hall itself is a 15 x 14 m dark floor with six bollards.
- **major**, Investigator scene trigger (9,1)-(15,4) against dr_acc_heavy at (12,13.8), sight 14, facing north: The scene starts at the door, 10-12 m in front of a hostile heavy and a patrolling rifleman with clear lines of sight. As authored the firefight should start while the heavy's calm line ('Sir. The drill team has asked for relief twice') plays from his position. Not verified in play.
- **minor**, Cage at (20,3.3), Investigator track end key {30000,cage}: The cage is a static fixture and the track is non-looping with no key that removes him: he appears to stay standing in the cage, in view over the shaft rail from the hall, while the lift-motor sound plays. Not verified in play.
- **minor**, Reader pylons at (9.43,15.1), (12,15.1), (14.57,15.1); view sv_108_5: From the south (the side the player leaves by and looks back from) lab_door_reader shows large flat pale-grey pods with no detail; they read as untextured.
- **minor**, Blast door at y17.6-18.4: Two jambs, a lintel and a floor line. There is no leaf, track or mechanism; it does not read as a blast door standing open.
- **minor**, All five Investigator/Cleanup lines: Authored with Sound::Empty: subtitle text only.

Missing for complete:

- Close the softlock: keep the player out of the lift room (hold the booth door shut during the scene, or set a state that reopens the lift door, or move the checkpoint), then play it.
- Relight the hall so the floor reads: lower pendants or longer-range tubes, plus light on the reader lanes and blast door.
- Restage the scene so it can be watched: trigger it from cover or behind glass, or hold the squad neutral until he is gone.
- Make the cage actually leave: animate it (platform actor) or cut the Investigator and cage from view when dr_inv_gone sets.
- Dress the queue hall, armoury and ready room with pack models (weapon racks, benches, ammo crates, cots, notice boards, a real blast-door leaf).
- Record the five lines.

### 109 EXTRACTION INTERFACE

makeExtractionInterface: the chapter's last map. A small lab (x4-20, y1-9.2) with control benches, an analyser, a mainframe, a toppled chair and a dead scientist, its south wall and ceiling broken by rock and violet growth; the mine_shield_ring at (12,9.6) as the way through; a steel threshold and painted rim at y12.4 over an 8 m void; one bridge (x7-10.6, y12.4-17); a drill derrick standing in the void at (12.6,15.8); a crystal formation east of it; a crystal seam face on the far wall; a timber set at the mine mouth and two lit scrap lanterns (LAB 4) on the far bank. 19 fixtures, 6 props, 22 lamps.

Route: N 9-15 @-15 from 108, through the lab and the ring, west along the rim to the bridge, across to the far bank, S 8-16 @-15 into Mine Access (110, Chapter 3). Edges match the plan; audit PASS, bridge, far bank and both borders reached.

Audit: PASS: 0 placement problems (3009 reachable cells); check PASS

Faults:

- **major**, mine_shield_ring at (12,9.6); views sv_109_0, sweep-109: The ring, its side power pack and its steps render as an almost pure black cut-out from the lab side, under three lab tubes. It is the map's centrepiece and the door the player walks through, and it has no visible surface.
- **major**, Rock everywhere: side masses at x1-6 and x18-23, far bank, seam face at y22.2-23; sweep-109 rows for THE SEAM: The cave is built from rectangular wall() boxes in rock material 26: flat tan planes meeting at right angles, with large blurry violet growth panels. Only two stalactite clusters, one stalagmite, a muck pile and a few crystal beds are real cave models. It reads as a room with rock wallpaper, not as rock swallowing a lab; the 8 m void is not readable from the lab or the far bank.
- **major**, Crystal formation at (14.7-18.4, 13.2-16.4), seam face prisms, derrick: Every crystal is a square-section cyan box (0.35-0.9 m), the derrick is four posts and box frames. As the image the chapter ends on it is programmer art.
- **major**, Whole map: Chapter 2 ends with no beat: no gate, no set-piece script, no checkpoint or title event, no payoff for the Investigator's descent seen in 108. The player crosses a bridge past three creatures and walks into 110.
- **minor**, cave_stalagmite at (15.4,20.2); view sv_109_4: Renders as a featureless dark purple-black blob about 2.5 m tall beside the exit.
- **minor**, Lab floor x4-20, y1-8.8 (material 11 steel plate): The 'clean lab' floor is a rusty orange-brown plate, not tile; with the tan rock it removes the clean-versus-rock contrast the brief asks for. The brief's cracked and buckled lab panels are absent: panel walls are intact rectangles.
- **minor**, cable_reel at (16.6,11.2); sweep-109: Flat untextured grey up close; it fills two sweep frames with a blank grey plane.
- **minor**, Threshold rim near (16.4,12.1): Audit has standing cells at z -13.9: the player can get about 1.1 m up onto the rail or the cable run at the rim (invisible proxies stop a fall). The arc hazard only covers the west 40% of the ring approach and is walked round on the east side.

Missing for complete:

- Rebuild the rock with cave fixture models and irregular masses (model::cave_* walls, boulders, columns), break and tilt the lab panels and ceiling where the rock comes through, and make the void visible from the ring.
- Light the shield ring from the lab side so it has form; replace or relight the black stalagmite.
- Replace box crystals and the box derrick with pack models (crystal clusters, mine/drill assets) or at least angled, varied prisms.
- Author the chapter finale: a gate or timed crossing at the bridge, a checkpoint, a last LAB 4 or Investigator beat, and a clear hand-off to Chapter 3.
- Give the lab its clean floor (clinic tile) so the contrast with rock works.
