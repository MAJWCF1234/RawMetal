---
tags: [design, chapter2, analysis-labs]
date: 2026-11-04
---

# Lab 4 and Field Measurement (maps 83 and 84): verdict on the three designs and the merged plan

## 0. Verdict on the three designs

I scored each design out of 10 on five measures.

| | Story | Stealth | Horror | Feasibility | Fit with 68-82 | Total |
|---|---|---|---|---|---|---|
| D1, the Blank (Lab 4 as a person) | 9 | 6 | 7 | 5 | 8 | 35 |
| D2, the Moth (stealth first) | 6 | 9 | 6 | 6 | 5 | 32 |
| D3, the Pointer (horror set pieces) | 8 | 7 | 9 | 7 | 7 | 38 |

**D1, the Blank.**
- Strengths: the strongest writing. The 01:58:04 "one trace" and the 02:10 contact read lead to the 02:15 red band reissue, which closes the slip in 72, "screening due" on her card in 70, and the survey in 84. The cut cell's sticky says "same." Her own words stay lowercase. Its light rule is the best of the three: the creature goes to the light, not to you, with a tell before it rushes.
- Weaknesses:
  - The Foundry-Base body would have to take the Mixamo clips through a retarget nobody has tried. The file `O:/retro official/Characters/Foundry-Base-PSX/Foundry_Base_PSX.fbx` exists, but its rig has not been checked.
  - The cradle socket adds a second mechanic.
  - Arden is a new name with no payoff.

**D2, the Moth.**
- Strengths: the deepest stealth. It has a duct, a hiding slot in the array island, lockers, and a creature that turns guard gunfire into a lure. Holding 84's airlock on `lab_chem_seal` is the right gate.
- Weaknesses:
  - Its Diagnostics detail repeats 71 almost line for line. 71 already has the burner at a barrel, a talking B ("two rounds and a radio"), and the truce, order and break chain (`CampaignChapterMedicalFront.cpp:464-485`).
  - Lamps the creature smashes and that switch themselves back on add state the AI changes, for little gain.
  - It says the creature pack has 26 takes. It has 21.

**D3, the Pointer.**
- Strengths: the best set pieces. The tape describes what is happening as it plays. The walking lamps are a tool the player can use. The terror comes as the player leaves with the disk.
- Asset: verified. `nightmare-creature-1_PSX.fbx` holds 21 named takes on `Creature_armature`: idle, walk, Run, crawl, crawl_idol, crawl_bite, state_to_crawl, crawl_to_state, jump, bite, attack_1-3, battle_idle, defence, eating, roar, hit_1/2, death_1/2. No retarget is needed.
- Weaknesses:
  - Two Transformed in 83 dilute the map.
  - Lighting hazards whenever the torch is on contradicts the "light that moves" rule.
  - Its "door that welcomes you back" commits to lore.

**The merged plan takes:**
- **From D3:** the creature (Nightmare-Creature, a new kind `Pointer`, R-46 and R-47, "the dogs"), the 84 tape set piece, and the walking-lamp rigs.
- **From D1:** the perception rule, Lab 4's story spine and drafts, the contact-read chain, the cut cell, and the survey in cell equivalents.
- **From D2:** the airlock gate in 84, a spoke reward (the field lamp bus), the array-island hiding slot, a prone crawl pass, guard gunfire as a lure, and the optional `SquadCaution` order.

**Cut:** the cradle socket, the Blank and Arden, smashing and self-restarting strobes, light-gated hazards, the second Transformed in 83, and 71-style truce dialogue.

**One new creature and one mechanic.** Her drafts are the climax: the guttering lamp at her desk is the lure that pulls R-47 off the disk.

## 1. Repo facts this plan depends on (checked)

- **Map 82 is authored but uncommitted.** Its S corridor is x 6-10, y 17.2-23, open to the S seam. Its notes already teach the rule:
  - 81: "THEY FOLLOW THE TORCH."
  - 82: "IT LEANS TOWARD WHOEVER IS HOLDING THE TORCH."
- **Without a gate in 84, the purge can be skipped.** The route 82 S → 84 → 83 W is open.
- **76 (Ward C South)** has its W seam at y 15-19, faces at y 15 and 19, floor lines at y 16.6 and 17.4 from x 0 to 7, and the title card "WARD C" at x 0-5, y 15-19. Its comment reads "the service corridor from Lab 4: the way they brought her". Admissions reads "02:58 ONE TIER 2 FROM ENERGY DIAGNOSTICS ... OBS 6".
- **Timeline anchors already in the game:**
  - Reactor scram 01:47 and surge alarm 01:58.
  - Mail relay "SENT 02:19".
  - Band reissue 02:14 amber, 02:15 red, "BY ORDER" (72).
  - Skull film 02:31 (73).
  - Cubicle 02:41, recount 02:44, escort to Ward C 02:50 by "DETAIL 4" (71).
  - Investigator at Sample Storage 03:05 (80).
- **Prologue chat format:** Sound::Exit (index 27), gain .35, LAB 4's lines at pitch 1.4 and YOU at 1.1. The player was sent to the service lift, which fell.
- **`med_override` is set nowhere yet.** It is required by 71's S door and by 71:488.
- **Investigator voice:** measured, no contractions ("Then you will not be busy."). Voice keys use the `inv_*` prefix.
- **Engine hooks that exist:**
  - `World::lightRayClear` is a static occluder BVH. It has no door check.
  - `World::rayClear(a,az,b,bz,doors,dynamic)`.
  - `traceShot(origin,dir,range,...)`.
  - `NpcWeaponPose.flash`.
  - `lampPower` and `lampSwitchedOn`.
  - `startGuardCaution(size_t)`.
  - `ScriptAction::SpawnEnemy`.
  - `Hazard.enabledFlag`.
  - Prone hull .58 m.
  - `migrateEnemiesAcrossChunks()`.
  - `SaveVersion=19`. Saves.cpp:111 caps the creature kind by save version.
- **Places that hard-code the last creature kind (`Transformed`):**
  - main.cpp lines 295-296 (`kindNames`, which also lacks "transformed"), 1416 and 1429.
  - CustomCampaign.cpp:375.
  - Game.cpp:746 (spawn list) and Game.cpp:82 (1.5x damage while recovering).
  - SoftwareRenderer.h:167 and 401.
  - CreatureAnimation.h.
- **No hazard code applies damage to creatures.** It appears to be player-only (Script.cpp:147). Verify this.
- **mdGuard, mdBody and mdSequence** live in CampaignChapterMedicalFront.cpp. Promote them to CampaignKit.h (`kit::guard`, `kit::body`, `kit::sequence`) rather than copying them. That follows the rule to extend existing systems.

## 2. Story for these two maps (one canon timeline)

| Time | Event | Where it shows |
|---|---|---|
| 01:44-01:47 | The prologue chat. She sends the player to the lift. | Prologue |
| 01:50 | R-47 is lent from Field Measurement for her bench test. | 84 sign-out |
| 01:58:04 | Every cell on her bench, the cell store and the Level 9 face sensor move on one trace. | 83 printout |
| 02:02 | Dunne records tape 46-11 in 84. | 84 tape |
| 02:10 | Dunne goes into the pan for his second meter and is killed. Her palm reads 0.11 cell equivalents on the array, mirrored to I.I. | 84 note, 83 letter |
| 02:15 | Her band is reprinted red. | 72 (already in the game) |
| 02:19 | Queued message "go up". | 35 (already in the game) |
| 02:20 | The Diagnostics detail arrives through 81 to hold Lab 4 and box her desk. | 83 order |
| 02:22 | The chemistry vent seals 81's S door and 84's airlock. The detail is trapped. | 81, 84 |
| 02:24 | Medical takes her out through the service door. Field Measurement opens the lamp bus. R-47 is left loose in Lab 4. | 84 bus panel |
| 02:27-02:28 | Lab 4 circuits are isolated. The arrays restart on no feed. | 83 night order, array log |
| 02:31 / 02:41 / 02:44 | Imaging, then Screening. | 73, 71 (already in the game) |
| 02:50 | Hale (Detail 4) escorts her. | 71 (already in the game) |
| 02:52-02:55 | Back through Lab 4 for her things. Her last draft. Hale's note. | 83 |
| 02:56 | Her session is logged out by I.I. | 83 drafts |
| 02:58 | Admitted to OBS 6. | 76 (already in the game) |
| 03:02 | I.I. pulls the survey file. | 84 mirror |
| 03:20 | Hale is ordered back to clear her desk. | 83 order |
| 03:24 | Hale goes in through the service door with his hand lamp. He dies by the door. The disk is on him. | 83 reader log |
| Now | The Investigator holds the trapped detail in the hall. "Nobody goes in." | 83 radio |

