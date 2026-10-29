---
tags: [design, assets]
date: 2026-10-25
---

# Descent chapter models

Imported from the purchased packs on 2026-10-25 and named for what they are in
the facility. Use them as `model::name` (src/world/ModelIds.h). Sizes are the
model's own metres; fixtures stretch the mesh to the width/depth/height you
give, so scale all three alike. Facing notes are relative to the model;
check in game, since packs differ.

## Power, water and maintenance

| Model | Slot | Size (m) | What it is | Placement | Facing |
| --- | --- | --- | --- | --- | --- |
| pwr_air_handler | 272 | 0.81 x 1.64 x 2.62 | air-handling unit for the ventilation works / scrubber plant | floor, solid | broad faces on Â±X, both detailed |
| pwr_ammeter_box | 273 | 0.23 x 0.10 x 0.37 | ammeter / load-meter box | wall | front +Z, back -Z on the wall |
| pwr_breaker_lever | 274 | 0.31 x 0.20 x 0.32 | wall throw-lever: the main breaker / "restore power" lever (the engine has no lever prop yet) | wall, candidate for an interactive prop | front +Z (lever side) |
| pwr_breaker_panel | 275 | 0.18 x 0.10 x 0.46 | closed breaker/distribution panel | wall | front +Z; the door normal points inward (rendering is double-sided, but the door may light dark) |
| pwr_breaker_panel_open | 276 | 0.32 x 0.23 x 0.46 | the same panel with its door open and the breakers showing (being worked on or sabotaged) | wall | front +Z; the door swings out toward +X, and the box sits in the -X part of the footprint |
| pwr_cable_bundle | 277 | 4.00 x 0.14 x 0.22 | 4 m power-cable bundle for walls, trays and cable trenches | wall or trench floor, not solid; tiles end to end | runs along X |
| pwr_catwalk_section | 278 | 4.00 x 2.00 x 1.31 | grated catwalk section with side rails (switch hall, cooling tower) | raised deck, solid; the walking surface is 0.10 m above the base | runs along X with rails on the Â±Z edges; chain sections along X |
| pwr_circulation_pump | 279 | 0.84 x 0.50 x 0.39 | motor-driven circulation pump (water treatment, heat-exchanger loop) | floor, solid | shaft along X |
| pwr_control_cabinet | 280 | 0.80 x 0.40 x 1.60 | floor-standing load-control cabinet | floor, solid, against a wall | front +Z |
| pwr_coolant_main | 281 | 0.50 x 7.26 x 0.50 | 0.5 m cooling-water main, or a round duct run | on supports, or hung from wall/ceiling; solid | runs along Z; base at 0 |
| pwr_diesel_genset | 282 | 2.29 x 1.07 x 1.22 | emergency diesel generator set | floor, solid | long axis X; both Â±Z sides are textured |
| pwr_disconnect_switch | 283 | 0.43 x 0.35 x 0.85 | fused disconnect switch with throw handle | wall, not solid | front +Z, flat back -Z on the wall; handle on the +X side |
| pwr_duct_fan_unit | 284 | 0.79 x 1.49 x 1.95 | wall-mounted duct fan / exhaust housing | wall-mounted or on the floor against a wall, solid | flat back -X, front +X |
| pwr_fire_cabinet | 285 | 0.25 x 0.20 x 0.56 | wall fire-extinguisher cabinet | wall | front +Z |
| pwr_gate_valve | 286 | 0.49 x 0.28 x 0.33 | inline gate valve with handwheel | on pipe runs | inline along X |
| pwr_generator_unit | 287 | 1.13 x 2.61 x 1.67 | larger standby generator skid (a second genset design for variety) | floor, solid | long axis Z; service side +X |
| pwr_handrail | 288 | 0.08 x 4.00 x 1.20 | 4 m safety handrail for platform and trench edges | floor edge, solid (thin) | runs along Z |
| pwr_lift_station | 289 | 2.04 x 2.00 x 2.14 | concrete sump / lift-station vault with manhole and pump-control riser | floor, solid; the top can be walked on | riser and two disconnects on the -X edge; manhole on top |
| pwr_locker_bank | 290 | 0.87 x 0.85 x 2.42 | steel locker unit for the maintenance break room | floor, solid, against a wall | doors +X, flat back -X |
| pwr_pipe_elbow | 291 | 0.42 x 0.28 x 0.40 | matching 90Â° elbow | anywhere | bends in the Xâ€“Y plane |
| pwr_pipe_straight | 292 | 0.99 x 0.28 x 0.27 | small-bore process pipe for water treatment | anywhere | runs along X |
| pwr_safety_bollard | 293 | 0.20 x 0.20 x 1.47 | yellow steel bollard guarding transformers and gensets | floor, solid | round |
| pwr_sheet_steel_stack | 294 | 1.79 x 3.40 x 0.13 | stack of steel plate stock for the fab shop / central stores | floor, solid, low | any yaw |
| pwr_step_transformer | 295 | 0.64 x 0.89 x 1.67 | dry-type step-down transformer for the substation and unit substations | floor, solid | detailed front +X, plain -X |
| pwr_switchgear_cabinet | 296 | 1.50 x 0.82 x 2.70 | switchgear section for the switch hall lineup | floor, solid, back against a wall | front +Z; it has no back face (-Z is open), so never leave the back exposed |
| pwr_tall_switch_cabinet | 297 | 0.95 x 1.54 x 3.02 | tall relay/protection cabinet; rows of it can also stand in for the battery-bank cabinets | floor, solid | broad door face on +X (denser UVs), back -X |
| pwr_tool_chest | 298 | 3.73 x 2.23 x 4.85 | rolling mechanic's tool chest for the fabrication shop | floor, solid | drawers +Z |
| pwr_turbine_housing | 299 | 2.50 x 2.50 x 1.82 | turbine casing (turbine hall or cooling-tower fan stack) | floor, solid | close to symmetric; no bottom face |
| pwr_vent_grille | 300 | 0.82 x 0.04 x 0.80 | ventilation grille for walls or ducts | wall (ceiling if rotated) | faces +Z |
| pwr_wall_ladder | 301 | 0.08 x 0.84 x 2.70 | fixed steel access ladder | wall, climbable | thin in X; either Â±X side can face the wall; rungs every 0.4 m |

