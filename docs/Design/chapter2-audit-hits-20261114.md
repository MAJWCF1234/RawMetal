# Chapter 2 inspection: audit hits, 2026-11-14

The map audit (src/game/MapAudit.cpp, `--map-audit=N`, lab `engine.map-audit`) gained the placement rules for every class the inspectors found (docs/Design/chapter2-inspection-20261114.md). This is every hit of those rules over all 180 campaign maps, as of the E1-E4 worktree (map 109 already has its ring moved). Each line is the audit's own: the record (fixture index, prop, structure or door), its model, position and height. Map content is not changed here: these are for the map workers.

## The rules

- FLOATING FIXTURE: nothing under it (floor, slab, a fixture top or shelf, a prop), not touching a wall, not under a roof or slab, not on a pipe, rod or rail.
- FLOATING STRUCTURE: a visible bar, shelf or strip (not a collision proxy, not a rail) in mid-air with no wall, floor, roof, other structure, fixture or pipe within 3 cm.
- STACKED: a piece of furniture (0.3 m2 footprint, 0.45 m tall or more) standing on another (fixture or prop; crates, boxes, drums, sample fridges and paper are allowed to stack).
- INSIDE: at least 6 of 27 sample points of one fixture inside another's box, unless it sits on a shelf or table surface of it (storage) or one of them is a frame.
- IN DOORWAY / IN DOOR SWING / BLOCKS DOOR APPROACH: a fixture, prop or structure in the opening, in a swinging leaf's arc, or within 1.2 m of the door line across its middle, on either side. TRIM ACROSS DOOR / STRUCTURE IN DOORWAY: a structure crossing the opening below 1.95 m.
- LAMPS STACKED: two lights within 0.25 m. DUPLICATE: the same model twice within 0.1 m.
- SCREEN FACES WALL: a monitor, terminal or console (fixture, prop, or the computer a terminal record is drawn as) with visible wall less than 0.4 m in front of its face.
- WALL DEVICE OVERSIZE / OFF WALL: a card reader or keypad (lab_id_scanner, lab_door_reader) over 0.35 m, or with no wall behind it.
- RAILING ACROSS WALKWAY: a rail with the same walked floor on both sides along 60% of it or more.
- SCALE: a pack model drawn at under 0.45 or over 2.2 times its own size.
- WALK-THROUGH: a non-solid fixture 1.2 m tall or more and 0.6 m2 or more whose footprint the player walks through.

## Totals

| Rule | Hits |
|---|---|
| FLOATING FIXTURE | 84 |
| FLOATING STRUCTURE | 77 |
| STACKED | 10 |
| INSIDE | 28 |
| IN DOORWAY | 2 |
| IN DOOR SWING | 26 |
| BLOCKS DOOR APPROACH | 27 |
| TRIM ACROSS DOOR | 17 |
| STRUCTURE IN DOORWAY | 4 |
| LAMPS STACKED | 22 |
| DUPLICATE | 8 |
| SCREEN FACES WALL | 6 |
| WALL DEVICE OVERSIZE | 37 |
| WALL DEVICE OFF WALL | 17 |
| RAILING ACROSS WALKWAY | 43 |
| SCALE | 30 |
| WALK-THROUGH | 1 |
| all | 439 |

## The inspectors' items and what catches them

| Item | Caught by | Hit |
|---|---|---|
| M1 68 cabinet with a box through its shelves | INSIDE | lab_test_chamber inside office_display_cabinet at 16.93,8.60 |
| M2 73 bars floating over the beds | partly: FLOATING STRUCTURE | one strip at 22.66-22.82,10.6-12.4; the x-ray table straps touch their pads, so not flagged |
| M3 77 floating med kit | FLOATING FIXTURE | med_first_aid_box at 6.06,13.20, 1.30 m up |
| M4 80 trim across a door | not caught | no structure crosses an 80 door opening below 1.95 m within 0.2 m of the line: look by eye |
| M5 81 cabinet | not caught | nothing on 81 but readers (WALL DEVICE) |
| M6 84 lamps floating by the railing | FLOATING FIXTURE | mine_cage_lamp at 8.40,18.42 and 11.60,18.42; pendant_lamp at 10.00,16.10 |
| M7 86 extinguisher; box at a doorway | FLOATING FIXTURE (extinguisher at 22.91,10.00); the box not caught | |
| M8 88 cabinets | possibly FLOATING FIXTURE | three lab_notice_board on walls flagged 0.99 m up: check whether they touch the wall |
| C1 94 door blockage, corpse over a bed | IN DOOR SWING, INSIDE | med_ward_bed and med_corpse_supine at 20.60,14.95 in door[3]'s swing; corpse inside its bed |
| C2 99 railing across the path | RAILING ACROSS WALKWAY | structures 113-116, x 17.00-17.06, y 2-6 |
| C3 102 wall lamps stacked | LAMPS STACKED | lights 18 and 27 at 18.00,21.30 |
| C4 103 desk on a desk | not caught | no furniture-sized fixture or prop stands on another on 103 |
| C5 107 terminal facing a wall | not caught | |
| C6 108 terminal facing a wall | not caught | |
| C7 108 bench blocking a door | BLOCKS DOOR APPROACH | bench structures 93-95 at 3.15-5.85,13.55-14.08 before door[3] |
| C8 108 giant readers | WALL DEVICE OVERSIZE / OFF WALL | the three lab_door_reader pylons at 9.43/12.00/14.57,15.10 |
| C9 108 notice board over a door | IN DOORWAY | lab_notice_board at 3.60,15.23 (door[3]) and 19.00,13.14 (door[2]) |

Known noise: lab_id_scanner at its own size (0.41 m tall) trips WALL DEVICE OVERSIZE (the 0.35 m rule); hung catwalk sections and decks (107) read as FLOATING when their hangers are not pipes; lamps hung under a non-solid ceiling grid can read as FLOATING FIXTURE.

## Chapter 2 (68-109)

### 68 RESEARCH INTAKE (17)

- FLOATING FIXTURE fixture[93] model 337 (med_extinguisher) at 10.12,22.40 z -11.55 (0.24x0.16x0.66) 0.45 m over what is under it
- STACKED fixture[100] model 196 (lab_instrument_terminal) at 21.10,1.30 z -11.25 (0.92x0.47x0.68) on fixture[99] model 24 (desk) at 21.10,1.35 z -12.00 (1.40x0.70x0.75)
- INSIDE fixture[75] model 210 (lab_test_chamber) at 16.93,8.60 z -10.95 (0.63x0.37x0.35) is inside fixture[74] model 355 (office_display_cabinet) at 16.93,8.60 z -12.00 (0.97x0.54x2.10) (27/27 of it)
- TRIM ACROSS DOOR structure[30] 8.93,0.00..9.07,1.12 z -12.00..-9.20 mat 20 (trim) of door[0] x 9.00-15.00 at y 0.50
- TRIM ACROSS DOOR structure[31] 14.93,0.00..15.07,1.12 z -12.00..-9.20 mat 20 (trim) of door[0] x 9.00-15.00 at y 0.50
- TRIM ACROSS DOOR structure[32] 10.43,3.18..10.57,4.42 z -12.00..-9.20 mat 27 (trim) of door[1] x 10.50-13.50 at y 3.80
- TRIM ACROSS DOOR structure[33] 13.43,3.18..13.57,4.42 z -12.00..-9.20 mat 27 (trim) of door[1] x 10.50-13.50 at y 3.80
- BLOCKS DOOR APPROACH fixture[26] model 332 (psx_locker) at 1.34,5.50 z -12.00 (0.64x0.85x2.42) of door[2] x 1.40-2.40 at y 6.50
- IN DOOR SWING structure[67] 1.00,6.40..1.40,6.60 z -12.00..-8.50 mat 31 of door[2] x 1.40-2.40 at y 6.50
- WALL DEVICE OVERSIZE fixture[0] model 195 (lab_id_scanner) at 9.65,3.12 z -11.00 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[0] model 195 (lab_id_scanner) at 9.65,3.12 z -11.00 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[13] model 195 (lab_id_scanner) at 10.57,6.05 z -11.00 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[13] model 195 (lab_id_scanner) at 10.57,6.05 z -11.00 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[14] model 195 (lab_id_scanner) at 11.23,6.05 z -11.00 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[14] model 195 (lab_id_scanner) at 11.23,6.05 z -11.00 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[15] model 195 (lab_id_scanner) at 13.43,6.05 z -11.00 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[15] model 195 (lab_id_scanner) at 13.43,6.05 z -11.00 (0.30x0.06x0.41)

### 69 PERSONNEL PROCESSING (7)

- TRIM ACROSS DOOR structure[0] 9.93,22.88..10.07,24.00 z -12.00..-9.20 mat 20 (trim) of door[0] x 10.00-14.00 at y 23.50
- TRIM ACROSS DOOR structure[1] 13.93,22.88..14.07,24.00 z -12.00..-9.20 mat 20 (trim) of door[0] x 10.00-14.00 at y 23.50
- SCREEN FACES WALL fixture[96] model 232 (med_vitals_monitor) at 22.45,21.70 z -11.19 (0.38x0.18x0.28) (wall 0.05 m off its face)
- WALL DEVICE OVERSIZE fixture[15] model 195 (lab_id_scanner) at 10.03,22.60 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[15] model 195 (lab_id_scanner) at 10.03,22.60 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[82] model 195 (lab_id_scanner) at 14.63,21.90 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[82] model 195 (lab_id_scanner) at 14.63,21.90 z -10.90 (0.30x0.06x0.41)

### 70 RESEARCH ADMINISTRATION (5)

- DUPLICATE fixture[104] model 339 (office_paper_stack) at 9.85,4.65 z -11.22 (0.33x0.44x0.03) and fixture[105]
- DUPLICATE fixture[161] model 348 (item_id_card) at 7.85,10.60 z -10.95 (0.13x0.09x0.00) and fixture[162]
- WALL DEVICE OVERSIZE fixture[185] model 195 (lab_id_scanner) at 8.35,22.97 z -10.95 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[185] model 195 (lab_id_scanner) at 8.35,22.97 z -10.95 (0.30x0.06x0.41)
- SCALE fixture[3] model 233 (med_wall_clock) at 17.60,15.96 z -8.50 (0.76x0.06x0.75) at x3.20 its own size