## 3. Map 84: FIELD MEASUREMENT (classroom and spoke)

**Setup**
- `makeFieldMeasurement()`.
- f=-12, roof=-7 (5 m). Surfaces Concrete. Ambient .14.
- playerStart {8,1.6}, yaw kPi*.5, spawnHeight -12.
- `openEdge N{6,10,f,false,0}` and `E{9,14,f,false,0}`.
- `machineryClock("anl_fm_clock",60000)`.
- Perimeter faces: (1,.9,6,1), (10,.9,23,1), (.9,1,1,23), (22.9,1,23,9), (22.9,14,23,23), (1,22.9,23,23).
- Its role: teach the rule. It is a dead end until the purge. Its reward is `anl_fm_bus`, which powers Lab 4's bench rig. After the purge, its airlock is a second way into 83, behind the guards.

### Rooms (x east, y south)

**1. ENTRY CORRIDOR, x 6-10, y 0-4.8**
- Walls x 5.8-6 and x 10-10.2. Each has an officeDoorway at y 2.2-3.4 (west to the kit room, east to the office).
- Soffit `wall(6,0,10,4.8,f+3,roof,3)` and `ceilingGrid`.
- Lamp (8,2.4) at f+2.95: .55, range 6, 4400 K.

**2. KIT ROOM, x 1-5.8, y 1-4.8**
- Slab f+3 to roof. South wall at y 4.8-5.
- Three psx_lockers on the north wall at x 1.6, 2.46 and 3.32, y 1.33, yaw -kPi*.5. They are hideable.
- lab_clean_bench at (3.4,4.4), anN, carrying:
  - 3 lab_field_meter.
  - 2 item_walkie.
  - 4 item_dosimeter at x 2.6, 2.95, 3.65 and 4.0. The gap at 3.3 is her kit. It has no text.
- Survey board: lab_notice_board at (1.04,3.0), f+1.25, anE, with F2 taped on it.
- Terminal F1 at (5.4,4.2).
- Lamp (3.4,2.9), .5.
- Health at (2,4.4).

**3. TECHS' OFFICE, x 10.2-22.9, y 1-4.8**
- Slab f+3.
- workstation(13,2.2,f,1,3), workstation(16,2.2,f,1,1) and workstation(20.6,2.2,f,1,0). F3 (the mirror) is on the last one.
- lab_notice_board at (18.6,1.04), f+1.3, anS, with F4 taped beside it.
- Lamp (16.5,2.9), .5.
- Ammo at (22.2,4.2).

**4. GALLERY, x 1-22.9, y 5-8.8**
- `ceilingGrid` at f+2.8 and `wall(1,5,22.9,8.8,f+2.8,roof,3)`.
- South wall at y 8.8-9, in order:
  - `wall(1,8.8,2.2,9)`.
  - `glazing(2.2,8.8,15,9,f,roof,3,.95,2.4,true)`: protective, holds.
  - `wall(15,8.8,15.4,9)`.
  - **PAN DOOR** `anSwing(15.4,16.6,y 8.9,swing +1,no lock)`.
  - `wall(16.6,8.8,19.4,9)`.
  - **AIRLOCK DOOR** `anSwing(19.4,20.6,y 8.9,swing +1,requireState lab_chem_seal)` with a wired vision panel.
  - `wall(20.6,8.8,22.9,9)`.
- Dunne's desk: `workstation(9.5,7.6,f,1,1)`. He faced the glass.
  - His hand lamp: kit::prop mine_cage_lamp at (8.9,7.85,f+.75), plus a Bare light at f+1.0: .7, range 5, 5600 K, `enabledState anl_fm_desk`, period .16, onTime .09, prop set to that index. It is a cycling lamp, so it counts as a lure.
  - Tape terminal F5 on the desk.
  - Recorder model: library-import the purchased voice recorder. If that is skipped, use lab_field_meter.
- Lamp test console: lab_control_board at (13.4,5.06), f+1.0, anS, with toggle terminal F7 at (13.4,5.45).
- Two dim tubes at (5,7) and (19,7): .3.
- Note F8 taped at (15.2,8.79), f+1.45, anN.

**5. CALIBRATION PAN, x 1-17.8, y 9-23, 5 m clear, no working ceiling light**
- **Basin:** floorLine material 22 over x 5.6-14.4, y 11.6-20.6.
  - A 0 dps Anomaly shimmer over the whole basin.
  - Four 3 dps Anomaly boxes: (5.6,11.6,10,16.1), (10,11.6,14.4,16.1), (5.6,16.1,10,20.6) and (10,16.1,14.4,20.6), from f to f+2.2.
  - `anHum("anl_fm_hum",5.4,11.4,14.6,20.8,f,f+2.5,.5)`.
  - A dry perimeter is left on all four sides.
- **Reference stand** in the water at (10,16.1): lab_clean_bench anN with 4 anl_cell_charged at f+.81, a violet Bare light (.3, range 4, 9500 K, tint -.4, static) and F9, her field book, as a Letter at surface .81.
- **WEST WALK rig:** `anWalkingLamps(m,"anl_fm_west",{(3.2,21.8),(3.2,18.2),(3.2,14.6),(3.2,11.0)},f,f+1.66,4.f,1.f)`. The light walks north up the west side, about 1 s per stake.
- **Dunne:** `kit::body(m,"anl_fm_clock",ActorVisual::LabTech,{4.4,20.9},f,.3)`, a blood decal, and his torch prop at (5.2,21.6).
- Pickups: HandgunAmmo (3.4,22.2), Ammo (5.6,22.3), Health (16.6,10.2).
- **East wall x 17.8-18:**
  - y 9-14: the airlock's west wall: `wall(..,9,..,9.6)`, `glazing(17.8,9.6,18,13.4,..,true)`, `wall(..,13.4,..,14)`.
  - y 14.2-15.4: open, the bus walk.
  - y 15.6-23: the bus room's west wall, with `glazing(17.8,17,18,20.6,f,roof,8,1.0,2.0,true)`.

**6. AIRLOCK, x 18-22.9, y 9-14**
- Open east to the seam, so it lines up with 83's faces at y 9 and 14.
- South wall `wall(18,14,22.9,14.2)`. Soffit at f+3.
- Amber lamp (20.5,11.5) at f+2.95: .45, 3200 K. Decontamination floor lines.
- Through the seam: 83's dark west aisle.

**7. BUS WALK and CHAMBER 3, "the readings never stopped", x 18-22.9, y 14.2-23**
- The bus walk is y 14.2-15.4, open to the pan.
- North wall at y 15.4-15.6 with **swinging door** `anSwing(19.6,20.8,y 15.5,+1)`. It has no lock, and the creature does not work doors, so this is a safe room.
- Chamber walls are material 8.
- **Bus panel** on the east wall: lab_knife_switch at (22.85,18.6) and (22.85,19.0), f+1.5, anW; pwr_ammeter_box at (22.85,19.6). Terminal F10 at (22.6,19.2) has `activateState anl_fm_bus`.
- Live needle: lab_scope_monitor on med_crash_cart at (21,16.4), with terminal F11.
- bench(20.2,21.6). Sticky F12 taped at (18.01,21.6), f+1.4, anE.
- A red lamp over the door inside, .12. The dead tube has a fitting only.
- Pickups: HandgunAmmo (22.2,22.2), Health (18.8,22.4).

**Signs: 2 of 3.**
- `wallSign(20.0,8.79,f+2.45,"ZONE 3 AIRLOCK","HELD ON THE CHEMISTRY READING. PURGE AT INSTRUMENTATION",2.6,anN)`.
- `wallSign(16.0,8.79,f+2.6,"CALIBRATION PAN","LIVE WATER. NO LIGHTS IN THE PAN",2.f,anN)`.

**Creature:** `CreatureSpawn{CreatureKind::Pointer,{4.9f,20.3f},-999,1,0,stateId("anl_fm_wake")}`. R-46 lies dormant on Dunne and plays the eating clip.