## Medical

| Model | Slot | Size (m) | What it is | Placement | Facing |
| --- | --- | --- | --- | --- | --- |
| med_autopsy_table | 212 | 2.20 x 0.90 x 1.05 | morgue autopsy and dissection slab | floor, solid | long axis on X; the raised drain end is at +X |
| med_bed_frame_wrecked | 213 | 1.98 x 0.94 x 0.84 | stripped bed frame with broken and missing slats | floor, solid | long axis on X |
| med_blood_bag | 214 | 0.11 x 0.02 x 0.14 | "DONOR O" transfusion blood bag | cart or table, or hung up; non-solid | label faces OBJ +Z (Blender -Y) |
| med_cell_bars | 215 | 2.50 x 0.04 x 2.40 | steel grid grille for the front of a quarantine isolation cell or an observation window | floor-standing wall panel, solid | plane is X-Z; same from both sides |
| med_cell_bars_doorway | 216 | 2.50 x 0.04 x 2.40 | isolation-cell bar front with an open doorway | floor-standing wall panel; solid sides with a 1.17 m gap in the middle (X -0.58 to +0.58) | plane is X-Z; same from both sides |
| med_crash_cart | 217 | 0.74 x 0.48 x 1.00 | rusted two-tier wheeled crash/instrument cart | floor, solid | near-symmetric |
| med_crutches | 218 | 0.17 x 0.06 x 1.20 | pair of steel crutches | floor, leaning on a wall; non-solid | flat side faces OBJ +Z (Blender -Y) |
| med_first_aid_box | 219 | 0.37 x 0.12 x 0.35 | wall-mounted "FIRST AID KIT" box | wall at about 1.4 m, non-solid | front faces OBJ +Z (Blender -Y) |
| med_floor_drain | 220 | 0.50 x 0.50 x 0.01 | grimy floor drain grate for the morgue, surgery and showers | floor decal, non-solid | symmetric |
| med_fluoro_light | 221 | 0.11 x 1.20 x 0.13 | single-tube fluorescent ceiling fitting | ceiling, non-solid; place at ceiling height minus 0.127 | long axis on Y; turned over so the tube hangs down; z=0 is the bottom of the tube |
| med_holding_cage | 222 | 4.49 x 2.03 x 2.01 | padlocked holding cage for infected patients in quarantine | floor, solid | long axis on X; the door and padlock are at the +X end |
| med_mattress_stained | 223 | 1.90 x 0.90 x 0.12 | bare stained mattress, for overflow triage bedding on the floor | floor, low (non-solid or step) | long axis on X |
| med_microscope | 224 | 0.18 x 0.22 x 0.29 | clinic lab microscope | desk or bench, non-solid | stage faces OBJ +Z (Blender -Y) |
| med_notice_board | 225 | 1.20 x 0.07 x 0.90 | clinic notice board (vaccination, medical centre and surgery notices) | wall, bottom edge at about 0.9 m; non-solid | face is OBJ +Z (Blender -Y) |
| med_nurse_desk | 226 | 1.18 x 0.56 x 0.81 | nurse-station or triage desk | floor, solid | the drawers and seat side face OBJ -Z (Blender +Y); the closed modesty panel faces OBJ +Z (-Y) |
| med_pill_bottle | 227 | 0.07 x 0.03 x 0.13 | pharmacy medicine bottle | shelf, table or floor clutter, non-solid | label faces OBJ +Z (Blender -Y) |
| med_records_cabinet | 228 | 0.90 x 0.49 x 1.00 | patient-records filing cabinet | floor against a wall, solid | drawers face OBJ +Z (Blender -Y) |
| med_scrub_sink | 229 | 0.49 x 0.54 x 1.13 | stained pedestal scrub sink | floor against a wall, solid | basin faces OBJ +Z (Blender -Y); tap and back at +Y |
| med_specimen_jar | 230 | 0.15 x 0.15 x 0.24 | murky jar of preserved tissue for the morgue or lab shelf | shelf or table clutter, non-solid | round |
| med_supply_cabinet | 231 | 1.10 x 0.44 x 1.70 | rust-flecked medical supply cabinet with glass upper doors | floor against a wall, solid | doors face OBJ +Z (Blender -Y); back to the wall at +Y |
| med_vitals_monitor | 232 | 0.38 x 0.18 x 0.28 | bedside cardiac monitor, still lit (ECG, 60 bpm, 120/80) | bedside shelf or cart top, non-solid | screen faces OBJ +Z (Blender -Y) |
| med_wall_clock | 233 | 0.24 x 0.02 x 0.23 | octagonal ward wall clock | wall at about 2.0 m, non-solid | face is OBJ +Z (Blender -Y) |
| med_ward_bed | 234 | 1.98 x 0.94 x 0.84 | iron-frame patient ward bed with a stained mattress | floor, solid | long axis on X; the taller headboard is at the -X end |
| med_ward_bed_tossed | 235 | 2.10 x 1.52 x 0.84 | ward bed with the mattress wrenched diagonally across the frame (a struggle or escape) | floor, solid (frame is 1.98 x 0.94, centred; the mattress hangs about 0.29 m over both long sides) | long axis on X |
| med_ward_toilet | 236 | 0.40 x 0.70 x 0.82 | filthy ward toilet | floor against a wall, solid | bowl faces OBJ +Z (Blender -Y); cistern at +Y |
| med_washroom_mirror | 237 | 1.50 x 0.04 x 1.00 | grime-blackened washroom mirror | wall above the sinks, bottom edge at about 1.1 m; non-solid | rotated so the glass faces OBJ +Z (Blender -Y); the pack faced +X |
| med_washroom_stall | 238 | 2.13 x 1.54 x 2.40 | staff washroom cubicle with its door shut | floor, solid | door faces OBJ +Z (Blender -Y); a bit tall (real about 2.0 m) but usable as is |
| med_wheelchair | 239 | 0.48 x 0.90 x 0.79 | rusted ward wheelchair | floor, solid | footrests face OBJ +Z (Blender -Y); a little small (real size about 0.65 x 1.05 x 0.9, so scale about 1.15) |
| med_xray_viewer_chest | 240 | 0.41 x 0.10 x 0.50 | x-ray lightbox holding a chest film | wall, bottom edge at about 1.2 m; non-solid | film faces OBJ +Z (Blender -Y) |
| med_xray_viewer_skull | 241 | 0.41 x 0.10 x 0.50 | x-ray lightbox holding a skull film | wall, bottom edge at about 1.2 m; non-solid | film faces OBJ +Z (Blender -Y) |