### 71 MEDICAL SCREENING (11)

- STACKED fixture[68] model 196 (lab_instrument_terminal) at 12.16,2.35 z -11.19 (0.83x0.42x0.62) on fixture[67] model 188 (lab_clean_bench) at 12.12,2.35 z -12.00 (1.18x0.56x0.81)
- STACKED fixture[70] model 196 (lab_instrument_terminal) at 12.16,3.70 z -11.19 (0.83x0.42x0.62) on fixture[69] model 188 (lab_clean_bench) at 12.12,3.70 z -12.00 (1.18x0.56x0.81)
- IN DOOR SWING structure[21] 7.60,9.80..7.90,10.00 z -12.00..-8.80 mat 3 of door[1] x 7.90-8.80 at y 9.90
- IN DOOR SWING structure[127] 4.30,14.00..4.50,14.20 z -12.00..-8.80 mat 3 of door[10] x 4.50-5.40 at y 14.10
- BLOCKS DOOR APPROACH prop model 234 (med_ward_bed) at 12.95,15.90 z -12.00 of door[12] x 11.40-12.60 at y 16.90
- IN DOOR SWING structure[157] 11.35,16.82..11.40,16.98 z -10.99..-9.45 mat 20 of door[12] x 11.40-12.60 at y 16.90
- SCREEN FACES WALL prop model 232 (med_vitals_monitor) at 11.45,8.55 z -12.00 (wall 0.25 m off its face)
- WALL DEVICE OVERSIZE fixture[23] model 195 (lab_id_scanner) at 1.40,9.03 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[23] model 195 (lab_id_scanner) at 1.40,9.03 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[155] model 195 (lab_id_scanner) at 13.97,22.40 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[155] model 195 (lab_id_scanner) at 13.97,22.40 z -10.90 (0.30x0.06x0.41)

### 72 MEDICAL / TRIAGE (7)

- FLOATING STRUCTURE structure[185] 2.05,8.77..2.95,8.79 z -10.98..-10.08 mat 16, 0.21 m over what is under it
- BLOCKS DOOR APPROACH fixture[31] model 228 (med_records_cabinet) at 4.76,5.85 z -12.00 (0.90x0.49x1.00) of door[0] x 4.00-4.90 at y 4.30
- DUPLICATE fixture[119] model 340 (office_file_binder) at 1.25,15.55 z -11.00 (0.09x0.30x0.42) and fixture[120]
- SCREEN FACES WALL prop model 232 (med_vitals_monitor) at 14.75,9.60 z -12.00 (wall 0.38 m off its face)
- WALL DEVICE OVERSIZE fixture[17] model 195 (lab_id_scanner) at 1.40,9.03 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[17] model 195 (lab_id_scanner) at 1.40,9.03 z -10.90 (0.30x0.06x0.41)
- SCALE fixture[134] model 298 (pwr_tool_chest) at 17.55,16.45 z -12.00 (0.75x0.45x0.97) at x0.20 its own size

### 73 MEDICAL / IMAGING (1)

- FLOATING STRUCTURE structure[454] 22.66,10.60..22.82,12.40 z -9.82..-9.13 mat 2, 2.18 m over what is under it

### 74 MEDICAL / PHARMACY (7)

- IN DOOR SWING structure[26] 11.85,6.82..11.90,6.98 z -11.04..-9.90 mat 20 of door[1] x 11.90-12.80 at y 6.90
- IN DOOR SWING structure[93] 3.50,3.45..3.60,3.55 z -12.00..-9.45 mat 20 of door[2] x 3.60-4.60 at y 3.50
- LAMPS STACKED lights 0 and 1 at 4.75,3.55 z -9.70
- WALL DEVICE OVERSIZE fixture[4] model 195 (lab_id_scanner) at 8.85,9.03 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[10] model 195 (lab_id_scanner) at 20.97,11.30 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[47] model 195 (lab_id_scanner) at 6.90,3.58 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[47] model 195 (lab_id_scanner) at 6.90,3.58 z -10.90 (0.30x0.06x0.41)

### 75 MEDICAL / WARD C NORTH (2)

- IN DOOR SWING structure[53] 4.10,10.40..4.40,10.60 z -12.00..-9.00 mat 3 of door[1] x 4.40-5.30 at y 10.50
- IN DOOR SWING structure[64] 4.10,12.40..4.40,12.60 z -12.00..-9.00 mat 3 of door[2] x 4.40-5.30 at y 12.50

### 76 MEDICAL / WARD C SOUTH (2)

- BLOCKS DOOR APPROACH structure[416] 20.25,13.20..20.28,13.23 z -11.94..-11.10 mat 20 of door[5] x 19.60-20.50 at y 14.10
- BLOCKS DOOR APPROACH structure[417] 19.73,12.68..20.27,13.22 z -11.84..-11.30 mat 23 of door[5] x 19.60-20.50 at y 14.10

### 77 MEDICAL / SURGERY (10)

- FLOATING FIXTURE fixture[8] model 219 (med_first_aid_box) at 6.06,13.20 z -10.70 (0.37x0.12x0.35) 1.30 m over what is under it
- INSIDE fixture[17] model 356 (med_corpse_supine) at 2.00,15.60 z -11.43 (0.71x1.67x0.39) is inside fixture[16] model 234 (med_ward_bed) at 2.04,15.60 z -12.00 (1.98x0.94x0.84) (18/27 of it)
- IN DOOR SWING structure[18] 10.80,7.20..11.00,7.40 z -12.00..-8.50 mat 3 of door[3] x 11.00-11.90 at y 7.30
- IN DOOR SWING structure[84] 10.80,7.40..11.00,7.42 z -12.00..-8.50 mat 27 of door[3] x 11.00-11.90 at y 7.30
- IN DOOR SWING prop model 18 (wooden_chair) at 19.30,19.00 z -12.00 of door[7] x 19.60-20.80 at y 18.10
- LAMPS STACKED lights 2 and 20 at 3.00,11.50 z -8.80
- LAMPS STACKED lights 4 and 21 at 7.30,11.50 z -8.80
- LAMPS STACKED lights 5 and 22 at 7.30,16.60 z -8.80
- SCALE fixture[70] model 185 (lab_anomaly_relic) at 14.20,22.67 z -11.18 (0.15x0.12x0.12) at x0.30 its own size
- SCALE fixture[71] model 185 (lab_anomaly_relic) at 14.85,22.67 z -11.18 (0.15x0.12x0.12) at x0.30 its own size

### 78 MEDICAL / MORGUE (5)

- FLOATING STRUCTURE structure[15] 14.65,2.88..14.85,2.94 z -10.90..-10.60 mat 20, 1.10 m over what is under it
- INSIDE fixture[12] model 356 (med_corpse_supine) at 5.05,16.90 z -14.12 (0.69x1.63x0.38) is inside fixture[6] model 212 (med_autopsy_table) at 5.00,16.90 z -15.00 (2.20x0.90x1.05) (9/27 of it)
- INSIDE fixture[13] model 356 (med_corpse_supine) at 5.05,19.30 z -14.12 (0.67x1.58x0.37) is inside fixture[8] model 212 (med_autopsy_table) at 5.00,19.30 z -15.00 (2.20x0.90x1.05) (9/27 of it)
- INSIDE fixture[14] model 356 (med_corpse_supine) at 5.05,21.70 z -14.12 (0.71x1.67x0.39) is inside fixture[10] model 212 (med_autopsy_table) at 5.00,21.70 z -15.00 (2.20x0.90x1.05) (9/27 of it)
- BLOCKS DOOR APPROACH fixture[23] model 332 (psx_locker) at 10.12,22.10 z -15.00 (0.64x0.85x2.42) of door[0] x 10.00-14.00 at y 23.50

### 79 MATERIALS INTAKE (2)

- IN DOOR SWING structure[67] 4.55,9.82..4.60,9.98 z -11.04..-9.90 mat 20 of door[1] x 4.60-5.80 at y 9.90
- WALL DEVICE OVERSIZE fixture[2] model 195 (lab_id_scanner) at 9.20,4.77 z -10.90 (0.30x0.06x0.41)

### 80 SAMPLE STORAGE (4)

- FLOATING FIXTURE fixture[67] model 209 (lab_status_display) at 14.00,22.11 z -12.88 (0.35x0.03x0.10) 0.12 m over what is under it
- FLOATING FIXTURE fixture[96] model 336 (clinic_ceiling_lamp_2) at 6.60,19.00 z -12.51 (0.44x1.04x0.17) 2.63 m over what is under it
- WALL DEVICE OVERSIZE fixture[75] model 195 (lab_id_scanner) at 16.77,18.30 z -13.90 (0.30x0.06x0.41)
- RAILING ACROSS WALKWAY structure[366] 17.00,11.00..17.06,12.00 z -12.00 (2/3 of it walked on both sides)

### 81 ANALYTICAL CHEMISTRY (2)

- WALL DEVICE OVERSIZE fixture[0] model 195 (lab_id_scanner) at 13.75,4.77 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[50] model 195 (lab_id_scanner) at 14.77,22.30 z -10.90 (0.30x0.06x0.41)

### 82 INSTRUMENTATION (7)

- BLOCKS DOOR APPROACH fixture[9] model 30 (cctv_tv) at 22.70,7.00 z -10.26 (0.50x0.40x0.42) of door[0] x 21.50-22.70 at y 6.10
- BLOCKS DOOR APPROACH fixture[10] model 30 (cctv_tv) at 22.70,7.50 z -10.26 (0.50x0.40x0.42) of door[0] x 21.50-22.70 at y 6.10
- BLOCKS DOOR APPROACH structure[107] 22.50,6.70..22.90,8.30 z -10.30..-10.26 mat 20 of door[0] x 21.50-22.70 at y 6.10
- LAMPS STACKED lights 0 and 1 at 20.50,6.36 z -10.22
- WALL DEVICE OVERSIZE fixture[72] model 195 (lab_id_scanner) at 16.97,14.80 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[79] model 195 (lab_id_scanner) at 21.20,5.97 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[83] model 195 (lab_id_scanner) at 6.04,22.40 z -10.90 (0.30x0.06x0.41)