### Beats
1. **Kit room.** The survey board puts her name in a column of cell equivalents. The dosimeter gap is hers.
2. **Gallery.**
   - Through the glass, in violet stand-light, something crouches over a body in the far south-west.
   - Event `anl_fm_feed_go` fires once on the gallery box (2,5.2,17.8,8.8) and starts `anl_fm_feed`: JunkSoft every 2.5 s at (4.6,20.8), gain .08, for 40 s.
3. **The tape** (sequence below). The rule happens live in front of the player:
   - the west lamps walk;
   - R-46 rises and follows;
   - the lamps die, and it stops at the north-west stake;
   - Dunne's hand lamp beside the player stutters for 2.2 s;
   - it crosses the pan and crouches at the glass in front of the player, "where my hand was".
   - If the player's beam moves at the glass, it lunges into the glass. The glass holds.
4. **Agency.** F7 runs the west walk at will, so R-46 paces the west side. The player then crosses the east perimeter to Chamber 3, throws the bus and shelters behind the door.
5. **After the purge,** the airlock opens. The player looks through its west glass at the pan, then steps through the seam into 83's west aisle, behind the detail.

## 4. Map 83: ENERGY DIAGNOSTICS / LAB 4

**Setup**
- `makeEnergyDiagnosticsLab4()`.
- f=-12, roof=-8.5. Wall SurfaceLabPanel; floor and ceiling SurfaceConcrete. Ambient .14.
- playerStart {12,1.6}, yaw kPi*.5.
- Edges:
  - `openEdge N{10,14,f,true,0}`: entry door 0. 81's S transfer door already holds `lab_chem_seal`.
  - `W{9,14,f,false,0}`.
  - `E{15,19,f,false,0}`.
  - `S{10,14,f,false,0}`.
- `machineryClock("anl_diag_clock",60000)`.
- Perimeter faces: (1,.9,10,1), (14,.9,23,1), (.9,1,1,9), (.9,14,1,23), (22.9,1,23,15), (22.9,19,23,23), (1,22.9,10,23), (14,22.9,23,23).

### Rooms

**1. VESTIBULE, x 9.2-14.8, y 1-4.8 (entry)**
- West wall: `wall(9,1,9.2,3.2)`, `officeDoorway(9,9.2,3.2,4.4,f,roof)`, `wall(9,4.4,9.2,4.8)`.
- East wall: `wall(14.8,1,15,2.6)`, `officeDoorway(14.8,15,2.6,3.8,f,roof)`, `wall(14.8,3.8,15,4.8)`.
- Front onto the hall: `glazing(9.2,4.8,11.4,5,f,roof,3,.95,2.4)`, a mouth at x 11.4-12.6 with header `wall(11.4,4.8,12.6,5,f+2.4,roof,3)`, and `glazing(12.6,4.8,14.8,5,...)`.
- Two psx_lockers at (9.53,1.5) and (9.53,2.36), yaw 0.
- Lamp (12,2.8): .4, range 6, 4600 K, flicker .15.
- R1 taped at (14.79,1.8), f+1.45, anW.
- HandgunAmmo at (13.9,1.6).

**2. NW OFFICE, x 1-9, y 1-4.8: the detail's packing room**
- South wall: `wall(1,4.8,6.6,5)`, `officeDoorway(6.6,7.8,4.8,5,f,roof)`, `wall(7.8,4.8,9,5)`.
- workstation(2.4,2.0,f,1,0) and workstation(6.2,2.0,f,1,3).
- Boxes: prop_cardboard_box_open at (3.6,3.6) and (4.6,3.7) holding office_file_binder props.
- Four office_pc_tower props lying on their sides at about (5.2,3.2): her drives.
- `paperDrift(1.2,1.2,8.8,4.6,f,12,8301)`.
- The only light: lab_desk_lamp prop at (6.7,1.8,f+.75) with a prop-bound Bare light (.45, range 4, 3000 K). It is still, so it is safe.
- item_walkie at (5.9,2.1,f+.75): the detail's handset.
- R2 as a Letter on the (2.4,2.0) desk, surface .75.
- Ammo at (2.9,2.3,f+.75).

**3. NE OFFICE, x 15-22.9, y 1-4.8**
- lab_photocopier at (16.2,1.5), anS. R3 is a Letter on it, surface .97.
- Two psx_lockers at (21.3,1.33) and (22.16,1.33), yaw -kPi*.5.
- Lamp (19,2.8): .4.
- Health at (18.4,3.8).
- South wall, which is Lab 4's north wall: `wall(15,4.8,21.4,5)`, `wall(21.4,4.8,22.4,5,f+.7,roof,3)` (**the cable pass**: prone only, 0.7 m, which neither the guards nor R-47 can use), `wall(22.4,4.8,22.9,5)`.
- A pwr_vent_grille leans off it at (21.9,4.45). It is not solid.

**4. DIAGNOSTICS HALL, x 1-15, y 5-14.8. The circuits are isolated, so no ceiling lamps, only `ceilingGrid`.**
- **Array island:**
  - North row: anl_switch_cabinet_1 anN at x 6.2, 7.45, 8.7 and 9.95, y 8.27.
  - South row: the same x values, anS, at y 10.53.
  - End cap: `wall(10.55,8.6,11.0,10.2,f,f+2.6,2)`.
  - **Hiding slot:** y 8.6-10.2, x 5.6-10.55. It is open at the west end, unlit, with a floorLine material 8 trench.
  - `cableTray({1.2,7},{14.8,7},f+3.1)`.
- **The arrays are logging on no feed.** lab_status_display at x 7.45 and 9.95, y 7.92, f+1.6, anN, each with a Bare light (.18, range 2.5, 6500 K, flicker .1). Static light: it keeps what it touches.
- **Still wet metal:**
  - Burst main: `servicePipe({1.2,12},{11.3,12},roof-.35,.06,4)` with the break at x 11.4, plus a Metal1 drip every 7.5 s.
  - floorLine 22 over x 11.6-15, y 11.6-14.8, with a lobe at (11.0,12.4,11.6,14.2).
  - Two 3 dps Anomaly boxes: (11.6,11.6,13.3,14.8) and (13.3,11.6,15,14.8).
  - `anHum("anl_diag_hum",11.2,11.2,15,14.8,f,f+2.5,.5)`.
- **Red emergency wall lamps** (1000 K, tint -.5): at (1.1,6.2), f+2.4, facing east, .18; and at (8,14.75), f+2.4, facing north, .15.
- The W seam (y 9-14) opens into the west aisle.

**5. LAB 4, x 15.2-22.9, y 5-14.8**
- **West wall:** `wall(15,5,15.2,6)`, `officeDoorway(15,15.2,6,7.2,f,roof)`, `wall(15,7.2,15.2,7.6)`, `glazing(15,7.6,15.2,14.4,f,roof,3,.95,2.4,true)`, `wall(15,14.4,15.2,14.8)`.
- **Door reader:** lab_id_scanner at (15.0,7.45), f+1.2, anW, with terminal R4.
- **South wall:** `wall(15,14.8,19.4,15)`. **SERVICE DOOR** `Door{19.4,20.6,14.9}`: swinging, swing +1, maxOpen .9, `requireState med_override`, with `wall(19.4,14.8,20.6,15,f+2.55,roof,3)` above. Then `wall(20.6,14.8,22.9,15)`.
- `titleCard(m,"LAB 4",15.2,5,22.9,14.8)`.
- **Her desk (north):**
  - `workstation(19.2,5.42,f,-1,2)`. Whoever reads it stands at about y 6.5 with their back to the room.
  - Desk lamp: kit::prop lab_desk_lamp at (19.85,5.3,f+.75).
    - Light A: static. Bare, .4, range 3.2, 3000 K, prop set to it.
    - Light B, the gutter: Bare, .55, range 4.5, `enabledState anl_l4_gutter`, period .16, onTime .09, same prop.
  - Terminal R8.
  - Her box on the floor at (21.0,5.6): prop_cardboard_box_open with item_dosimeter, a mug and office_file_binder inside. It also hides the cable-pass exit.
- **Drive rack:** anl_switch_cabinet_2 at (22.45,7.6), anW, with three office_pc_tower props pulled onto the floor beside it.
- **Her printout:** lab_notice_board at (22.86,9.6), f+1.3, anW, with R5.
- **Cut-cell bench:**
  - lab_clean_bench at (22.3,11.6), anW, with lab_microscope, anl_cell_core upright, lab_anomaly_relic at scale .25 and med_specimen_jar.
  - Violet Bare light: .22, range 2, 9000 K, tint -.4.
  - R6 as a Letter on the bench, surface .81. R7 as a sticky taped at (22.89,11.6), f+1.4, anW.