## Labs and research

| Model | Slot | Size (m) | What it is | Placement | Facing |
| --- | --- | --- | --- | --- | --- |
| lab_analyzer_cabinet | 184 | 1.83 x 2.22 x 3.11 | spectrometer/analyser cabinet with a sample drawer | floor; solid | front (slot and drawer) -y, rack/IO back +y |
| lab_anomaly_relic | 185 | 0.50 x 0.40 x 0.40 | anomalous pyramid recovered from The Dark Below, under study | plinth, table, specimen cabinet or containment cell; not solid | symmetric |
| lab_boardroom_chair | 186 | 0.99 x 0.93 x 1.16 | leather boardroom/executive chair | floor; solid | seat faces -y (imported with yaw 180) |
| lab_cell_gate | 187 | 3.50 x 0.19 x 2.72 | barred containment-cell front with a gated door, arched top | floor, spans an opening; solid | flat in the x-z plane, works from either side; arched top suits arched openings |
| lab_clean_bench | 188 | 1.18 x 0.56 x 0.81 | white enamel clean-lab bench with drawers | floor; solid | drawer/seated side -y (imported with yaw 180) |
| lab_control_board | 189 | 1.32 x 0.19 x 0.60 | three-bay instrument and relay control board for diagnostics bays and containment control | wall, base at about 1.0 m; not solid (flush) | front -y, has no back faces, put +y against the wall |
| lab_data_terminal | 190 | 0.89 x 0.81 x 0.48 | all-in-one 1980s workstation terminal | desk top; not solid | screen and keyboard face -y |
| lab_desk_lamp | 191 | 0.18 x 0.40 x 0.52 | articulated desk lamp | desk top; not solid | lamp head reaches -y (imported with yaw 180) |
| lab_door_reader | 192 | 0.85 x 0.52 x 2.39 | card reader above a keypad, beside sealed lab doors | wall; not solid | faces -y |
| lab_exec_desk | 193 | 2.21 x 1.10 x 0.89 | director's executive desk for the boardroom and executive offices | floor; solid | seated/drawer side -y, modesty panel +y |
| lab_field_meter | 194 | 0.25 x 0.33 x 0.15 | portable field/flux meter for energy diagnostics | bench-top clutter; not solid | dial on top, knob end -y, carry handle +y |
| lab_id_scanner | 195 | 0.30 x 0.06 x 0.41 | palm/ID scanner at containment doors | wall at about 1.1 m; not solid | faces -y |
| lab_instrument_terminal | 196 | 0.92 x 0.47 x 0.68 | readout terminal (CRT and keypad) for the energy diagnostics benches and consoles | bench or desk top, or a wall shelf; solid | screen faces -y |
| lab_knife_switch | 197 | 0.31 x 0.20 x 0.32 | heavy knife-switch isolator for test rigs and cell power | wall at about 1.2 m; not solid | handle faces -y |
| lab_mainframe | 198 | 2.20 x 2.28 x 3.18 | lab data-processing mainframe with tape drives | floor against a wall; solid | tape-drive front faces -y |
| lab_microscope | 199 | 0.18 x 0.22 x 0.29 | analytical-chemistry microscope | bench top (z about 0.81 on lab_clean_bench); not solid | eyepieces toward -y (operator stands at -y) |
| lab_notice_board | 200 | 1.20 x 0.07 x 0.90 | staff notice board (its posters are medical/vaccination notices) | wall, base at about 0.9 m; not solid | faces -y |
| lab_pa_horn | 201 | 0.49 x 0.56 x 0.43 | PA/alarm horn for the observation theater and corridors | high on a wall (about 2.5 m); not solid | horn mouth -y angled down, bracket +y against the wall |
| lab_photocopier | 202 | 0.83 x 0.75 x 0.97 | admin photocopier | floor; solid | front doors and trays -y (imported with yaw 180) |
| lab_potted_plant | 203 | 0.37 x 0.43 x 0.69 | potted plant for the executive suite and lobby | floor; not solid | symmetric |
| lab_pressure_gauge | 204 | 0.29 x 0.11 x 0.41 | line pressure gauge on a pipe stub, for cryo lines and test loops | wall or pipe at 1.2-1.6 m; not solid | dial faces -y |
| lab_sample_fridge | 205 | 0.70 x 0.85 x 1.00 | under-counter sample refrigerator (chemistry, cryo vault anteroom) | floor; solid | door -y, coils +y |
| lab_scope_monitor | 206 | 0.47 x 0.34 x 0.45 | oscilloscope/waveform monitor on a stand | desk top; not solid | screen faces -y |
| lab_specimen_cabinet | 207 | 1.34 x 0.74 x 2.19 | glass-front specimen and sample storage cabinet | floor against a wall; solid | glass doors -y (glass texture has alpha) |
| lab_specimen_crate | 208 | 1.12 x 1.12 x 1.12 | sealed specimen transport crate | floor, stackable; solid | symmetric |
| lab_status_display | 209 | 0.69 x 0.06 x 0.20 | LED status/alert readout above doors and in the theater | wall at about 2.2 m; not solid | faces -y |
| lab_test_chamber | 210 | 1.14 x 0.68 x 0.64 | octagonal sealed energy-cell test chamber | floor or heavy bench; solid | long axis along x with vented ends at Â±x; no clear front |
| lab_wall_clock | 211 | 0.40 x 0.03 x 0.40 | wall clock | wall at about 2.0 m; not solid | dial faces -y |