### 83 ENERGY DIAGNOSTICS / LAB 4 (13)

- INSIDE fixture[58] model 197 (lab_knife_switch) at 3.00,22.06 z -10.70 (0.31x0.20x0.32) is inside fixture[57] model 363 (anl_switch_cabinet_2) at 2.60,22.50 z -12.00 (1.50x0.82x2.70) (9/27 of it)
- INSIDE fixture[60] model 197 (lab_knife_switch) at 5.00,22.06 z -10.70 (0.31x0.20x0.32) is inside fixture[59] model 363 (anl_switch_cabinet_2) at 4.60,22.50 z -12.00 (1.50x0.82x2.70) (9/27 of it)
- INSIDE fixture[62] model 197 (lab_knife_switch) at 7.00,22.06 z -10.70 (0.31x0.20x0.32) is inside fixture[61] model 363 (anl_switch_cabinet_2) at 6.60,22.50 z -12.00 (1.50x0.82x2.70) (9/27 of it)
- STRUCTURE IN DOORWAY structure[213] 7.40,5.00..7.80,5.32 z -12.00..-8.50 mat 15 of door[1] x 6.60-7.80 at y 4.90
- LAMPS STACKED lights 5 and 41 at 6.80,9.40 z -9.70
- LAMPS STACKED lights 6 and 42 at 9.30,9.40 z -9.70
- LAMPS STACKED lights 16 and 17 at 22.45,6.00 z -10.80
- LAMPS STACKED lights 19 and 25 at 22.20,11.60 z -10.85
- LAMPS STACKED lights 28 and 29 at 21.00,14.70 z -10.50
- LAMPS STACKED lights 36 and 37 at 4.60,21.50 z -10.10
- SCREEN FACES WALL terminal 'BENCH RIG / WALKING LAMPS' at 17.00,5.55 (wall 0.60 m in front of it)
- WALL DEVICE OVERSIZE fixture[29] model 195 (lab_id_scanner) at 14.97,7.40 z -10.80 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[47] model 195 (lab_id_scanner) at 21.00,14.78 z -10.80 (0.30x0.06x0.41)

### 84 FIELD MEASUREMENT (13)

- FLOATING FIXTURE fixture[57] model 246 (mine_cage_lamp) at 8.40,18.42 z -8.96 (0.21x0.20x0.36) 3.24 m over what is under it
- FLOATING FIXTURE fixture[58] model 246 (mine_cage_lamp) at 11.60,18.42 z -8.96 (0.21x0.20x0.36) 3.24 m over what is under it
- FLOATING FIXTURE fixture[59] model 42 (pendant_lamp) at 10.00,16.10 z -8.30 (0.50x0.50x0.34) 3.90 m over what is under it
- INSIDE fixture[31] model 29 (pc_tower) at 10.02,7.70 z -12.00 (0.20x0.45x0.42) is inside fixture[25] model 24 (desk) at 9.50,7.60 z -12.00 (1.40x0.70x0.75) (27/27 of it)
- BLOCKS DOOR APPROACH fixture[48] model 246 (mine_cage_lamp) at 15.90,10.10 z -10.28 (0.21x0.20x0.36) of door[0] x 15.40-16.60 at y 8.90
- IN DOOR SWING structure[163] 4.00,10.00..16.00,11.00 z -11.43..-11.38 mat 8 of door[0] x 15.40-16.60 at y 8.90
- IN DOOR SWING structure[165] 4.00,10.00..16.00,10.06 z -11.58..-11.43 mat 12 of door[0] x 15.40-16.60 at y 8.90
- IN DOOR SWING structure[181] 15.56,10.02..15.64,10.10 z -12.00..-11.58 mat 12 of door[0] x 15.40-16.60 at y 8.90
- BLOCKS DOOR APPROACH structure[388] 16.00,10.00..16.28,11.00 z -11.50..-11.38 mat 2 of door[0] x 15.40-16.60 at y 8.90
- BLOCKS DOOR APPROACH structure[391] 16.28,10.00..16.56,11.00 z -11.71..-11.59 mat 2 of door[0] x 15.40-16.60 at y 8.90
- SCREEN FACES WALL terminal 'FIELD LAMPS / WEST WALK' at 13.40,5.55 (wall 0.60 m in front of it)
- WALL DEVICE OVERSIZE fixture[44] model 195 (lab_id_scanner) at 20.85,8.77 z -10.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[45] model 195 (lab_id_scanner) at 16.85,8.77 z -10.90 (0.30x0.06x0.41)

### 85 RESONANCE LABORATORY (3)

- IN DOOR SWING structure[98] 11.37,14.30..11.40,14.50 z -11.05..-9.60 mat 20 of door[3] x 11.40-12.60 at y 14.40
- BLOCKS DOOR APPROACH fixture[26] model 19 (wooden_table) at 6.40,15.60 z -12.00 (1.37x0.72x0.78) of door[4] x 5.00-6.20 at y 17.00
- WALL DEVICE OVERSIZE fixture[33] model 195 (lab_id_scanner) at 13.97,18.20 z -10.90 (0.30x0.06x0.41)

### 86 HIGH-ENERGY TEST BAY (5)

- FLOATING FIXTURE fixture[13] model 337 (med_extinguisher) at 22.91,10.00 z -11.55 (0.24x0.16x0.66) 0.45 m over what is under it
- FLOATING FIXTURE fixture[21] model 283 (pwr_disconnect_switch) at 1.30,11.55 z -19.00 (0.43x0.35x0.85) 1.00 m over what is under it
- RAILING ACROSS WALKWAY structure[235] 8.00,16.94..9.00,17.00 z -12.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[237] 9.00,16.94..10.00,17.00 z -12.00 (3/3 of it walked on both sides)
- SCALE fixture[17] model 210 (lab_test_chamber) at 8.50,8.50 z -19.90 (3.42x2.04x1.92) at x3.00 its own size

### 87 OBSERVATION RING / NORTH-WEST (8)