- **The pen:** med_cell_bars bays at x 15.4-17.9, y 12.1-14.6 (the 79 pattern), with the gate swung open on the east side. floorLine 22 inside.
- **Her bench rig:** `anWalkingLamps(m,"anl_l4_rig",{(21.6,10),(19.8,10),(18,10),(16.2,10)},f,f+1.5,4.8f,1.2f)`.
  - Toggle terminal R10 at (16.4,5.25), facing into the room, `requireState anl_fm_bus`.
  - Her sticky R11 taped at (15.21,7.4), f+1.5, anE.
- **Hale:**
  - `kit::body(m,"anl_diag_clock",ActorVisual::CleanupMedic,{20.1f,14.1f},f,<facing the door>)`.
  - Blood `wall(19.3,13.3,21,14.7,f+.004,f+.01,16)` and a hand smear `wall(20.7,14.79,21.3,14.8,f+.9,f+1.5,16)`.
  - His walkie at (19.6,13.6).
  - **His hand lamp, still burning:** propPose mine_cage_lamp at (19.3,13.4,f), pitch kPi/2, with a Bare light (.5, range 3.5, 5600 K, prop set to it). It is static, so R-47 stands in it.
  - R12 order and R13 disk as Letters at surface 0, at (20.9,13.8) and (20.5,14.4).
  - R14 taped on the door jamb at (19.2,14.79), f+1.45, anN.
  - Service reader: lab_id_scanner at (21.0,14.78), f+1.2, anN, with terminal R15.
  - HandgunAmmo at (18.9,14.4).
- **Lab 4 light:** desk lamp, screen, Hale's lamp, the cut-cell violet, and a red wall lamp at (22.85,13.4) facing west, .15. Nothing else.

**6. SERVICE CORRIDOR, x 14.2-24, y 15-19, to the E seam**
- West wall x 14-14.2. South wall `wall(14.2,19,22.9,19.2)`. Fill `wall(14.2,19.2,22.9,23,f,roof,3)`.
- floorLine material 8 at y 16.6 and 17.4 from x 14.2 to 24, continuing 76's.
- Lamp (19,17), roof-.05, .4, 5000 K, `enabledState med_blackout`, invert. It dies with the ward.
- Red lamp at (14.25,17) facing east, .12.
- `extinguisher(14.2,16.2,f,anE)`.

**7. SOUTH PASSAGE, x 10-14, y 15-23, to 85**
- Walls (9.8,15,10,23) and (14,15,14.2,23). Mouth header `wall(10,14.8,14,15,f+2.6,roof,3)`.
- **Back door** `Door{10,14,19.1}`, not swinging, `requireState res_lab_unsealed`, with `wall(10,19,14,19.2,f+2.6,roof,3)` above.
- Red lamp at (12,18.9), f+2.4, facing north.
- R16 taped at (12.6,18.99), f+1.45, anN.
- Ammo at (13.4,17.6).

**8. ARRAY POWER, x 1-9.8, y 15-23**
- `officeDoorway(5.4,6.6,14.8,15)` from the hall. The hall's south wall: `wall(1,14.8,5.4,15)`, `wall(6.6,14.8,10,15)`, `wall(14,14.8,15,15)`.
- Three anl_switch_cabinet_2 at (2.6,22.5), (4.6,22.5) and (6.6,22.5), anN, with lab_knife_switch props.
- R17 taped at (1.01,18.6), f+1.45, anE.
- Lamp (5,19): .35, flicker .25.
- Ammo at (8.8,16), Health at (1.8,21.6).

**Signs: 2 of 3.**
- `wallSign(20.0,14.78,f+2.62,"WARD C / SERVICE DOOR","MEDICAL OVERRIDE ONLY",1.8,anN)`.
- `wallSign(12,18.98,f+2.75,"TEST COMPLEX PASSAGE","SEALED FROM THE RESONANCE LABORATORY SIDE",2.4,anN)`.

**Zones:** "ENERGY DIAGNOSTICS / LAB 4", "ENERGY DIAGNOSTICS / SERVICE CORRIDOR" and "ENERGY DIAGNOSTICS". Then `recessedFittings(m)`. There are no dead lamps; about 23 lights in total.

### Guards: the Diagnostics detail
Two men, Hostile and breaking. They have been trapped since 02:22. They obey the rule: no torches, standing in still light.
- Use `kit::guard` (mdGuard promoted to CampaignKit.h).
- Squad "anl_diag_detail". Dead and damage states: `anl_diag_a/_b_dead` and `_damage`.
- Look bearings below use the actor heading convention (atan2: east 0, south kPi*.5, west kPi, north -kPi*.5). The `an*` constants are model yaws and do not apply here.

| Guard | Visual / role | Weapon, sight, speed, health | Posts |
|---|---|---|---|
| **A, the packer** | Cleanup, Assault | weapon 1, sight 9, speed 1.0, 100 hp | (4.0,3.4), 16 s, facing north into the boxes, back to the hall doorway; (7.2,5.8), 5 s, facing east down the north aisle |
| **B, the walker** | CleanupSharpshooter, Sharpshooter | weapon 2, sight 11, speed 1.0, 90 hp, `dropsSidearm` | (3.2,6.4) 3 s east; (13.4,6.4) 4 s south, with Lab 4's doorway at his left shoulder; (13.2,10.6) 2.5 s west; (3.2,12.2) 3.5 s west, watching the 84 seam |

No dialogue. 71 already has the talking man.

## 5. Route and gating