## Mines

| Model | Slot | Size (m) | What it is | Placement | Facing |
| --- | --- | --- | --- | --- | --- |
| mine_entrance | 114 | 3.86 x 8.28 x 4.82 |  |  |  |
| mine_headframe | 115 | 2.72 x 2.69 x 9.72 |  |  |  |
| mine_lift | 116 | 2.07 x 2.30 x 3.90 |  |  |  |
| mine_bent_grate | 242 | 3.14 x 0.11 x 3.38 | iron bar grate bent open from inside (breached shaft or adit gate) | floor, solid except the breach | panel in the X-Z plane, Â±Y |
| mine_blast_plunger | 243 | 0.51 x 0.43 x 1.09 | plunger blasting machine | floor, small solid | T-handle bar runs along X; any yaw; MIS-SCALED about 1.8x, real about 0.30 x 0.25 x 0.60 |
| mine_board_barricade | 244 | 3.31 x 0.15 x 3.30 | cross-boarded closure sealing old workings | floor, solid barrier across a drift | panel in the X-Z plane, two-sided (Â±Y) |
| mine_cage_gate | 245 | 2.10 x 0.07 x 2.55 | lattice gate for a cage-lift landing | floor, solid (door) | panel in the X-Z plane, Â±Y |
| mine_cage_lamp | 246 | 0.21 x 0.20 x 0.36 | caged bulkhead lamp | ceiling (hang under timber caps) or a wall bracket; not solid | domed top is +Z; symmetric |
| mine_depot_forklift | 247 | 1.20 x 3.73 x 2.27 | staging-depot forklift | floor, solid | forks point -Y; counterweight at +Y |
| mine_dewatering_pump | 248 | 3.06 x 0.94 x 1.75 | dewatering pump set for the pump station (hero, 4730 tris) | floor, solid | front (doors and handwheels) faces -Y; pipe elbows at the Â±X ends |
| mine_dynamite_bundle | 249 | 0.22 x 0.21 x 0.58 | bundled dynamite sticks with fuse | floor, shelf or crate top; not solid | upright; MIS-SCALED about 2.5x, real about 0.09 x 0.08 x 0.24 |
| mine_first_aid_box | 250 | 0.26 x 0.42 x 0.59 | wall-mounted first-aid box | wall at about 1.3 m; solid | red cross faces +X, back is -X; yaw so +X faces the room |
| mine_hard_hat | 251 | 0.28 x 0.35 x 0.15 | miner's cap-lamp helmet, dropped | floor or shelf; not solid | lamp faces +Y |
| mine_mesh_doorway | 252 | 3.19 x 0.06 x 2.78 | chain-link panel with a door opening, 1.08 w x 2.29 h, centred | floor; sides solid, opening passable | panel in the X-Z plane; opening spans x -0.54..0.54 |
| mine_mesh_fence | 253 | 3.19 x 0.06 x 2.78 | chain-link panel for explosives-magazine or lamp-room cages | floor, solid | panel in the X-Z plane, Â±Y; tiles every 3.19 m |
| mine_muck_pile | 254 | 1.59 x 1.16 x 0.30 | blasted-rock muck pile / spoil | floor, low, walkable | any yaw |
| mine_ore_cart | 255 | 1.77 x 1.34 x 1.53 | ore tub / mine car | floor, solid | travels along X with wheels on the Â±Y sides; yaw 90 degrees to sit on mine_rail_* (wheel track about 1.3 m matches the gauge) |
| mine_pick | 256 | 0.10 x 0.66 x 1.16 | miner's pick | floor, leaning on a wall or tool rack; not solid | upright, head along Y; slightly large (real about 0.9 m tall, optional scale 0.8) |
| mine_portable_generator | 257 | 0.87 x 1.04 x 0.82 | portable generator in a tubular frame (tool station) | floor, solid | outlet/control panel faces +X |
| mine_prop_post | 258 | 0.34 x 0.26 x 2.89 | single timber stull or roof prop | floor to roof, solid | round, any yaw |
| mine_rail_curve | 259 | 5.02 x 5.06 x 0.14 | 90-degree track curve | floor, walkable | one end exits -X (track centre y about -1.6); the other exits +Y (track centre x about +1.6) |
| mine_rail_straight | 260 | 1.80 x 8.28 x 0.13 | ore-haulage track: sleepers and rails, gauge about 1.2 m | floor, walkable (not solid) | runs along Y; tiles every 8.276 m |
| mine_rail_switch | 261 | 9.01 x 8.28 x 0.13 | Y-split turnout | floor, walkable | single track enters at +Y (x about +0.3); two branches leave the -Y edge at x about Â±3.6 |
| mine_shaft_ladder | 262 | 0.78 x 0.90 x 7.46 | steel ladderway section for shafts | wall, against the shaft wall, not solid | rungs span Y (0.9 m); wall side is -X; foot kicks out toward +X; climb from +X |
| mine_shield_ring | 263 | 5.33 x 3.15 x 3.79 | boring-shield segment-erector ring under assembly, with access stair and control cabinet (hero, 4848 tris) | floor, solid | ring axis along Y; stair on the -Y side; cabinet at +X; intended about 2x for tunnel-boring-machine scale (about 10.7 x 6.3 x 7.6) |
| mine_shoring_bay | 264 | 3.29 x 2.73 x 3.11 | double timber set with lagged side walls (shored bay or drift portal) | floor; timbers solid, centre passable | passage runs along Y, lagging on the Â±X sides |
| mine_shovel | 265 | 0.11 x 0.31 x 1.51 | square-mouth muck shovel | floor, leaning on a wall; not solid | upright; blade width along Y, flat faces Â±X |
| mine_timber_lining | 266 | 3.21 x 7.69 x 3.16 | fully timbered drift section (sets, caps, lagging), 1420 tris | floor; walls only, centre passable | runs along Y; sits inside mine_tunnel_straight at the same origin |
| mine_timber_set | 267 | 3.19 x 0.27 x 3.09 | timber shoring set (two legs and a cap) for blasted drifts | floor; legs solid, opening about 2.7 m wide is passable | frame lies in the X-Z plane and the drift runs along Y; repeat every 1.5-2 m |
| mine_tool_chest | 268 | 3.73 x 2.23 x 4.85 | red rolling tool chest for maintenance-drift tool stations | floor, solid | drawers face -Y; MIS-SCALED 5x, real 0.75 x 0.45 x 0.97 (scale 0.2) |
| mine_tunnel_straight | 269 | 3.86 x 8.28 x 3.37 | blasted-rock drift shell (survey tunnel or adit) | floor; hollow shell with inward-facing normals, open at both ends | runs along Y; tiles every 8.276 m |
| mine_tunnel_tee | 270 | 8.28 x 6.01 x 3.35 | blasted-rock T-junction shell | floor; hollow shell with inward-facing normals | through-run along X (open at Â±X); branch opens toward +Y; -Y side is solid rock |
| mine_water_main | 271 | 0.40 x 2.04 x 0.40 | 2 m flanged water-main pipe (rising main along drift walls) | run along a wall, floor or ceiling; solid | runs along Y; chains every 2.044 m |