- FLOATING FIXTURE fixture[6] model 209 (lab_status_display) at 12.40,10.16 z -9.30 (0.69x0.06x0.20) 2.70 m over what is under it
- FLOATING FIXTURE fixture[7] model 209 (lab_status_display) at 11.00,10.16 z -9.30 (0.69x0.06x0.20) 2.70 m over what is under it
- RAILING ACROSS WALKWAY structure[116] 17.00,7.85..18.90,7.95 z -12.00 (6/6 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[140] 7.85,17.00..7.95,18.90 z -12.00 (6/6 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[340] 19.10,7.85..20.90,7.95 z -12.00 (6/6 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[341] 21.10,7.85..23.00,7.95 z -12.00 (6/6 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[342] 7.85,19.10..7.95,20.90 z -12.00 (6/6 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[343] 7.85,21.10..7.95,23.00 z -12.00 (6/6 of it walked on both sides)

### 88 OBSERVATION RING / NORTH-EAST (4)

- FLOATING FIXTURE fixture[49] model 200 (lab_notice_board) at 6.60,15.55 z -11.00 (1.20x0.07x0.90) 0.99 m over what is under it
- FLOATING FIXTURE fixture[50] model 200 (lab_notice_board) at 8.60,17.60 z -11.00 (1.20x0.07x0.90) 0.99 m over what is under it
- FLOATING FIXTURE fixture[51] model 200 (lab_notice_board) at 8.60,23.00 z -11.00 (1.20x0.07x0.90) 0.99 m over what is under it
- IN DOOR SWING structure[86] 7.00,7.80..7.40,8.00 z -12.00..6.00 mat 3 of door[0] x 7.40-8.60 at y 7.90

### 89 OBSERVATION RING / SOUTH-WEST (3)

- FLOATING FIXTURE fixture[52] model 246 (mine_cage_lamp) at 19.15,2.20 z -21.15 (0.21x0.20x0.36) 6.71 m over what is under it
- STACKED fixture[38] model 262 (mine_shaft_ladder) at 16.42,6.60 z 8.00 (0.78x0.90x7.46) on fixture[39] model 262 (mine_shaft_ladder) at 16.42,6.60 z 0.60 (0.78x0.90x7.46)
- BLOCKS DOOR APPROACH fixture[43] model 19 (wooden_table) at 3.70,17.35 z -12.00 (1.37x0.72x0.78) of door[1] x 3.40-4.60 at y 16.10

### 90 OBSERVATION RING / SOUTH-EAST (4)

- IN DOOR SWING fixture[46] model 369 (urb_jersey_barrier) at 7.60,14.30 z -12.00 (2.48x0.81x1.14) of door[0] x 7.40-8.60 at y 16.10
- BLOCKS DOOR APPROACH fixture[52] model 208 (lab_specimen_crate) at 8.70,14.90 z -12.00 (0.90x0.90x0.90) of door[0] x 7.40-8.60 at y 16.10
- IN DOOR SWING structure[86] 7.00,16.00..7.40,16.20 z -12.00..6.00 mat 3 of door[0] x 7.40-8.60 at y 16.10
- BLOCKS DOOR APPROACH fixture[29] model 369 (urb_jersey_barrier) at 20.80,14.35 z -12.00 (2.48x0.81x1.14) of door[1] x 19.40-20.60 at y 16.10

### 91 SHIELDING CORRIDOR (2)

- LAMPS STACKED lights 5 and 6 at 12.00,17.58 z -9.25
- WALL DEVICE OVERSIZE fixture[14] model 195 (lab_id_scanner) at 16.20,8.88 z -10.90 (0.30x0.06x0.41)

### 93 CONVERSION LAB (5)

- FLOATING FIXTURE fixture[7] model 246 (mine_cage_lamp) at 5.90,5.75 z -12.60 (0.21x0.20x0.36) 3.30 m over what is under it
- FLOATING FIXTURE fixture[9] model 246 (mine_cage_lamp) at 5.90,13.35 z -12.60 (0.21x0.20x0.36) 3.30 m over what is under it
- FLOATING FIXTURE fixture[11] model 210 (lab_test_chamber) at 14.60,12.60 z -14.40 (1.14x0.68x0.64) 0.70 m over what is under it
- SCALE fixture[8] model 185 (lab_anomaly_relic) at 11.50,8.60 z -14.40 (1.10x0.90x0.90) at x2.23 its own size
- SCALE fixture[10] model 185 (lab_anomaly_relic) at 11.50,16.80 z -14.40 (1.10x0.90x0.90) at x2.23 its own size

### 94 QUARANTINE WARD (3)

- INSIDE fixture[39] model 356 (med_corpse_supine) at 20.60,14.95 z -14.34 (0.74x1.75x0.41) is inside fixture[37] model 234 (med_ward_bed) at 20.60,14.95 z -15.00 (1.98x0.94x0.84) (9/27 of it)
- IN DOOR SWING fixture[37] model 234 (med_ward_bed) at 20.60,14.95 z -15.00 (1.98x0.94x0.84) of door[3] x 19.40-20.60 at y 13.90
- IN DOOR SWING fixture[39] model 356 (med_corpse_supine) at 20.60,14.95 z -14.34 (0.74x1.75x0.41) of door[3] x 19.40-20.60 at y 13.90

### 95 CONTAINMENT INTAKE (15)

- FLOATING FIXTURE fixture[16] model 209 (lab_status_display) at 20.40,6.75 z -12.90 (0.69x0.06x0.20) 2.10 m over what is under it
- FLOATING STRUCTURE structure[109] 6.28,3.30..6.56,4.80 z -12.31..-12.19 mat 2, 2.69 m over what is under it
- FLOATING STRUCTURE structure[110] 6.56,3.30..6.84,4.80 z -12.49..-12.38 mat 2, 2.51 m over what is under it
- FLOATING STRUCTURE structure[111] 6.84,3.30..7.12,4.80 z -12.68..-12.56 mat 2, 2.32 m over what is under it
- FLOATING STRUCTURE structure[112] 7.12,3.30..7.40,4.80 z -12.87..-12.75 mat 2, 2.13 m over what is under it
- FLOATING STRUCTURE structure[113] 7.40,3.30..7.68,4.80 z -13.06..-12.94 mat 2, 1.94 m over what is under it
- FLOATING STRUCTURE structure[114] 7.68,3.30..7.96,4.80 z -13.24..-13.12 mat 2, 1.76 m over what is under it
- FLOATING STRUCTURE structure[115] 7.96,3.30..8.24,4.80 z -13.43..-13.31 mat 2, 1.57 m over what is under it
- FLOATING STRUCTURE structure[116] 8.24,3.30..8.52,4.80 z -13.62..-13.50 mat 2, 1.38 m over what is under it
- FLOATING STRUCTURE structure[117] 8.52,3.30..8.80,4.80 z -13.81..-13.69 mat 2, 1.19 m over what is under it
- FLOATING STRUCTURE structure[118] 8.80,3.30..9.08,4.80 z -13.99..-13.88 mat 2, 1.01 m over what is under it
- FLOATING STRUCTURE structure[119] 9.08,3.30..9.36,4.80 z -14.18..-14.06 mat 2, 0.82 m over what is under it
- FLOATING STRUCTURE structure[120] 9.36,3.30..9.64,4.80 z -14.37..-14.25 mat 2, 0.63 m over what is under it
- FLOATING STRUCTURE structure[121] 9.64,3.30..9.92,4.80 z -14.56..-14.44 mat 2, 0.44 m over what is under it
- FLOATING STRUCTURE structure[122] 9.92,3.30..10.20,4.80 z -14.74..-14.62 mat 2, 0.26 m over what is under it

### 96 CONTAINMENT A (8)

- FLOATING FIXTURE fixture[3] model 246 (mine_cage_lamp) at 3.70,7.25 z -12.50 (0.21x0.20x0.36) 2.50 m over what is under it
- FLOATING FIXTURE fixture[6] model 246 (mine_cage_lamp) at 9.25,7.25 z -12.50 (0.21x0.20x0.36) 2.50 m over what is under it
- FLOATING FIXTURE fixture[10] model 246 (mine_cage_lamp) at 14.75,7.25 z -12.50 (0.21x0.20x0.36) 2.50 m over what is under it
- FLOATING FIXTURE fixture[13] model 246 (mine_cage_lamp) at 20.30,7.25 z -12.50 (0.21x0.20x0.36) 1.95 m over what is under it
- WALL DEVICE OVERSIZE fixture[1] model 195 (lab_id_scanner) at 6.15,6.73 z -13.85 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[5] model 195 (lab_id_scanner) at 11.65,6.73 z -13.85 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[8] model 195 (lab_id_scanner) at 17.15,6.73 z -13.85 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[12] model 195 (lab_id_scanner) at 22.75,6.73 z -13.85 (0.30x0.06x0.41)

### 98 CONTAINMENT B (3)

- FLOATING FIXTURE fixture[204] model 189 (lab_control_board) at 22.86,12.20 z -14.05 (1.32x0.19x0.60) 0.95 m over what is under it
- DUPLICATE fixture[69] model 378 (res_rock_crag) at 7.57,11.22 z -15.04 (0.94x0.71x0.55) and fixture[73]
- DUPLICATE fixture[74] model 378 (res_rock_crag) at 4.80,11.61 z -15.05 (2.30x1.47x1.34) and fixture[75]

### 99 SPECIMEN HANDLING (6)

- FLOATING FIXTURE fixture[1] model 222 (med_holding_cage) at 12.00,12.00 z -14.20 (4.49x2.03x2.01) 3.80 m over what is under it
- FLOATING FIXTURE fixture[18] model 356 (med_corpse_supine) at 12.90,14.40 z -17.05 (0.74x1.75x0.41) -0.10 m over what is under it
- RAILING ACROSS WALKWAY structure[113] 17.00,2.00..17.06,3.00 z -15.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[114] 17.00,3.00..17.06,4.00 z -15.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[115] 17.00,4.00..17.06,5.00 z -15.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[116] 17.00,5.00..17.06,6.00 z -15.00 (3/3 of it walked on both sides)

### 100 ISOLATION WING (19)

- INSIDE fixture[30] model 223 (med_mattress_stained) at 8.15,1.50 z -14.55 (1.90x0.90x0.12) is inside fixture[54] model 304 (scrap_bedroll) at 8.15,1.50 z -14.55 (0.87x1.86x0.17) (9/27 of it)
- INSIDE fixture[31] model 223 (med_mattress_stained) at 8.15,22.50 z -14.55 (1.90x0.90x0.12) is inside fixture[61] model 304 (scrap_bedroll) at 8.15,22.50 z -14.55 (0.87x1.86x0.17) (9/27 of it)
- INSIDE fixture[41] model 223 (med_mattress_stained) at 14.15,22.50 z -14.55 (1.90x0.90x0.12) is inside fixture[64] model 304 (scrap_bedroll) at 14.15,22.50 z -14.55 (0.87x1.86x0.17) (9/27 of it)
- INSIDE fixture[52] model 223 (med_mattress_stained) at 20.15,1.50 z -14.55 (1.90x0.90x0.12) is inside fixture[57] model 304 (scrap_bedroll) at 20.15,1.50 z -14.55 (0.87x1.86x0.17) (9/27 of it)
- IN DOOR SWING structure[7] 3.00,9.30..3.40,9.50 z -15.00..-11.00 mat 15 of door[0] x 3.40-4.60 at y 9.40
- TRIM ACROSS DOOR structure[39] 9.16,9.50..10.84,9.62 z -13.92..-13.80 mat 8 (trim) of door[1] x 9.40-10.60 at y 9.40
- BLOCKS DOOR APPROACH prop model 191 (lab_desk_lamp) at 16.00,10.10 z -15.00 of door[2] x 15.40-16.60 at y 9.40
- TRIM ACROSS DOOR structure[87] 20.16,9.50..21.84,9.62 z -13.92..-13.80 mat 8 (trim) of door[3] x 20.40-21.60 at y 9.40
- IN DOOR SWING structure[96] 3.00,13.50..3.40,13.70 z -15.00..-11.00 mat 15 of door[4] x 3.40-4.60 at y 13.60
- TRIM ACROSS DOOR structure[109] 3.16,13.38..4.84,13.50 z -13.92..-13.80 mat 8 (trim) of door[4] x 3.40-4.60 at y 13.60
- IN DOOR SWING structure[117] 9.10,13.50..9.40,13.70 z -15.00..-11.00 mat 15 of door[5] x 9.40-10.60 at y 13.60
- TRIM ACROSS DOOR structure[123] 9.10,13.33..10.90,13.50 z -14.38..-14.27 mat 8 (trim) of door[5] x 9.40-10.60 at y 13.60
- TRIM ACROSS DOOR structure[126] 9.10,13.33..10.90,13.50 z -13.68..-13.57 mat 8 (trim) of door[5] x 9.40-10.60 at y 13.60
- TRIM ACROSS DOOR structure[132] 10.44,13.26..10.64,13.50 z -13.95..-13.70 mat 19 (trim) of door[5] x 9.40-10.60 at y 13.60
- TRIM ACROSS DOOR structure[173] 9.16,13.38..10.84,13.50 z -13.92..-13.80 mat 8 (trim) of door[5] x 9.40-10.60 at y 13.60
- TRIM ACROSS DOOR structure[192] 15.16,13.38..16.84,13.50 z -13.92..-13.80 mat 8 (trim) of door[6] x 15.40-16.60 at y 13.60
- TRIM ACROSS DOOR structure[212] 20.30,13.37..21.70,13.50 z -14.15..-14.07 mat 13 (trim) of door[7] x 20.40-21.60 at y 13.60
- TRIM ACROSS DOOR structure[213] 20.30,13.37..21.70,13.50 z -13.45..-13.37 mat 13 (trim) of door[7] x 20.40-21.60 at y 13.60
- TRIM ACROSS DOOR structure[214] 21.50,13.37..21.62,13.50 z -13.10..-12.50 mat 13 (trim) of door[7] x 20.40-21.60 at y 13.60

### 101 OBSERVATION THEATER (5)

- FLOATING STRUCTURE structure[316] 7.00,11.90..17.00,12.10 z -11.60..-11.30 mat 12, 6.40 m over what is under it
- FLOATING STRUCTURE structure[317] 7.00,12.50..17.00,12.70 z -11.60..-11.30 mat 12, 6.40 m over what is under it
- FLOATING STRUCTURE structure[318] 10.84,12.14..11.16,12.46 z -12.15..-11.60 mat 8, 5.85 m over what is under it
- FLOATING STRUCTURE structure[319] 12.84,12.14..13.16,12.46 z -12.15..-11.60 mat 8, 5.85 m over what is under it
- FLOATING STRUCTURE structure[320] 14.64,12.14..14.96,12.46 z -12.15..-11.60 mat 8, 5.85 m over what is under it

### 102 BREACH SECTOR (2)

- INSIDE fixture[154] model 281 (pwr_coolant_main) at 14.00,3.50 z -17.95 (0.50x7.26x0.50) is inside fixture[152] model 278 (pwr_catwalk_section) at 12.60,4.50 z -18.30 (4.00x2.00x1.31) (9/27 of it)
- LAMPS STACKED lights 18 and 27 at 18.00,21.30 z -12.60

### 103 EMERGENCY CONTAINMENT (1)

- IN DOOR SWING structure[69] 3.00,13.20..3.05,13.26 z -15.00..-12.60 mat 13 of door[1] x 3.00-4.20 at y 13.10

### 104 DEEP MATERIALS LAB (5)

- FLOATING FIXTURE fixture[77] model 246 (mine_cage_lamp) at 14.85,16.75 z -12.50 (0.21x0.20x0.36) 2.50 m over what is under it
- FLOATING FIXTURE fixture[87] model 221 (med_fluoro_light) at 22.94,9.60 z -13.45 (0.11x1.20x0.13) 1.55 m over what is under it
- INSIDE fixture[67] model 29 (pc_tower) at 19.52,21.30 z -15.00 (0.20x0.45x0.42) is inside fixture[61] model 24 (desk) at 19.00,21.40 z -15.00 (1.40x0.70x0.75) (27/27 of it)
- DUPLICATE fixture[24] model 376 (res_crystal_s) at 3.71,13.41 z -13.88 (0.10x0.11x0.12) and fixture[25]
- DUPLICATE fixture[48] model 376 (res_crystal_s) at 6.91,22.66 z -13.89 (0.09x0.10x0.12) and fixture[49]

### 106 GEOPHYSICS (3)

- INSIDE fixture[30] model 29 (pc_tower) at 6.22,8.50 z -15.00 (0.20x0.45x0.42) is inside fixture[23] model 24 (desk) at 5.70,8.60 z -15.00 (1.40x0.70x0.75) (27/27 of it)
- INSIDE fixture[54] model 29 (pc_tower) at 6.22,12.50 z -15.00 (0.20x0.45x0.42) is inside fixture[48] model 24 (desk) at 5.70,12.60 z -15.00 (1.40x0.70x0.75) (27/27 of it)
- SCALE fixture[85] model 298 (pwr_tool_chest) at 1.50,17.50 z -15.00 (0.75x0.45x0.97) at x0.20 its own size

### 107 ENERGY CORE RESEARCH (17)

- FLOATING FIXTURE fixture[19] model 278 (pwr_catwalk_section) at 12.00,8.40 z -20.00 (4.00x2.00x1.31) 3.68 m over what is under it
- FLOATING FIXTURE fixture[21] model 278 (pwr_catwalk_section) at 8.40,12.00 z -20.00 (4.00x2.00x1.31) 3.78 m over what is under it
- FLOATING FIXTURE fixture[22] model 278 (pwr_catwalk_section) at 15.60,12.00 z -20.00 (4.00x2.00x1.31) 3.94 m over what is under it
- FLOATING FIXTURE fixture[23] model 278 (pwr_catwalk_section) at 12.00,8.40 z -10.50 (4.00x2.00x1.31) 8.19 m over what is under it
- FLOATING FIXTURE fixture[24] model 278 (pwr_catwalk_section) at 12.00,15.60 z -10.50 (4.00x2.00x1.31) 8.19 m over what is under it
- FLOATING FIXTURE fixture[25] model 278 (pwr_catwalk_section) at 8.40,12.00 z -10.50 (4.00x2.00x1.31) 8.19 m over what is under it
- FLOATING FIXTURE fixture[26] model 278 (pwr_catwalk_section) at 15.60,12.00 z -10.50 (4.00x2.00x1.31) 8.19 m over what is under it
- FLOATING STRUCTURE structure[81] 7.40,7.40..9.40,9.40 z -20.08..-20.00 mat 20, 4.91 m over what is under it
- FLOATING STRUCTURE structure[82] 14.60,7.40..16.60,9.40 z -20.08..-20.00 mat 20, 0.92 m over what is under it
- FLOATING STRUCTURE structure[83] 7.40,14.60..9.40,16.60 z -20.08..-20.00 mat 20, 0.92 m over what is under it
- FLOATING STRUCTURE structure[84] 14.60,14.60..16.60,16.60 z -20.08..-20.00 mat 20, 4.91 m over what is under it
- FLOATING STRUCTURE structure[89] 7.40,7.40..9.40,9.40 z -10.58..-10.50 mat 20, 9.42 m over what is under it
- FLOATING STRUCTURE structure[90] 14.60,7.40..16.60,9.40 z -10.58..-10.50 mat 20, 9.42 m over what is under it
- FLOATING STRUCTURE structure[91] 7.40,14.60..9.40,16.60 z -10.58..-10.50 mat 20, 9.42 m over what is under it
- FLOATING STRUCTURE structure[92] 14.60,14.60..16.60,16.60 z -10.58..-10.50 mat 20, 9.42 m over what is under it
- INSIDE fixture[59] model 45 (valve_wheel) at 11.23,16.20 z -19.70 (0.40x0.12x0.40) is inside fixture[20] model 278 (pwr_catwalk_section) at 12.00,15.60 z -20.00 (4.00x2.00x1.31) (27/27 of it)
- INSIDE fixture[60] model 45 (valve_wheel) at 12.77,16.20 z -19.70 (0.40x0.12x0.40) is inside fixture[20] model 278 (pwr_catwalk_section) at 12.00,15.60 z -20.00 (4.00x2.00x1.31) (27/27 of it)

### 108 DEEP ACCESS CONTROL (13)

- FLOATING FIXTURE fixture[62] model 200 (lab_notice_board) at 3.60,15.23 z -13.70 (1.20x0.07x0.90) 1.30 m over what is under it
- BLOCKS DOOR APPROACH prop model 346 (prop_cardboard_box) at 13.20,1.70 z -15.00 of door[0] x 10.00-14.00 at y 0.50
- IN DOORWAY fixture[75] model 200 (lab_notice_board) at 19.00,13.14 z -13.70 (1.20x0.07x0.90) of door[2] x 17.60-18.80 at y 13.00
- IN DOORWAY fixture[62] model 200 (lab_notice_board) at 3.60,15.23 z -13.70 (1.20x0.07x0.90) of door[3] x 3.00-4.20 at y 15.10
- BLOCKS DOOR APPROACH structure[93] 3.15,13.55..5.85,14.05 z -14.57..-14.49 mat 10 of door[3] x 3.00-4.20 at y 15.10
- BLOCKS DOOR APPROACH structure[94] 3.15,14.00..5.85,14.08 z -14.45..-13.95 mat 10 of door[3] x 3.00-4.20 at y 15.10
- BLOCKS DOOR APPROACH structure[95] 3.44,13.60..3.56,14.00 z -15.00..-14.57 mat 2 of door[3] x 3.00-4.20 at y 15.10
- WALL DEVICE OVERSIZE fixture[0] model 192 (lab_door_reader) at 9.43,15.10 z -15.00 (0.85x0.52x2.39)
- WALL DEVICE OFF WALL fixture[0] model 192 (lab_door_reader) at 9.43,15.10 z -15.00 (0.85x0.52x2.39)
- WALL DEVICE OVERSIZE fixture[1] model 192 (lab_door_reader) at 12.00,15.10 z -15.00 (0.85x0.52x2.39)
- WALL DEVICE OFF WALL fixture[1] model 192 (lab_door_reader) at 12.00,15.10 z -15.00 (0.85x0.52x2.39)
- WALL DEVICE OVERSIZE fixture[2] model 192 (lab_door_reader) at 14.57,15.10 z -15.00 (0.85x0.52x2.39)
- WALL DEVICE OFF WALL fixture[2] model 192 (lab_door_reader) at 14.57,15.10 z -15.00 (0.85x0.52x2.39)

### 109 EXTRACTION INTERFACE (5)

- FLOATING FIXTURE fixture[116] model 246 (mine_cage_lamp) at 12.00,20.62 z -12.71 (0.21x0.20x0.36) 2.17 m over what is under it
- BLOCKS DOOR APPROACH structure[90] 0.00,21.30..10.00,24.00 z -15.00..-8.00 mat 26 of door[0] x 8.00-16.00 at y 23.50
- BLOCKS DOOR APPROACH structure[91] 14.00,21.30..24.00,24.00 z -15.00..-8.00 mat 26 of door[0] x 8.00-16.00 at y 23.50
- STRUCTURE IN DOORWAY structure[109] 10.03,21.70..10.09,23.40 z -15.00..-11.70 mat 32 of door[0] x 8.00-16.00 at y 23.50
- STRUCTURE IN DOORWAY structure[110] 13.91,21.70..13.97,23.40 z -15.00..-11.70 mat 32 of door[0] x 8.00-16.00 at y 23.50

No hits: 92, 97, 105

## Chapter 1 and prologue (0-67)

### 1 PRESSURE WORKS (10)

- FLOATING FIXTURE fixture[15] model 45 (valve_wheel) at 1.13,9.20 z 1.25 (0.32x0.12x0.32) -3.45 m over what is under it
- FLOATING FIXTURE fixture[17] model 45 (valve_wheel) at 1.13,12.60 z 1.25 (0.32x0.12x0.32) -3.45 m over what is under it
- FLOATING FIXTURE fixture[19] model 45 (valve_wheel) at 1.13,15.40 z 1.25 (0.32x0.12x0.32) -3.45 m over what is under it
- FLOATING FIXTURE fixture[21] model 45 (valve_wheel) at 22.87,9.60 z 1.25 (0.32x0.12x0.32) -3.45 m over what is under it
- FLOATING FIXTURE fixture[23] model 45 (valve_wheel) at 22.87,13.60 z 1.25 (0.32x0.12x0.32) -3.45 m over what is under it
- INSIDE fixture[15] model 45 (valve_wheel) at 1.13,9.20 z 1.25 (0.32x0.12x0.32) is inside fixture[14] model 44 (pipe_riser) at 1.06,9.20 z 0.00 (0.40x0.10x4.70) (9/27 of it)
- INSIDE fixture[17] model 45 (valve_wheel) at 1.13,12.60 z 1.25 (0.32x0.12x0.32) is inside fixture[16] model 44 (pipe_riser) at 1.06,12.60 z 0.00 (0.40x0.10x4.70) (9/27 of it)
- INSIDE fixture[19] model 45 (valve_wheel) at 1.13,15.40 z 1.25 (0.32x0.12x0.32) is inside fixture[18] model 44 (pipe_riser) at 1.06,15.40 z 0.00 (0.40x0.10x4.70) (9/27 of it)
- INSIDE fixture[21] model 45 (valve_wheel) at 22.87,9.60 z 1.25 (0.32x0.12x0.32) is inside fixture[20] model 44 (pipe_riser) at 22.94,9.60 z 0.00 (0.40x0.10x4.70) (9/27 of it)
- INSIDE fixture[23] model 45 (valve_wheel) at 22.87,13.60 z 1.25 (0.32x0.12x0.32) is inside fixture[22] model 44 (pipe_riser) at 22.94,13.60 z 0.00 (0.40x0.10x4.70) (9/27 of it)

### 2 TURBINE GANTRY (2)

- FLOATING FIXTURE fixture[1] model 45 (valve_wheel) at 10.60,10.35 z 3.05 (0.32x0.10x0.32) 2.05 m over what is under it
- BLOCKS DOOR APPROACH structure[68] 22.78,17.75..23.00,18.25 z 0.00..10.40 mat 15 of door[2] x 20.00-23.00 at y 17.50

### 3 REACTOR COMPLEX (8)

- FLOATING FIXTURE fixture[32] model 45 (valve_wheel) at 17.40,18.50 z -4.40 (0.32x0.10x0.32) 0.20 m over what is under it
- FLOATING FIXTURE fixture[33] model 45 (valve_wheel) at 18.80,18.50 z -4.40 (0.32x0.10x0.32) 0.20 m over what is under it
- FLOATING FIXTURE fixture[34] model 45 (valve_wheel) at 20.20,18.50 z -4.40 (0.32x0.10x0.32) 0.20 m over what is under it
- FLOATING FIXTURE fixture[35] model 45 (valve_wheel) at 17.40,21.70 z -4.40 (0.32x0.10x0.32) 0.20 m over what is under it
- FLOATING FIXTURE fixture[36] model 45 (valve_wheel) at 18.80,21.70 z -4.40 (0.32x0.10x0.32) 0.20 m over what is under it
- FLOATING FIXTURE fixture[37] model 45 (valve_wheel) at 20.20,21.70 z -4.40 (0.32x0.10x0.32) 0.20 m over what is under it
- RAILING ACROSS WALKWAY structure[44] 10.95,8.00..11.00,8.94 z 0.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[45] 13.00,8.00..13.05,8.94 z 0.00 (3/3 of it walked on both sides)

### 4 SERVICE GALLERY (2)

- FLOATING FIXTURE fixture[8] model 45 (valve_wheel) at 22.87,2.50 z -7.75 (0.32x0.12x0.32) -1.85 m over what is under it
- INSIDE fixture[8] model 45 (valve_wheel) at 22.87,2.50 z -7.75 (0.32x0.12x0.32) is inside fixture[7] model 44 (pipe_riser) at 22.94,2.50 z -9.00 (0.40x0.10x3.10) (9/27 of it)

### 7 PUMP ANNEX (2)

- RAILING ACROSS WALKWAY structure[230] 18.00,15.00..18.06,16.00 z -9.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[242] 17.00,16.00..18.00,16.06 z -9.00 (2/3 of it walked on both sides)

### 8 UTILITY JUNCTION (2)

- RAILING ACROSS WALKWAY structure[102] 16.94,1.00..17.00,2.00 z -4.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[112] 16.94,2.00..17.00,3.00 z -4.00 (3/3 of it walked on both sides)

### 9 WASTE HANDLING (1)

- BLOCKS DOOR APPROACH fixture[10] model 7 (metal_shelf) at 22.35,22.00 z -12.00 (1.80x0.50x1.80) of door[1] x 20.00-23.00 at y 23.50

### 10 FREIGHT ACCESS (5)

- FLOATING FIXTURE fixture[0] model 8 (wall_box) at 6.45,2.70 z -10.90 (0.67x0.20x0.91) 1.10 m over what is under it
- FLOATING STRUCTURE structure[64] 22.45,6.10..22.60,6.50 z -11.60..-11.10 mat 8, 0.37 m over what is under it
- FLOATING STRUCTURE structure[65] 22.45,9.50..22.60,9.90 z -11.60..-11.10 mat 8, 0.37 m over what is under it
- RAILING ACROSS WALKWAY structure[45] 1.00,14.00..2.80,14.06 z -12.00 (4/6 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[46] 4.20,14.00..5.90,14.06 z -12.00 (5/5 of it walked on both sides)

### 13 RECEIVING OVERLOOK (5)

- FLOATING STRUCTURE structure[126] 11.80,11.45..12.20,11.85 z -12.40..-12.00 mat 8, 12.59 m over what is under it
- FLOATING STRUCTURE structure[146] 8.85,22.45..9.05,22.60 z -24.60..-24.10 mat 8, 0.37 m over what is under it
- FLOATING STRUCTURE structure[147] 10.95,22.45..11.15,22.60 z -24.60..-24.10 mat 8, 0.37 m over what is under it
- FLOATING STRUCTURE structure[165] 12.85,22.45..13.05,22.60 z -24.60..-24.10 mat 8, 0.37 m over what is under it
- FLOATING STRUCTURE structure[166] 14.95,22.45..15.15,22.60 z -24.60..-24.10 mat 8, 0.37 m over what is under it

### 14 RECEIVING LANES (7)

- RAILING ACROSS WALKWAY structure[112] 12.72,11.00..13.70,11.06 z -24.90 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[113] 16.54,11.12..16.60,15.08 z -24.90 (12/13 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[114] 12.60,11.12..12.66,15.08 z -24.90 (13/13 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[115] 12.72,15.14..13.70,15.20 z -24.90 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[116] 15.52,15.14..16.48,15.20 z -24.90 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[299] 13.82,11.00..15.40,11.06 z -24.90 (5/5 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[300] 15.52,11.00..16.48,11.06 z -24.90 (3/3 of it walked on both sides)

### 15 WAREHOUSE INTAKE (17)

- FLOATING FIXTURE fixture[0] model 8 (wall_box) at 18.30,22.70 z -24.00 (0.75x0.15x1.00) 1.00 m over what is under it
- FLOATING STRUCTURE structure[12] 6.60,22.76..6.66,22.79 z -24.30..-24.24 mat 8, 0.70 m over what is under it
- FLOATING STRUCTURE structure[13] 6.60,22.76..6.66,22.79 z -20.00..-19.94 mat 8, 4.24 m over what is under it
- FLOATING STRUCTURE structure[14] 7.60,22.76..7.66,22.79 z -24.30..-24.24 mat 8, 0.70 m over what is under it
- FLOATING STRUCTURE structure[15] 7.60,22.76..7.66,22.79 z -20.00..-19.94 mat 8, 4.24 m over what is under it
- FLOATING STRUCTURE structure[16] 8.60,22.76..8.66,22.79 z -24.30..-24.24 mat 8, 0.70 m over what is under it
- FLOATING STRUCTURE structure[17] 8.60,22.76..8.66,22.79 z -20.00..-19.94 mat 8, 4.24 m over what is under it
- FLOATING STRUCTURE structure[18] 9.60,22.76..9.66,22.79 z -24.30..-24.24 mat 8, 0.70 m over what is under it
- FLOATING STRUCTURE structure[19] 9.60,22.76..9.66,22.79 z -20.00..-19.94 mat 8, 4.24 m over what is under it
- FLOATING STRUCTURE structure[28] 13.75,22.76..13.81,22.79 z -24.30..-24.24 mat 8, 0.70 m over what is under it
- FLOATING STRUCTURE structure[29] 13.75,22.76..13.81,22.79 z -20.00..-19.94 mat 8, 4.24 m over what is under it
- FLOATING STRUCTURE structure[30] 14.75,22.76..14.81,22.79 z -24.30..-24.24 mat 8, 0.70 m over what is under it
- FLOATING STRUCTURE structure[31] 14.75,22.76..14.81,22.79 z -20.00..-19.94 mat 8, 4.24 m over what is under it
- FLOATING STRUCTURE structure[32] 15.75,22.76..15.81,22.79 z -24.30..-24.24 mat 8, 0.70 m over what is under it
- FLOATING STRUCTURE structure[33] 15.75,22.76..15.81,22.79 z -20.00..-19.94 mat 8, 4.24 m over what is under it
- FLOATING STRUCTURE structure[34] 16.75,22.76..16.81,22.79 z -24.30..-24.24 mat 8, 0.70 m over what is under it
- FLOATING STRUCTURE structure[35] 16.75,22.76..16.81,22.79 z -20.00..-19.94 mat 8, 4.24 m over what is under it

### 17 WAREHOUSE / C BLOCK (5)

- RAILING ACROSS WALKWAY structure[62] 15.00,1.10..15.06,4.80 z -25.00 (12/12 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[63] 15.10,8.54..17.40,8.60 z -25.00 (7/7 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[64] 19.10,8.54..22.70,8.60 z -25.00 (12/12 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[65] 19.00,8.64..19.06,10.00 z -25.00 (4/4 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[226] 15.00,4.90..15.06,8.50 z -25.00 (12/12 of it walked on both sides)

### 24 TRANSFER / RAIL YARD (1)

- FLOATING STRUCTURE structure[140] 14.10,22.85..15.50,22.95 z -24.00..-23.92 mat 12, 1.00 m over what is under it

### 28 DEPOT / INSPECTION PITS (2)

- RAILING ACROSS WALKWAY structure[324] 17.94,17.00..18.00,18.00 z -25.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[325] 20.00,17.00..20.06,18.00 z -25.00 (3/3 of it walked on both sides)

### 31 CARGO / DEEP JUNCTION (3)

- RAILING ACROSS WALKWAY structure[157] 9.20,17.60..10.60,17.66 z -25.00 (4/4 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[158] 12.20,17.60..14.00,17.66 z -25.00 (6/6 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[159] 13.94,17.60..14.00,23.00 z -25.00 (18/18 of it walked on both sides)

### 35 DISTRIBUTION / HUB (1)

- FLOATING FIXTURE fixture[14] model 209 (lab_status_display) at 1.08,19.60 z -23.10 (0.69x0.06x0.20) 1.90 m over what is under it

### 36 SUBSTATION A (1)

- SCALE fixture[0] model 298 (pwr_tool_chest) at 2.00,21.90 z -25.00 (0.75x0.45x0.97) at x0.20 its own size

### 38 CABLE TRENCH (4)

- FLOATING FIXTURE fixture[0] model 277 (pwr_cable_bundle) at 2.00,18.82 z -27.20 (2.40x0.14x0.22) 0.80 m over what is under it
- FLOATING FIXTURE fixture[1] model 277 (pwr_cable_bundle) at 4.50,18.82 z -27.20 (2.40x0.14x0.22) 0.80 m over what is under it
- FLOATING FIXTURE fixture[2] model 277 (pwr_cable_bundle) at 7.00,18.82 z -27.20 (2.40x0.14x0.22) 0.80 m over what is under it
- FLOATING FIXTURE fixture[4] model 277 (pwr_cable_bundle) at 12.00,18.82 z -27.20 (2.40x0.14x0.22) 0.80 m over what is under it

### 40 LOAD CONTROL (8)

- LAMPS STACKED lights 5 and 6 at 4.00,7.00 z -15.60
- LAMPS STACKED lights 7 and 8 at 12.00,7.00 z -15.60
- LAMPS STACKED lights 9 and 10 at 4.00,13.00 z -15.60
- LAMPS STACKED lights 11 and 12 at 12.00,13.00 z -15.60
- LAMPS STACKED lights 13 and 14 at 20.00,13.00 z -15.60
- LAMPS STACKED lights 15 and 16 at 4.00,19.00 z -15.60
- LAMPS STACKED lights 17 and 18 at 12.00,19.00 z -15.60
- LAMPS STACKED lights 19 and 20 at 20.00,19.00 z -15.60

### 41 MACHINE HALL / NORTH GALLERY (6)

- FLOATING FIXTURE fixture[0] model 300 (pwr_vent_grille) at 9.00,1.12 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING FIXTURE fixture[1] model 300 (pwr_vent_grille) at 15.00,1.12 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING FIXTURE fixture[2] model 300 (pwr_vent_grille) at 21.00,1.12 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- SCALE fixture[0] model 300 (pwr_vent_grille) at 9.00,1.12 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[1] model 300 (pwr_vent_grille) at 15.00,1.12 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[2] model 300 (pwr_vent_grille) at 21.00,1.12 z -9.00 (2.05x0.10x2.00) at x2.50 its own size

### 42 MACHINE HALL / EXCITER BAY (8)

- FLOATING FIXTURE fixture[0] model 300 (pwr_vent_grille) at 3.00,1.12 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING FIXTURE fixture[1] model 300 (pwr_vent_grille) at 9.00,1.12 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING FIXTURE fixture[2] model 300 (pwr_vent_grille) at 15.00,1.12 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- RAILING ACROSS WALKWAY structure[347] 7.00,1.00..7.05,2.00 z -25.00 (2/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[349] 7.00,3.00..7.05,4.00 z -25.00 (2/3 of it walked on both sides)
- SCALE fixture[0] model 300 (pwr_vent_grille) at 3.00,1.12 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[1] model 300 (pwr_vent_grille) at 9.00,1.12 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[2] model 300 (pwr_vent_grille) at 15.00,1.12 z -9.00 (2.05x0.10x2.00) at x2.50 its own size

### 43 MACHINE HALL / TURBINE FLOOR (8)

- FLOATING FIXTURE fixture[0] model 300 (pwr_vent_grille) at 9.00,22.88 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING FIXTURE fixture[1] model 300 (pwr_vent_grille) at 15.00,22.88 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING FIXTURE fixture[2] model 300 (pwr_vent_grille) at 21.00,22.88 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING STRUCTURE structure[294] 4.48,10.26..4.88,10.66 z -22.75..-22.45 mat 8, 0.25 m over what is under it
- FLOATING STRUCTURE structure[296] 19.73,15.53..20.13,15.93 z -22.75..-22.45 mat 8, 0.25 m over what is under it
- SCALE fixture[0] model 300 (pwr_vent_grille) at 9.00,22.88 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[1] model 300 (pwr_vent_grille) at 15.00,22.88 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[2] model 300 (pwr_vent_grille) at 21.00,22.88 z -9.00 (2.05x0.10x2.00) at x2.50 its own size

### 44 MACHINE HALL / CRANE END (7)

- FLOATING FIXTURE fixture[0] model 300 (pwr_vent_grille) at 3.00,22.88 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING FIXTURE fixture[2] model 300 (pwr_vent_grille) at 15.00,22.88 z -9.00 (2.05x0.10x2.00) 1.70 m over what is under it
- FLOATING STRUCTURE structure[304] 9.83,8.90..10.23,9.30 z -25.75..-25.45 mat 8, 0.25 m over what is under it
- SCALE fixture[0] model 300 (pwr_vent_grille) at 3.00,22.88 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[1] model 300 (pwr_vent_grille) at 9.00,22.88 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[2] model 300 (pwr_vent_grille) at 15.00,22.88 z -9.00 (2.05x0.10x2.00) at x2.50 its own size
- SCALE fixture[10] model 268 (mine_tool_chest) at 12.50,2.00 z -28.00 (0.75x0.45x0.97) at x0.20 its own size

### 46 AIR HANDLING (7)

- FLOATING STRUCTURE structure[13] 22.40,2.60..22.90,2.66 z -21.80..-19.60 mat 8, 0.20 m over what is under it
- FLOATING STRUCTURE structure[14] 22.40,3.50..22.90,3.56 z -21.80..-19.60 mat 8, 0.20 m over what is under it
- FLOATING STRUCTURE structure[15] 22.40,4.40..22.90,4.46 z -21.80..-19.60 mat 8, 0.20 m over what is under it
- FLOATING STRUCTURE structure[16] 22.40,5.30..22.90,5.36 z -21.80..-19.60 mat 8, 0.20 m over what is under it
- FLOATING STRUCTURE structure[17] 22.40,6.20..22.90,6.26 z -21.80..-19.60 mat 8, 0.20 m over what is under it
- FLOATING STRUCTURE structure[18] 22.40,7.10..22.90,7.16 z -21.80..-19.60 mat 8, 0.20 m over what is under it
- FLOATING STRUCTURE structure[19] 22.40,8.00..22.90,8.06 z -21.80..-19.60 mat 8, 0.20 m over what is under it

### 47 WATER TREATMENT (1)

- FLOATING FIXTURE fixture[4] model 201 (lab_pa_horn) at 14.60,1.36 z -19.95 (0.49x0.56x0.43) 2.05 m over what is under it

### 48 DRAINAGE GALLERY (1)

- SCREEN FACES WALL terminal 'DRAINAGE / P-4 LOCAL' at 22.20,19.30 (wall 0.30 m in front of it)

### 49 SUMP WORKS (4)

- FLOATING STRUCTURE structure[65] 10.85,3.05..11.35,3.55 z -21.90..-21.60 mat 18, 3.10 m over what is under it
- RAILING ACROSS WALKWAY structure[242] 4.00,8.97..5.00,9.03 z -25.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[245] 7.00,8.97..8.00,9.03 z -25.00 (3/3 of it walked on both sides)
- RAILING ACROSS WALKWAY structure[248] 10.00,8.97..11.00,9.03 z -25.00 (3/3 of it walked on both sides)

### 53 MAINTENANCE DISTRICT (6)

- FLOATING STRUCTURE structure[130] 7.70,18.55..7.80,18.65 z -17.38..-17.30 mat 8, 0.32 m over what is under it
- STRUCTURE IN DOORWAY structure[185] 2.84,13.35..3.04,15.85 z -18.05..-17.26 mat 2 of door[2] x 2.40-3.60 at y 15.90
- BLOCKS DOOR APPROACH structure[187] 2.90,15.52..2.98,15.60 z -19.00..-18.05 mat 12 of door[2] x 2.40-3.60 at y 15.90
- BLOCKS DOOR APPROACH fixture[64] model 217 (med_crash_cart) at 21.60,14.90 z -19.00 (0.74x0.48x1.00) of door[3] x 20.60-21.80 at y 15.90
- SCALE fixture[55] model 298 (pwr_tool_chest) at 17.00,22.60 z -19.00 (0.75x0.45x0.97) at x0.20 its own size
- SCALE fixture[56] model 298 (pwr_tool_chest) at 18.00,22.60 z -19.00 (0.75x0.45x0.97) at x0.20 its own size

### 54 CENTRAL STORES (6)

- FLOATING FIXTURE fixture[96] model 21 (packing_crate) at 12.10,5.75 z -16.95 (0.60x0.42x0.40) 2.05 m over what is under it
- FLOATING STRUCTURE structure[242] 11.98,5.63..12.22,5.87 z -16.42..-16.20 mat 18, 2.58 m over what is under it
- DUPLICATE fixture[115] model 294 (pwr_sheet_steel_stack) at 14.60,21.75 z -19.00 (1.20x2.28x0.09) and fixture[116]
- SCALE fixture[113] model 68 (cable_reel) at 8.40,21.60 z -19.00 (0.45x0.90x0.90) at x0.45 its own size
- SCALE fixture[114] model 68 (cable_reel) at 8.40,22.50 z -19.00 (0.45x0.90x0.90) at x0.45 its own size
- SCALE fixture[130] model 68 (cable_reel) at 5.70,17.60 z -19.00 (0.45x0.90x0.90) at x0.45 its own size

### 55 FABRICATION SHOP (15)

- FLOATING FIXTURE fixture[21] model 219 (med_first_aid_box) at 14.70,11.75 z -17.60 (0.37x0.12x0.35) 1.37 m over what is under it
- FLOATING FIXTURE fixture[22] model 221 (med_fluoro_light) at 12.00,3.00 z -15.40 (0.11x1.20x0.13) 3.60 m over what is under it
- FLOATING FIXTURE fixture[23] model 221 (med_fluoro_light) at 12.00,9.00 z -15.40 (0.11x1.20x0.13) 3.60 m over what is under it
- FLOATING FIXTURE fixture[24] model 221 (med_fluoro_light) at 12.00,15.00 z -15.40 (0.11x1.20x0.13) 3.60 m over what is under it
- FLOATING FIXTURE fixture[25] model 221 (med_fluoro_light) at 12.00,21.00 z -15.40 (0.11x1.20x0.13) 3.60 m over what is under it
- FLOATING FIXTURE fixture[26] model 221 (med_fluoro_light) at 5.00,3.00 z -15.40 (0.11x1.20x0.13) 1.00 m over what is under it
- FLOATING FIXTURE fixture[27] model 221 (med_fluoro_light) at 5.00,9.00 z -15.40 (0.11x1.20x0.13) 3.60 m over what is under it
- FLOATING FIXTURE fixture[28] model 221 (med_fluoro_light) at 5.00,15.00 z -15.40 (0.11x1.20x0.13) 3.60 m over what is under it
- FLOATING FIXTURE fixture[29] model 221 (med_fluoro_light) at 5.00,21.00 z -15.40 (0.11x1.20x0.13) 3.60 m over what is under it
- FLOATING FIXTURE fixture[30] model 221 (med_fluoro_light) at 19.50,4.50 z -15.40 (0.11x1.20x0.13) 2.66 m over what is under it
- FLOATING FIXTURE fixture[31] model 221 (med_fluoro_light) at 19.50,13.50 z -15.40 (0.11x1.20x0.13) 3.34 m over what is under it
- FLOATING FIXTURE fixture[32] model 221 (med_fluoro_light) at 19.50,20.00 z -15.40 (0.11x1.20x0.13) 3.60 m over what is under it
- FLOATING STRUCTURE structure[77] 2.50,8.10..4.30,8.60 z -17.95..-17.85 mat 12, 0.15 m over what is under it
- SCALE fixture[12] model 268 (mine_tool_chest) at 22.45,15.60 z -19.00 (0.75x0.45x0.97) at x0.20 its own size
- SCALE fixture[13] model 268 (mine_tool_chest) at 22.45,16.50 z -19.00 (0.75x0.45x0.97) at x0.20 its own size

### 57 OPERATIONS CORRIDOR (6)

- FLOATING FIXTURE fixture[77] model 209 (lab_status_display) at 12.00,22.27 z -16.48 (0.69x0.06x0.20) 2.50 m over what is under it
- FLOATING STRUCTURE structure[268] 22.10,19.10..22.12,19.60 z -18.40..-17.40 mat 16, 0.58 m over what is under it
- WALL DEVICE OVERSIZE fixture[75] model 195 (lab_id_scanner) at 9.63,14.95 z -17.90 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[75] model 195 (lab_id_scanner) at 9.63,14.95 z -17.90 (0.30x0.06x0.41)
- WALL DEVICE OVERSIZE fixture[76] model 195 (lab_id_scanner) at 14.37,14.90 z -17.90 (0.30x0.06x0.41)
- WALL DEVICE OFF WALL fixture[76] model 195 (lab_id_scanner) at 14.37,14.90 z -17.90 (0.30x0.06x0.41)

### 60 CHECKPOINT DELTA (1)

- WALL DEVICE OVERSIZE fixture[6] model 195 (lab_id_scanner) at 9.20,5.20 z -17.90 (0.30x0.06x0.41)

### 62 PERSONNEL TRANSIT (1)

- FLOATING STRUCTURE structure[43] 10.40,4.85..13.60,5.15 z -16.42..-15.73 mat 2, 2.58 m over what is under it

### 64 RESEARCH RECEIVING (5)

- FLOATING FIXTURE fixture[49] model 208 (lab_specimen_crate) at 12.00,10.85 z -16.02 (1.12x1.12x1.12) 2.98 m over what is under it
- FLOATING STRUCTURE structure[113] 4.25,15.00..5.30,15.20 z -18.50..-18.38 mat 19, 0.50 m over what is under it
- IN DOOR SWING structure[120] 15.12,4.17..15.20,4.43 z -19.00..-16.80 mat 20 of door[0] x 15.20-16.20 at y 4.30
- WALL DEVICE OVERSIZE fixture[45] model 192 (lab_door_reader) at 19.45,5.64 z -19.00 (0.85x0.52x2.39)
- WALL DEVICE OFF WALL fixture[45] model 192 (lab_door_reader) at 19.45,5.64 z -19.00 (0.85x0.52x2.39)

### 65 DECONTAMINATION (4)

- FLOATING STRUCTURE structure[125] 9.75,16.97..9.83,16.99 z -14.75..-14.66 mat 16, 1.25 m over what is under it
- FLOATING STRUCTURE structure[126] 9.90,16.97..9.98,16.99 z -14.45..-14.36 mat 16, 1.55 m over what is under it
- FLOATING STRUCTURE structure[127] 10.15,16.97..10.23,16.99 z -14.70..-14.61 mat 16, 1.30 m over what is under it
- FLOATING STRUCTURE structure[129] 13.70,16.97..14.00,16.99 z -14.65..-14.40 mat 16, 1.35 m over what is under it

### 66 ENERGY RESEARCH / FORECOURT (3)

- INSIDE fixture[6] model 203 (lab_potted_plant) at 9.00,3.20 z -16.00 (0.37x0.43x0.69) is inside fixture[5] model 193 (lab_exec_desk) at 7.80,3.60 z -16.00 (2.20x1.10x0.90) (6/27 of it)
- SCALE fixture[0] model 185 (lab_anomaly_relic) at 12.00,12.00 z -14.95 (1.50x1.20x1.20) at x3.00 its own size
- WALK-THROUGH fixture[0] model 185 (lab_anomaly_relic) at 12.00,12.00 z -14.95 (1.50x1.20x1.20) (9/9 of its footprint walked)

### 67 ENERGY RESEARCH / GREAT DOOR (3)

- FLOATING STRUCTURE structure[44] 9.41,20.51..9.59,20.69 z -11.72..-11.62 mat 20, 0.28 m over what is under it
- FLOATING STRUCTURE structure[45] 14.41,20.51..14.59,20.69 z -11.72..-11.62 mat 20, 0.28 m over what is under it
- LAMPS STACKED lights 23 and 24 at 12.00,21.00 z -9.60

No hits: 0, 5, 6, 11, 12, 16, 18, 19, 20, 21, 22, 23, 25, 26, 27, 29, 30, 32, 33, 34, 37, 39, 45, 50, 51, 52, 56, 58, 59, 61, 63

## Placeholder and later chunks (110-179)

### 179 SECURITY OFFICE (7)

- STACKED fixture[2] model 30 (cctv_tv) at 2.75,2.42 z 9.71 (0.83x0.63x0.91) on fixture[0] model 31 (file_cabinet_wide) at 3.30,2.45 z 9.00 (2.40x0.70x0.71)
- STACKED fixture[3] model 30 (cctv_tv) at 3.85,2.42 z 9.71 (0.83x0.63x0.91) on fixture[0] model 31 (file_cabinet_wide) at 3.30,2.45 z 9.00 (2.40x0.70x0.71)
- STACKED fixture[4] model 30 (cctv_tv) at 5.35,2.42 z 9.71 (0.83x0.63x0.91) on fixture[1] model 31 (file_cabinet_wide) at 5.90,2.45 z 9.00 (2.40x0.70x0.71)
- STACKED fixture[5] model 30 (cctv_tv) at 6.45,2.42 z 9.71 (0.83x0.63x0.91) on fixture[1] model 31 (file_cabinet_wide) at 5.90,2.45 z 9.00 (2.40x0.70x0.71)
- STACKED fixture[8] model 27 (crt_monitor) at 4.60,4.15 z 9.89 (0.60x0.53x0.57) on fixture[6] model 24 (desk) at 4.60,4.30 z 9.00 (2.20x1.09x0.89)
- STACKED fixture[10] model 27 (crt_monitor) at 7.90,3.60 z 9.89 (0.60x0.53x0.57) on fixture[7] model 25 (desk_corner) at 7.50,4.10 z 9.00 (2.20x2.21x0.89)
- INSIDE fixture[11] model 29 (pc_tower) at 5.35,4.00 z 9.00 (0.26x0.61x0.61) is inside fixture[6] model 24 (desk) at 4.60,4.30 z 9.00 (2.20x1.09x0.89) (27/27 of it)

No hits: 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178