**First visit.**
1. 82, running the purge on the way, then the S corridor into 84. 84 is a dead end: its airlock is held on `lab_chem_seal`. The player takes the tape, the rig and optionally the bus (`anl_fm_bus`).
2. The purge sets `lab_chem_seal`.
3. Into 83 either through 81's S door (into the vestibule, in front of the detail) or through 84's airlock (into the hall's west aisle, behind them).
4. Three ways into Lab 4:
   - the hall doorway at (15, y 6-7.2), past B;
   - the hiding slot in the array island;
   - the vestibule east door to the NE office, then the prone cable pass into Lab 4's north-east corner.
5. In Lab 4: the drafts, the guttering lamp pulls R-47 away from Hale, the disk sets `med_override`, the service door opens, the corridor, then the E seam into 76.

`med_override` also opens 71's S door, as already designed.

**Return.** 85 sets `res_lab_unsealed`, which opens the back door. 83 then joins the Energy Test Complex to Ward C and Analysis.

**Closures that must be tested:**
- From 82 without the purge, the player can reach 84 but not 83.
- The duct and cable pass never reach the service corridor.
- The service door is the only way between Lab 4 and the corridor.
- `med_override` is set only by R13.

## 6. Readables

Titles and system text are in capitals; her own writing is lowercase.

**Map 84**

| # | Placement | Title | Line 1 | Line 2 |
|---|---|---|---|---|
| F1 | Terminal | FIELD KIT / SIGN-OUT | 01:50 LAB 4: R-47 ON LOAN FOR THE BENCH TEST. 1 HAND LAMP. IN HER HAND: BACK BY THREE. | 02:05 DUNNE: 2 METERS, 1 RECORDER, CHASE RIG KEY. NOTHING SIGNED BACK IN. |
| F2 | Note, taped | BODY SURVEY / THIS WEEK | LAB 4 (ENERGY DIAG.): MON 0.03, WED 0.04, FRI 0.06 CELL EQ. TIER 2 STOPS AT 0.05. SCREENING DUE. | PER I.I. FORMAT: ALL STAFF FIELDS IN CELL EQUIVALENTS. NAMES ON THE COPY THAT GOES UP ONLY. |
| F3 | Terminal | SURVEY / MIRROR TO I.I. | EVERY READING COPIED TO INTERNAL INVESTIGATIONS AS TAKEN. 214 STAFF ON FILE. | 03:02 FILE CALLED UP IN FULL BY I.I. 03:02 LOCAL COPY DELETED. |
| F4 | Note, taped | PAN MAP / 01:50 | R-46 TURNS TO A LIGHT BEFORE THE METERS DO. ONLY A LIGHT THAT MOVES. THE PLANT'S OWN LAMPS IT IGNORES. | WE STOPPED CALIBRATING THE METERS. WE CALIBRATE THE DOG. |
| F5 | Terminal, `activateState anl_fm_tape` | FIELD RECORDER / TAPE 46-11 | 02:02 / DUNNE. LAST TAPE IN THE MACHINE. | E / PLAY. |
| F7 | Terminal, toggle `anl_fm_west` | FIELD LAMPS / WEST WALK | CHASE RIG, STAKES 1-4. THE LIGHT WALKS THE WEST SIDE OF THE PAN AND STARTS AGAIN. | E / RUN. E AGAIN / STOP. |
| F8 | Note, taped | PAN ENTRY / 02:10 | DUNNE WENT IN FOR HIS SECOND METER WITH THE TORCH IN HIS TEETH. R-46 IS OUT OF ITS PEN. | NOBODY GOES IN. IF YOU HAVE TO, GO DARK AND GO SLOW. IT HEARS BOOTS. |
| F9 | Letter, on the stand | FIELD BOOK / LAB 4 | light that moves draws the big ones. still light does nothing. dark does nothing. they don't see us, only what we carry. | so the plant's lamps are safe and our torches aren't. lamps that walk would move them without a hand on them. ask dunne for stakes. |
| F10 | Terminal, `activateState anl_fm_bus` | FIELD LAMP BUS / CHAMBER 3 PANEL | BUS OPENED BY HAND 02:24 WHEN MEDICAL TOOK LAB 4. HER BENCH RIG DEAD SINCE. | CLOSE TO RESTORE: ENERGY DIAGNOSTICS, LAB 4 BENCH RIG. |
| F11 | Terminal | CHAMBER 3 / LIVE READING | READING RISING SINCE 01:58. MAINS ISOLATED 02:40. READING STILL RISING. | THE NEEDLE LEANS TOWARD WHOEVER WALKS PAST WITH A TORCH. |
| F12 | Sticky | CHAMBER 3 / SIT STILL | AN HOUR AND A HALF IN HERE WITH THE LIGHTS OFF. IT CAME TO THE WINDOW ONCE, WHEN I CHECKED MY WATCH. | THE WATCH HAS A LIGHT. I HAVE TAKEN IT OFF. |

**Map 83**

| # | Placement | Title | Line 1 | Line 2 |
|---|---|---|---|---|
| R1 | Note, taped | DIAGNOSTICS / NIGHT ORDER | LAB 4 CIRCUITS ISOLATED AT THE PANEL 02:27 BY ORDER OF I.I. THE ARRAYS RESTARTED 02:28 ON NO FEED. LEAVE THEM. | NOBODY INTO LAB 4 WITH A TORCH ON. NOBODY INTO LAB 4. |
| R2 | Letter | I.I. / RECOVERY ORDER / 02:20 | DIAGNOSTICS DETAIL: HOLD ENERGY DIAGNOSTICS. LAB 4 STATION DRIVES, HER PAPERS, ALL PRINTED READINGS, BOXED FOR I.I. | THE ANALYST GOES TO MEDICAL WITH THE ESCORT. YOU DO NOT SPEAK TO HER. |
| R3 | Letter | ARRAY LOG / CONTINUOUS | 01:58:04 ARRAYS 1-6 OFF SCALE. 02:20 FEEDS PULLED. 02:21 STILL OFF SCALE. | 02:27 MAINS ISOLATED. 02:28 STILL LOGGING. THE PAPER COMES OUT WARM. |
| R4 | Terminal | DOOR READER / LAB 4 | 02:24 OUT: LAB 4, ESCORTED. 02:52 IN: LAB 4, R. HALE. 02:55 OUT: LAB 4, R. HALE. | 03:24 IN: R. HALE. NO OUT. *(See decision 3 for an optional extra entry.)* |
| R5 | Note on the board; second line in her pen | ARRAY / 01:58:04 / PRINTOUT | DEEP FACE SENSOR (LEVEL 9). CELL BENCH, 14 CELLS. CELL STORE, INSTRUMENTATION. ONE TRACE. | zero delay over 900 m. nothing travelled. it was already here. |
| R6 | Letter | ARRAY / CONTACT READ / 02:10 | PALM CONTACT, ANALYST'S OWN HAND: 0.11 CELL EQ. TIER 2 STOPS AT 0.05. | MIRRORED TO I.I. AS TAKEN. 02:15 REPLY: SCREENING. BAND RED. NO NAME GIVEN. |
| R7 | Sticky | ON THE CUT CELL | same. | *(empty)* |
| R8 | Terminal, `activateState anl_l4_drafts` | LAB 4 / DIRECT LINE: SECURITY DESK 2 | 7 DRAFTS NOT SENT. 1 QUEUED 02:19: CARRIER LOST. | E: OPEN THE DRAFTS. |
| R10 | Terminal, toggle `anl_l4_rig`, requires `anl_fm_bus` | BENCH RIG / WALKING LAMPS | FOUR STAKES ON THE FIELD LAMP BUS. BUS OPENED AT FIELD MEASUREMENT 02:24. | E / WALK THE LAMPS. E AGAIN / STOP THEM. |
| R11 | Sticky | ON THE RIG | it follows light that walks and stops where the light stops. | it has never once looked at me. |
| R12 | Letter | DETAIL 4 / ORDER 03:20 | R. HALE: RETURN TO LAB 4 BY THE SERVICE DOOR. CLEAR THE ANALYST'S DESK. ALL MEDIA TO I.I. BY HAND. DO NOT READ IT. | THE DIAGNOSTICS DETAIL WILL NOT GO IN. YOU HAVE THE DISK. |
| R13 | Letter, `activateState med_override` | WARD C OVERRIDE / R. HALE | DEPTHWORKS MEDICAL. EVERY MEDICAL LOCK ANSWERS IT: THE SERVICE DOOR, THE WARD, SCREENING SOUTH. | E / TAKE THE DISK. |
| R14 | Note, taped | ESCORT / LAB 4 / 02:55 | COLLECTED FROM SCREENING 02:50. VIA LAB 4 FOR HER THINGS, AS SHE ASKED. THROUGH TO WARD C. | SHE LEFT HER BOX ON THE DESK. I SAID I WOULD BRING IT DOWN. -R. HALE |
| R15 | Terminal | WARD C SERVICE DOOR / READER | MEDICAL OVERRIDE ONLY. EVERY OPENING LOGGED TO I.I. | LAST: 03:24 IN, R. HALE. |
| R16 | Note | BACK DOOR / RESONANCE LAB | SEALED FROM THEIR SIDE AT 01:58. IT OPENS WHEN THEY OPEN THEIRS. | IF YOU ARE READING THIS FROM IN HERE, THEY HAVEN'T. |
| R17 | Note | ARRAY POWER | MAINS ISOLATED 02:27. ARRAYS 1-6 LOGGING ON NOTHING. | BREAKERS LEFT OPEN. I AM NOT CLOSING THEM TO SEE WHAT HAPPENS. |

## 7. Sequences

All captions; voices come later through the pipeline.

**`anl_fm_tape`** (84)
- Sequence: 32000 ms, finishAt 31000, at the desk (9.6,7.8,f+1).
- Event `anl_fm_tape_go` (requires `anl_fm_tape`, once) sets `anl_fm_tape`, `anl_fm_t1`, `anl_fm_t2`, `anl_fm_t3` and `anl_fm_t4` to 1.
- Relays, as script-only sequences built with `kit::sequence`:

| Timer | Fires at (ms) | Finish state | Then |
|---|---|---|---|
| `anl_fm_t1` | 6000 | `anl_fm_t1_go` | Event sets `anl_fm_west` 1 and `anl_fm_wake` 1 |
| `anl_fm_t2` | 10000 | `anl_fm_west_stop` | Event sets `anl_fm_west` 0 |
| `anl_fm_t3` | 17000 | `anl_fm_desk` | (the finish state is the lamp's own state) |
| `anl_fm_t4` | 19200 | `anl_fm_desk_stop` | Event sets `anl_fm_desk` 0 |

- Captions (Sound::Metal1 .15; the end cue uses Machine .2):

| Time (ms) | Caption |
|---|---|
| 400 | TAPE 46-11. 02:02. DUNNE, FIELD MEASUREMENT. R-46 IN THE PAN. R-47 IS UP IN LAB 4 FOR HER TEST. |
| 5800 | DUNNE (TAPE): West lamps walking. |
| 8600 | DUNNE (TAPE): It's up. It's going with them. Like a dog after a ball. |
| 14000 | DUNNE (TAPE): Lamps off. It stops where the light stopped. It doesn't look for me. |
| 16400 | DUNNE (TAPE): Hand lamp on. Two seconds, for the log. |
| 19300 | DUNNE (TAPE): Off. Off. |
| 22500 | DUNNE (TAPE): It came to the glass. It's sitting where my hand was. |
| 27500 | DUNNE (TAPE): Note for Lab 4. Her rule holds. It doesn't see us. It sees what we carry. |
| 31000 | TAPE 46-11: END. |

**`anl_diag_radio`** (83)
- Sequence: 22000 ms, finishAt 21000. Started by event `anl_diag_arrive` on the vestibule box (9.2,1,14.8,4.8), once.
- Cues play at the NW walkie (5.9,2.1,f+.8):

| Time (ms) | Cue | Voice key |
|---|---|---|
| 400 | RadioSquelch | |
| 800 | INVESTIGATOR (RADIO): Diagnostics detail. Report. | inv_diag_1 |
| 3800 | RADIO: Her drives are boxed. Paper's bagged. Nobody's been in the hall. | anl_diag_a_1 |
| 8000 | INVESTIGATOR (RADIO): And the orderly's disk. | inv_diag_2 |
| 11200 | RADIO: Hale's on the floor in Lab 4 by the service door. So's what put him there. It's standing over him, sir. | anl_diag_a_2 |
| 17000 | INVESTIGATOR (RADIO): Then the disk stays with him. Hold the hall. | inv_diag_3 |
| 20200 | RADIO: ...Copy. | anl_diag_a_3 |

**`anl_l4_drafts`** (83)
- Sequence: 48000 ms, finishAt 47000, finish state `anl_l4_drafts_done`. LAB 4 cues use Sound::Exit, gain .35, pitch 1.4, at (19.2,5.6).
- Event `anl_l4_drafts_go` (requires `anl_l4_drafts`, once) starts the relays:
  - `anl_l4_t1` fires at 39500 and sets `anl_l4_gutter`.
  - `anl_l4_t2` fires at 41900 and sets `anl_l4_gutter_stop`; event `anl_l4_gutter_off` then sets `anl_l4_gutter` 0.

| Time (ms) | Caption |
|---|---|
| 0 | SECURITY DESK 2 / 7 DRAFTS NOT SENT / 1 QUEUED |
| 2500 | LAB 4 (02:03, NOT SENT): the lift. tell me you made the lift. |
| 7000 | LAB 4 (02:06, NOT SENT): every cell on my bench moved at 01:58. the face sensor on 9 moved with them. same instant. |
| 12000 | LAB 4 (02:08, NOT SENT): nothing travels that fast. it isn't a signal. it's one thing, and we cut pieces off it. |
| 17500 | LAB 4 (02:11, NOT SENT): i put my hand on the array to steady it. it read me like a cell. |
| 23000 | LAB 4 (02:16, NOT SENT): they reprinted my band. red. nobody will say what red means. i feel fine. that's the part i don't like. |
| 28500 | LAB 4 (02:19, QUEUED): if you're alive don't come down to the labs. go up. |
| 33500 | LAB 4 (02:22, NOT SENT): field measurement's dog is still in my pen. it has never looked at me once. it watches my lamps. |
| 37500 | Sound::Machine .2 at the lamp: the buzz before the gutter |
| 39000 | LAB 4 (02:55, NOT SENT): hale's giving me two minutes for my things. if you come down anyway, keep your light still. it goes to the light, not to you. i'm sorry about the lift |
| 45500 | SESSION ENDED 02:56 / LOGGED OUT BY I.I. |

**`anl_l4_door_radio`** (83)
- Sequence: 15000 ms, finishAt 14000.
- Event `anl_l4_door_go` fires on the box (19,13.8,21,15.8) when it requires `med_override`, once. It sets the timer and a Checkpoint.
- Cues play at Hale's walkie:

| Time (ms) | Cue | Voice key |
|---|---|---|
| 300 | Sound::Door at (20,14.9), aiRadius 14. R-47 hears it. | |
| 900 | RadioSquelch | |
| 1300 | INVESTIGATOR (RADIO): Lab 4 service door. Opened on the Ward C disk. | inv_l4_1 |
| 5600 | INVESTIGATOR (RADIO): Detail four is lost. So it is not Detail four. | inv_l4_2 |
| 10200 | INVESTIGATOR (RADIO): Diagnostics detail. Whoever carries that disk is exposed. | inv_l4_3 |

- Optional event `anl_diag_order` (requires `anl_l4_door_radio_done`): **SquadCaution** on `anl_diag_detail`.

**Small things scatter.** These are once events that require `light_moving` (section 9). Each starts a 3 s sequence of SpiderCall cues stepping away from the player, at gain .25 falling to .08, at 0, 700, 1400 and 2100 ms.
- `anl_fm_scatter` covers the pan basin and runs toward (17.4,22.4).
- `anl_diag_scatter` covers the hall pool and runs toward the S passage.

**Other 84 wake events:**
- `anl_fm_glass`: box (2.2,6.4,17.8,8.8), requires `light_moving`, sets `anl_fm_wake`.
- `anl_fm_wake_in`: pan box (1,9,17.8,23), sets `anl_fm_wake`.

## 8. The new creature: the POINTER (R-46 and R-47, "the dogs")

**Asset.** `O:/retro official/Creatures/Nightmare-Creature-1-PSX-2112/` (the FBX and its 512 px texture), on its own rig. Bake it from its own takes with no retarget.
- `tools/BakePointer.py`, modelled on BakeSnatcher.py: Blender, scale to 2.0 m upright, ground on the bind floor.
- Then: `RawMetalAuthoredCreatureBake pointer.fbx pointer-animation.bin Pointer_Idle Pointer_Walk Pointer_Strike Pointer_Hit Pointer_Death Pointer_Crawl Pointer_ToCrawl Pointer_Lunge Pointer_Grab Pointer_Eat Pointer_Roar Pointer_Stand`.

| Index | Clip | Source take |
|---|---|---|
| 0 | idle (crouched, sitting in the light) | crawl_idol |
| 1 | walk (upright, investigating) | walk |
| 2 | strike | attack_1 |
| 3 | pain | hit_1 |
| 4 | death | death_1 |
| 5 | crawl | crawl |
| 6 | to crawl | state_to_crawl |
| 7 | lunge | jump |
| 8 | grab | crawl_bite |
| 9 | dormant | eating |
| 10 | first-contact tell | roar |
| 11 | upright idle (Search) | idle |

- Confirm the bake's clip cap. The Transformed uses 6 clips.
- Add assets.rc entries on the next free IDs after 2345, then `--creature-video=8`.

**Behaviour.** Its sight is replaced. It is blind to bodies and sees light that moves.

**Lit contact: `visible` for this kind.** Any one of these makes the player visible to it:
- The player's beam is moving and crosses it: the beam's segment passes within .6 m of its chest, or the trace hits it.
- A muzzle flash within 18 m.
- Touch range: within 1.2 m on the same level.

Rays use `World::rayClear(a,az,b,bz,doors=true,dynamic=false)`, which is blocked by doors and passes intact glass. Verify that `dynamic=false` skips breakables.

**Lures** apply only when it has no lit contact, in this priority:
1. A guard's muzzle flash.
2. The player's moving beam spot within 16 m.
3. Cycling lamps (`period>0`, `lampSwitchedOn`), choosing the one whose on-window began most recently, within 16 m.

It walks to the nearest standable point within 1.2 m of the light's floor point at 1.6 m/s, crawling. On arrival it crouches in Watch facing the light and sets `home` and `homeZ` there: it stays where the light left it.

**Hearing** is unchanged: footsteps, impacts, gunshots and cue `aiRadius`. It walks upright to the sound, searches, then returns home. "IT HEARS BOOTS."

**Modes**, reusing the existing StalkModes and the Snatcher's code:

| Situation | Mode | What it does | Clip |
|---|---|---|---|
| First lit contact | Watch | A 1.0 s tell: turns its head and roars once | roar |
| Still lit after the tell | Flank | Crawls straight at the target at 3.4 m/s | crawl |
| The light goes off or stops | Flank, then Watch | Goes to the last lit point and stands. It never seeks the player. | crawl |
| Within 3.5 m and lit | Rush | A .45 s crouch, then a 5 m/s lunge using the Snatcher's lunge sampling | lunge |
| Lunge connects | Grab | 1.4 s hold, two bites of 11. A hit with `painFlash>.85` knocks it off. | grab |
| Lunge misses, or knocked off | Recover | .9 s. Takes 1.5x damage, as in Game.cpp:82. | |
| Touch | Generic windup strike | 22 damage, whether lit or not | strike |

- It never opens doors. It never migrates across seams.

**Traits row**, appended as kind 8:
```
{"POINTER",240,1.85f,2.0f,1.0f,22,.45f,.34f,2,.78f,false,true,3.2f,4.5f,2.f,2.2f,1.2f,5.0f,.35f,1.8f,false,16.f}
```
Constants beside `TransformedLeap`: `PointerTell=1.f,PointerLunge=.45f,PointerHold=1.4f,PointerRecover=.9f`.

**Exact engine work**
1. **WorldDefinition.h:** append `Pointer` to `CreatureKind`. Append `float lightSight=0;` as the last `CreatureTraits` field; the aggregate default leaves the eight existing rows unchanged. Add the row above.
2. **EnemyAI.cpp:**
   - At the `visible` line (202): for `traits.lightSight>0`, compute the lit contact instead, and skip the friendly-NPC sight loop. Guards count only as muzzle-flash lures.
   - After the hearing block (around line 224): pick a lure and set `lastKnown`, `lastKnownZ` (`supportBelow`), awareness at least 5, `Investigate`, and a transient `e.lure=true`.
   - Add a `Pointer` branch before the Transformed block for the tell, crawl, lunge, grab and recover, reusing the Snatcher's lunge and grab sampling. It sets `goal` and `stalkSpeed`, then falls through to shared movement.
   - Guard the reactor-stalker block with `&&traits.lightSight==0`.
3. **Wake override** (lines 194-196): lightSight kinds do not get the player's position. Set awareness 0, Idle, and play one `enemySound`.
4. **Spawn facing:** a lightSight creature faces the nearest powered lamp within 4 m at creation. R-47 then faces Hale's lamp automatically.
5. **`migrateEnemiesAcrossChunks`:** skip lightSight kinds, so R-47 never walks into Ward C's blackout set piece.
6. **CreatureAnimation.h:**
   - Dormant (`e.wakeState`) plays clip 9.
   - Moving while `Flank` or `e.lure` plays clip 5; otherwise clip 1.
   - Tell plays 10; Rush 7; Grab 8; Recover plays 6 backwards.
7. **Save:** `SaveVersion` 20. Saves.cpp:111 accepts `Pointer` for version 20 and later. The transient fields (`lure`, the per-frame light stimulus) are never archived.
8. **Bounds:**
   - main.cpp:295-296 `kindNames`: add "transformed" and "pointer", and change the loop bound and digit range.
   - main.cpp:1416 and 1429.
   - CustomCampaign.cpp:375.
   - Game.cpp:746 spawn list and Game.cpp:82 recover multiplier.
   - SoftwareRenderer.h: `m_pointerMesh{id,"",id+2}`, the texture, the `creatureVisuals` row `{..,false,.9f}`, `m_bloodVariants` size and `static_assert`, and `modelReport`.
9. **Optional:** append `ScriptAction::Type::SquadCaution` (id = squad), which calls `startGuardCaution` on that squad's live guards on this level. Parse it as "CAUTION" in CustomCampaign.

## 9. The resonance mechanic: light that moves

There is one stimulus, computed once per frame and never saved. It drives both halves of the rule from the design doc: the big things come toward the light, and the small things scatter.

```cpp
// Game.h
struct LightStimulus{Point3 eye{},spot{};bool beam=false,moving=false;float flash=0;int litEnemy=-1;};
LightStimulus m_light;Point3 m_beamPrev{};float m_beamSample=0,m_beamMovingFor=0,m_beamYaw=0;
void updateLightStimulus(float dt);
// Stealth.cpp; called in Game.cpp right after updateExposure(dt), before updateEnemies
void Game::updateLightStimulus(float dt){
 auto&L=m_light;L.eye={m_player.pos.x,m_player.pos.y,m_player.z+m_player.eye};
 L.flash=muzzleFlash()>0?.3f:std::max(0.f,L.flash-dt);
 float yawRate=std::fabs(wrapAngle(m_player.angle-m_beamYaw))/std::max(dt,1e-4f);m_beamYaw=m_player.angle;
 L.beam=flashlightOn();
 if(!L.beam){m_beamMovingFor=0;L.moving=false;L.litEnemy=-1;}
 else{
  if((m_beamSample-=dt)<=0){m_beamSample=.15f;                      // one trace per .15 s
   float pitch=m_player.pitch/140.f;                                 // the gaze conversion EnemyAI uses
   Point3 dir{std::cos(m_player.angle)*std::cos(pitch),std::sin(m_player.angle)*std::cos(pitch),std::sin(pitch)};
   auto hit=traceShot(L.eye,dir,16.f);
   L.spot=hit.kind!=ShotKind::None?hit.point:Point3{L.eye.x+dir.x*16,L.eye.y+dir.y*16,L.eye.z+dir.z*16};
   L.litEnemy=hit.kind==ShotKind::Enemy?hit.index:-1;
   if(distance3(L.spot,m_beamPrev)>.45f)m_beamMovingFor=.6f;m_beamPrev=L.spot;}
  if(yawRate>1.4f||length(m_velocity)>2.6f)m_beamMovingFor=.6f;    // a swept torch, or one carried at a run
  m_beamMovingFor=std::max(0.f,m_beamMovingFor-dt);L.moving=m_beamMovingFor>0;}
 const StateId id=stateId("light_moving");if((state(id)!=0)!=L.moving)setState(id,L.moving?1:0);
}
```

- **Cycling lamps are lures as data.** Any `WorldLight` with `period>0` that is switched on counts. A strobe, a guttering desk lamp and a walking rig are all just authored lamps.
- **`anWalkingLamps`** goes in the an kit. It authors N stakes: a pipe r .03 from f to the top, a mine_cage_lamp head, and a Bare lamp (.6, range 4.5, 5600 K) with `enabledState=state`, `period`, `onTime=period/N`, and phase set so that stake i lights i·period/N after stake 0. Match `lampSwitchedOn`'s window formula when setting the phase. The whole rig is switched by one toggle terminal or one relay.
- **The `light_moving` state** lets content gate on moving light with no further engine work: the scatter cues and 84's glass wake. Pools stay always-on (3 dps) to match 80. Hazards are not gated on light.
- **Who sees what:**
  - A still torch, the plant's lamps, her desk lamp (until it gutters) and Hale's lamp are invisible to the Pointer.
  - Guards still see by `m_playerExposure`: a torch makes the player visible to them, moving or not.
  - The torch therefore costs something with both enemies, and gunfire from either side calls the dog.

## 10. Creatures and pickups

**84:** R-46 (Pointer), dormant at (4.9,20.3) on `anl_fm_wake`. Nothing else in the map.

**83:**
- R-47 (Pointer), awake at (18.4,12.4), facing Hale's lamp. It is 2.8 m from the disk at (20.5,14.4) and 6 m from her desk.
- Guards A and B in the north half.
- R-47 is the only creature in 83.

**Pickups:**
- 84: Health ×3, Ammo ×2, HandgunAmmo ×2. HandgunAmmo and Health in Chamber 3 reward the bus run.
- 83: HandgunAmmo ×2 (vestibule, Hale), Ammo ×3, Health ×2.

## 11. States

**Map 84**
- Set here:
  - `anl_fm_feed`.
  - `anl_fm_tape`, `anl_fm_t1`-`t4` (and their `_done` / `_go` / `_stop` finish states).
  - `anl_fm_west`, `anl_fm_desk`, `anl_fm_wake`, `anl_fm_hum(_done)`, `anl_fm_scatter_ms`.
  - `anl_fm_bus`, which is read by 83.
- Required here: `lab_chem_seal` (airlock), `light_moving`.

**Map 83**
- Set here:
  - `anl_diag_radio(_done)`.
  - `anl_l4_drafts(_done)`, `anl_l4_t1/t2`, `anl_l4_gutter(_stop)`.
  - `anl_l4_rig`, `anl_l4_door_radio(_done)`, `anl_diag_hum(_done)`, `anl_diag_scatter_ms`.
  - `anl_diag_a/b_dead/_damage`.
  - **`med_override`**, set only by R13.
- Required here:
  - `lab_chem_seal`, held by 81's door.
  - `med_override` (service door, `anl_l4_door_go`).
  - `res_lab_unsealed` (back door).
  - `anl_fm_bus` (rig).
  - `med_blackout`, inverted (corridor lamp).
  - `light_moving`.

Run `tools/check_state_ids.py`.

## 12. Tests

**Light stimulus:**
- A still torch held 3 s gives `moving=false`.
- A 90° yaw in .3 s gives `moving=true`.
- Sprinting with the torch on counts as moving.
- A shot gives `flash` for .3 s.
- `light_moving` mirrors the stimulus.
- The spot lands on a wall 10 m away.

**Pointer** (`--each-test=pointer`, plus the existing creature-behaviour suites staying green):
- A still lit torch at 6 m: no reaction for 5 s.
- A moving spot 8 m away: it goes to the spot and stands.
- A beam swept over it: a 1.0 s tell, then the crawl approach.
- Torch off during the tell: it stands at the last lit point, never at the player.
- A shot at 15 m: approach, then lunge.
- A dark, silent player at 3 m: undetected for 5 s. A crouched pass at 1.6 m: undetected. 1.0 m: struck.
- A cycling lamp behind holds glass: it stops at the glass.
- The walking rig: it follows and stops at the last lit stake.
- A shut Door: never passed.
- No seam migration.
- Waking from `wakeState` does not reveal the player's position.
- Saves at version 20 round-trip; version 19 still loads.

**Campaign map test** for 83 and 84:
- From 82 with `lab_chem_seal`=0, 84 is reachable and 83 is not.
- The service door requires `med_override`, and only R13 sets it.
- The back door requires `res_lab_unsealed`.
- R10 requires `anl_fm_bus`, which only F10 sets.
- The prone pass rejects any hull above .72, and neither pass reaches the service corridor.
- No guard post has line of sight to R-47 at spawn. The Lab 4 glass holds, so check the doorway angles.
- R-47's spawn is 2.5 m or more from R13.
- Signs: 2 per map, with titles unique across the game.
- The 84 tape relays fire at 6, 10, 17 and 19.2 s.

**Audits:**
- `--map-audit` on 83 and 84 (the battery does not cover them).
- `tools/dev` placement and facing check, including fronts for any newly imported prop.
- `edge_check` for the seams 83W/84E, 83E/76W and 83S/85N.
- Spot-view shots with arrival states (`lab_chem_seal`=1, `anl_fm_bus` 0 and 1).
- Perf: about 23 lights in 83 and about 14 in 84.

## 13. Build order

| Step | Work | Estimate |
|---|---|---|
| 1 | LightStimulus, the `light_moving` state and its tests. Independent and small. | ~1 h |
| 2 | Pointer plumbing: enum, traits field, bounds, save v20, renderer row. Then the AI branch, wake override, spawn facing and seam skip, with the Pointer tests run against a temporary internal mesh (never shipped). | ~4 h |
| 3 | `BakePointer.py`, the bake, assets.rc, `--creature-video=8` review, and the descent-assets.md row. The only real slip risk; the native takes keep it small. | 2-3 h |
| 4 | Promote mdGuard, mdBody and mdSequence to `kit::`. Add `anSwing` and `anWalkingLamps` to the an kit. | 30 min |
| 5 | Map 84: geometry, pan, rig, tape relays, Chamber 3 bus, readables. Then spot-view and map-audit. | ~3 h |
| 6 | Map 83: geometry, Lab 4, drafts and gutter, Hale and the disk, service door and radio, rig, guards, hall. Then spot-view and map-audit. | ~4 h |
| 7 | Optional `SquadCaution`. | 30 min |
| 8 | Route and state tests, edge checks, the full battery. Then Build.cmd and verify the exe size, hash and FileVersion before testing (the memory note about agent.py skipping builds). | |
| 9 | Docs and voice. Docs: the plan.py 84 entry ("E airlock held on `lab_chem_seal`; Chamber 3's field lamp bus feeds Lab 4's bench rig"), the design doc's Analysis section, the creature rules in memory. Voice: `inv_diag_1-3`, `inv_l4_1-3`, `anl_diag_a_1-3` and optional `dunne_tape_1-8` (ElevenLabs v3, filter, master, STT check). LAB 4 stays unvoiced. | |

**Release.** Chapter 2 stays sealed in release builds. Keep the Pointer, R-46 and R-47, the drafts and the "one thing" line out of trailers, store copy and patch notes. Notes stay spoiler-free and in the game's voice.

## 14. Decisions for the user

1. **The Diagnostics detail in 83.** plan.py describes 83 as "Story beat, quiet." I propose keeping two hostile, trapped men in the dark hall for stealth depth and the gunfire-calls-the-dog interplay. Lab 4 itself stays quiet: no forced fight. The fallback is to cut them and play the radio over a dead man's walkie.
2. **Her conclusion that the cells are pieces of one thing.** "it's one thing, and we cut pieces off it" commits Containment and the Dark Heart to not contradicting it. It is framed as her reading, not proven fact.
3. **R4 door log, optional extra entry "03:38 IN: LAB 4".** It would mean she came back through Lab 4 after OBS 6, and seeds "doors opened from inside". It is a strong breadcrumb and a lore commitment. The default omits it.
4. **Lowercase for her writing** (drafts, field book, stickies, the printout's pen line). This breaks the all-caps readable convention on purpose, following the prologue and map 35. The judge may flag it.
5. **The Investigator's pronoun.** 80's note says "SHE", 71's comment says "his", and the design doc says "they". The new lines use no pronoun. Which is canon?
6. **Save version 20 and the optional `SquadCaution` ScriptAction.** Both are small, but they are format and API additions.
7. **Optional library-import** of the purchased voice recorder for Dunne's desk (Claude, not Codex, per the memory note). The fallback is lab_field_meter.
8. **Tuning to settle in playtests:**
   - The drafts gutter gives about 4 s between the buzz and R-47 reaching the reading spot.
   - R-47's tell is 1.0 s; raise it to 1.4 s if first contact kills testers.
   - The tape relay times assume a lure speed of 1.6 m/s.

**Files:**
- N:\rawmetal\src\world\CampaignChapterAnalysis.cpp
- N:\rawmetal\src\world\CampaignKit.h
- N:\rawmetal\src\world\WorldDefinition.h
- N:\rawmetal\src\game\ai\enemy\EnemyAI.cpp
- N:\rawmetal\src\game\Game.h
- N:\rawmetal\src\game\Stealth.cpp
- N:\rawmetal\src\game\Game.cpp
- N:\rawmetal\src\game\CreatureAnimation.h
- N:\rawmetal\src\game\Saves.cpp
- N:\rawmetal\src\main.cpp
- N:\rawmetal\src\world\CustomCampaign.cpp
- N:\rawmetal\src\renderer\SoftwareRenderer.h
- N:\rawmetal\tools\campaign\plan.py
- N:\rawmetal\docs\Design\descent-campaign-20261025.md
- N:\rawmetal\docs\Design\descent-assets.md
- O:\retro official\Creatures\Nightmare-Creature-1-PSX-2112\nightmare-creature-1_PSX.fbx

## 15. Decisions (user, 2026-11-04)

1. **The Diagnostics detail stays.** Two trapped, hostile men hold the dark hall. Their gunfire lures the Pointer, and Lab 4's own room stays quiet.
2. **Resonance is cursed energy ore mined from the Dark Below.** The cells are filled with refined ore, not charged. Replace LAB 4's "one thing, we cut pieces off it" with her working out that the cells hold the ore itself, brought up from the deep face and sold as power. Chemistry's "same crystal habit" result in 81 already points there.
3. **The Investigator is male.** Map 80's note is fixed to "HE".
4. **The player (Claire) is female.** She is private security hired by Depthworks, but not authorized in this sector. The Investigator's orders include eliminating her along with the rest, so any radio or order text that names the security officer should say so.
5. **LAB 4's own writing is lowercase:** drafts, field book and sticky notes, matching the prologue chat and map 35's message.