## Caverns

| Model | Slot | Size (m) | What it is | Placement | Facing |
| --- | --- | --- | --- | --- | --- |
| cave_boulder_a | 154 | 3.60 x 2.67 x 2.10 | Fallen breakdown boulder: rubble fields, cavern wall mass, scree at the foot of terraces | floor, solid; scale 4-15 m for organic walls | no front; any yaw |
| cave_boulder_round | 155 | 3.15 x 2.17 x 2.98 | Water-worn domed boulder for lake shores and big cavern-wall bulges | floor, solid | no front; any yaw |
| cave_calcified_tree | 156 | 2.97 x 3.34 x 7.91 | Petrified, calcified tree for the calcified-column forest (same mesh as dead_tree_a, stone skin) | floor, solid trunk (narrow the width/depth for collision) | branches asymmetric; vary yaw |
| cave_ceiling_rock | 157 | 3.15 x 2.17 x 2.98 | Hanging ceiling bulge / roof boss for the stalactite chambers | ceiling: base = ceiling - height; not solid | no front; any yaw |
| cave_crystal_bed_blue | 158 | 1.22 x 1.22 x 0.21 | Cyan mineral/crystal deposit for the crystal galleries | floor patch, not solid; scale and tile | square; any yaw |
| cave_fish_skeleton | 159 | 0.52 x 0.17 x 0.02 | Picked-clean cave-fish skeleton on underground lake shores | floor, not solid; at 0.52 m long, scale up for a big lake fish | long axis X |
| cave_fungus_bulbs | 160 | 1.22 x 0.95 x 1.79 | Cluster of glowing blue bulb fungus (light-source dressing) | floor, not solid | radial |
| cave_fungus_funnel | 161 | 1.98 x 1.84 x 1.75 | Blue-lit funnel fungus | floor, solid | radial |
| cave_fungus_giant | 162 | 3.65 x 3.65 x 3.34 | Giant blue-capped cave fungus, the "trees" of the fungal groves | floor, solid stem (shrink width/depth for the stem, or leave not solid) | radial |
| cave_fungus_shelf | 163 | 3.24 x 2.32 x 0.68 | Orange bracket/shelf fungus | wall bracket: raise base onto a rock face; or floor mat; not solid | flat plate; any yaw |
| cave_fungus_spire | 164 | 1.96 x 1.97 x 3.40 | Violet glowing spire-cap fungus | floor, solid | radial |
| cave_nest_pod | 165 | 0.70 x 0.70 x 0.56 | Native-fauna nest pod / egg chimney | floor, solid once scaled | radial |
| cave_oldcity_arch | 166 | 3.00 x 0.60 x 1.80 | Freestanding stone arch spanning two Old City columns | set on two cave_oldcity_pillar about 2.2 m apart (base = pillar height); solid | spans X; faces +/-Z; both sides textured |
| cave_oldcity_dais | 167 | 5.20 x 6.00 x 1.20 | Carved hexagonal dais: ritual platform / Deep Gate threshold | floor, solid; walkable top (lower the height to about 0.6 if it has no steps) | hexagon; any 60-degree yaw |
| cave_oldcity_gateway | 168 | 3.00 x 1.11 x 3.00 | Arched Old City portal. Scaled x4-5 (12-15 m) it is the monumental Deep Gate frame | floor, solid frame; the opening is only about 1.5 x 2.2 at this size, so use x1.5 or more for a walk-through door | passage runs along Z; wall along X; trim on both faces |
| cave_oldcity_pillar | 169 | 0.85 x 0.85 x 4.53 | Fluted column of the pre-human Old City | floor, solid | square; any 90-degree yaw |
| cave_oldcity_pillar_broken | 170 | 0.85 x 0.85 x 1.46 | Snapped Old City column on its carved plinth | floor, solid | square |
| cave_oldcity_pillar_fallen | 171 | 2.13 x 0.60 x 0.52 | Toppled column drum lying in the ruins | floor, solid | long axis X |
| cave_oldcity_relic | 172 | 0.50 x 0.40 x 0.40 | Carved pyramid relic / Deep Gate keystone | floor or on the dais, not solid | carved faces; any yaw |
| cave_oldcity_rubble | 173 | 1.71 x 1.71 x 0.30 | Low pile of fallen masonry | floor, not solid (walk over) | any yaw |
| cave_oldcity_wall_ruin | 174 | 2.85 x 0.75 x 3.00 | Collapsed Old City wall with a broken edge | floor, solid | wall along X; broken edge on +X; both faces textured |
| cave_ossuary_wall | 175 | 3.00 x 0.75 x 3.00 | Bone-passage ossuary wall packed with skulls | floor/wall, solid; tile along passages | wall along X; skulls on both faces |
| cave_rock_block | 176 | 3.14 x 2.02 x 2.88 | Flat-topped bedrock block: terraces over the abyss, ledges, wall courses | floor, solid; walkable top | no front; any yaw |
| cave_rock_column | 177 | 2.10 x 2.67 x 3.60 | Upright rock buttress: floor-to-ceiling columns, chamber dividers | floor, solid; stretch height to the ceiling | no front; any yaw |
| cave_rock_slab | 178 | 3.47 x 2.36 x 1.82 | Low rock lens: terrace lips, stepping ledges, overhangs (raise base) | floor or raised ledge, solid | long axis X |
| cave_stalactite | 179 | 1.97 x 2.74 x 3.72 | Hanging stalactite mass; broad top, point down | ceiling: base = ceiling - height, sink the top 10-20% into the ceiling; not solid unless within reach | no front; any yaw |
| cave_stalactite_cluster | 180 | 3.50 x 4.50 x 2.00 | Ceiling of hanging needle stalactites | ceiling: base = ceiling - 2.0; not solid | no front |
| cave_stalagmite | 181 | 1.97 x 2.74 x 3.72 | Stalagmite spire / rock fang; tapers to the top | floor, solid; looks best thin, e.g. 1.2 x 1.2 x 5 | no front; any yaw |
| cave_stalagmite_cluster | 182 | 3.50 x 4.50 x 2.00 | Field of needle stalagmites (nesting-ground spikes) | floor, solid or a hazard | no front |
| cave_web_corner | 183 | 1.36 x 0.00 x 0.80 | Silk webbing of the nesting grounds | wall corner or between rocks, not solid; one alpha quad, give depth about 0.02 | vertical quad in the X-Y plane facing +/-Z; may need two-sided rendering |

## Scrap city

| Model | Slot | Size (m) | What it is | Placement | Facing |
| --- | --- | --- | --- | --- | --- |
| scrap_aerial | 302 | 0.91 x 1.40 x 1.95 | Rooftop TV aerial or antenna spike | rooftop, not solid | elements run roughly along X |
| scrap_barrier_wall | 303 | 2.96 x 0.40 x 2.05 | Facility jersey barrier topped with salvaged corrugated sheet; shanty perimeter | floor, solid | long axis X; both faces look much the same |
| scrap_bedroll | 304 | 0.92 x 1.96 x 0.18 | Sleeping-bag bedroll | floor, not solid | long axis Y; head end at +Y |
| scrap_brazier | 305 | 0.65 x 0.75 x 0.52 | Fire basin on legs for heat and light (furnace row, market) | floor, solid | round, no facing; 1780 triangles |
| scrap_bridge_plank | 306 | 0.20 x 2.00 x 0.03 | Single plank for rope-bridge decking and walkways | floor or bridge deck, walkable | long axis Y; lay several side by side along X |
| scrap_burn_barrel | 307 | 0.78 x 0.84 x 0.89 | Open rusted can used as a burn barrel | floor, solid | 8-sided, open top, no facing |
| scrap_cable_pole | 308 | 2.27 x 0.30 x 6.73 | Wooden pole carrying cable-district lines and anchoring rope bridges | floor, solid (pole) | crossarm runs along X |
| scrap_catwalk_grate | 309 | 1.00 x 1.00 x 0.05 | Steel grating tile for cable-district walkways and ledges | floor or deck tile, walkable | none (tiles) |
| scrap_chair | 310 | 0.56 x 0.55 x 1.21 | Stacking chair hauled down from the facility canteen | floor, solid | seat faces -Y |
| scrap_couch | 311 | 0.83 x 2.55 x 1.06 | Rotting plaid couch in a shanty home | floor, solid | seat faces -X; long axis Y |
| scrap_drum | 312 | 0.80 x 0.69 x 0.90 | Painted salvaged oil drum | floor, solid | 8-sided, no facing |
| scrap_foundry_shed | 313 | 8.80 x 14.32 x 5.50 | Large corrugated hall over the foundry or smelter bays | floor, walk-in, solid walls | both gable ends (Â±Y) are open; small door in the +X wall near -Y; long axis is Y |
| scrap_lantern | 314 | 0.26 x 0.31 x 0.77 | Kerosene hurricane lantern | tabletop or floor, or hung from a hook, not solid | none |
| scrap_leanto | 315 | 3.57 x 2.12 x 3.06 | Open-front corrugated stall (market stall or furnace-row shelter) | floor, solid side and back walls | open at -Y; the Â±X sides are closed; the +Y back wall has a doorway |
| scrap_mattress | 316 | 2.62 x 1.35 x 0.20 | Stained mattress for sleeping nooks | floor, not solid | long axis X |
| scrap_notice_board | 317 | 2.00 x 0.40 x 3.30 | Roofed signboard at the settlement entrance | floor, solid posts | sign faces -Y; the texture reads "TERVETULOA" (Finnish for "welcome"), blurred at 128 px |
| scrap_patched_wall | 318 | 3.05 x 0.20 x 2.50 | Cracked facility concrete panel patched with planks; settlement wall | floor, solid | long axis X; the plank patch is on the -Y face |
| scrap_plank_pile | 319 | 2.14 x 4.08 x 0.45 | Stack of salvaged planks (building stock) | floor, solid but low enough to step on | long axis Y |
| scrap_sat_dish | 320 | 1.03 x 1.10 x 1.73 | Satellite dish on a post, for shrine and shanty roofs | floor or rooftop, solid | dish points -Y and upward |
| scrap_server_cabinet | 321 | 0.58 x 0.70 x 1.12 | Salvaged computer and drive cabinet; Machine Shrine pillar (stack or repeat it) | floor, solid | front panel faces -Y |
| scrap_shack | 322 | 3.57 x 6.25 x 3.06 | Corrugated-iron hut, the basic dwelling of the outer shanties | floor, solid (walls are flat single planes) | doorway in the +X long wall near the -Y end and a second doorway in the +Y end wall; the -Y end and -X wall are solid |
| scrap_shade_lamp | 323 | 0.36 x 0.48 x 0.37 | Gooseneck shade lamp rigged over stalls and doorways | wall, not solid | wall plate is on +Y (put that side flush to the wall); the shade hangs toward -Y |
| scrap_shrine_candle | 324 | 0.30 x 0.30 x 0.10 | Candle on a dish, a Machine Shrine offering | floor or tabletop, not solid | none |
| scrap_shrine_terminal | 325 | 0.92 x 0.47 x 0.68 | Rugged facility control terminal, the Machine Shrine altar piece | on a crate or table, solid | screen and keypad face -Y |
| scrap_shrine_tv | 326 | 0.60 x 0.33 x 0.40 | Dead CRT set stacked in the Machine Shrine | on crates, shelves or a floor stack, solid | screen faces -Y |
| scrap_still | 327 | 2.22 x 2.22 x 3.52 | Copper still or smelter column on furnace row | floor, solid | spout and pipework sit on the -Y/+X side; 2680 triangles |
| scrap_stovepipe | 328 | 0.50 x 0.40 x 1.41 | Capped stove flue for shacks and the furnace row | floor beside a shack wall or on a roof, not solid | bottom elbow opens toward +X, so point +X into the wall |
| scrap_train_car | 329 | 3.83 x 23.39 x 5.33 | Wrecked facility transit or ore-line car embedded in rock (hero piece, 8 textures) | floor or partly buried in rock, solid; has a floor texture and door gaps | long axis Y; the two ends look alike; 3106 triangles |
| scrap_trash_heap | 330 | 1.85 x 3.20 x 1.18 | Refuse heap with bin bags | floor, not solid (low) | none |
| scrap_van_husk | 331 | 2.84 x 6.34 x 2.71 | Stripped, rusted van shell on the crushed-machinery plains | floor, solid | front of the van faces -Y; long axis Y; rests on its belly |

## Creatures

Two purchased creatures, and the transformed's clips, kept with the library
(gitignored) under `src/assets/library/creatures/`, are rebuilt from their packs
by tools that are in the repository. Their numbers live in `creatureTraits` (WorldDefinition.h).

| Kind | Pack | Runtime files (resources) | Rebuild | Clips |
| --- | --- | --- | --- | --- |
| Snatcher | `O:/retro official/psx-snatcher-low-poly-horror.zip` (rigged on the Mixamo skeleton, A-pose) | `snatcher.fbx` 2126, `snatcher.png` 2127 (512 px from the 2048 px sheet), `snatcher-animation.bin` 2128 | `blender -b --python tools/BakeSnatcher.py -- Snatcher.glb snatcher.fbx`, then `RawMetalAuthoredCreatureBake snatcher.fbx snatcher-animation.bin Snatcher_Idle Snatcher_Creep Snatcher_Snatch Snatcher_Stagger Snatcher_Death mixamorig:Hips` | its own idle; the Mixamo injured walk (its limping creep), neck bite (frames 6-36), hit stagger (9-47) and death on its back (8-64), retargeted by absolute bone orientation |
| Chimera | `O:/retro official/chimera.zip` (eleven rigid parts, no skeleton) | `chimera.fbx` 2129, `chimera.png` 2130, `chimera-animation.bin` 2131 | `blender -b --python tools/RigChimera.py -- chimera.glb chimera.fbx`, then `RawMetalAuthoredCreatureBake chimera.fbx chimera-animation.bin Chimera_Idle Chimera_Walk Chimera_Attack Chimera_Pain Chimera_Death` | keyed by RigChimera.py on its own ten-bone rig: a twitching idle (head jerks, a pawing forehoof), a walk whose leg swing comes from the CC0 Quaternius wolf (`tools/data/wolf_walk.json`, extracted by `tools/ExtractWolfGait.py`; licence in `tools/data/wolf_walk.LICENSE.txt`) under a hunched, clawing torso, rearing with the petals open, a flinch, a collapse onto its side. The texture's island gutters are padded so no black bleeds through at seams or distant mips (same for the Snatcher) |
| Transformed (kind 7) | the Freight Worker's body, `O:/retro official/Characters/Radiation Workers – Retro PSX Character Pack/heavy radiation suit.fbx` (tracked as `src/assets/actors/source/freight-worker.fbx`, Mixamo-named rig); Mixamo takes in `source-assets/` | the Worker's `freight-worker.obj` 277 and textures 279-281 (scorched at load: half saturation, two thirds light, an olive cast), `transformed-animation.bin` 2185 | `RawMetalCreatureBake --takes src/assets/actors/source/freight-worker.fbx transformed-animation.bin source-assets/worker_agony_head.fbx:1:112 source-assets/transformed_lunge_run.fbx:1:25 source-assets/transformed_neck_bite.fbx:16:104:drift source-assets/transformed_hit_stagger.fbx:1:50 source-assets/transformed_death_back.fbx:1:75 source-assets/transformed_overhead_strike.fbx:4:70:drift` | six RMA3 clips at every 30 fps frame: idle (clutching its head), lunge run (in place, one cycle per 3 m), leap and neck bite (the leap is its first .15, the hold the rest, with the take's forward lunge), hit stagger, death onto its back, overhead blow (the fists fall at .55). Retargeted by the engine's own bone-delta retarget with arms matched by direction, as the Worker's own clips are |

Chapter 1 placements: Snatchers in the Fabrication Shop's welding bay, on
Personnel Transit's platform (a Tram 4 passenger) and in Checkpoint Delta's
holding cells; the Chimera is R-114's occupant in Research Receiving's cold hold.
