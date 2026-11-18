# Library parts imported for 1.5.3

Seventy-six models from the purchased packs (`O:/retro official/`), imported with
`tools/library/import_model.py` (Blender 5.2 headless, textures at 256 px) and registered with
`tools/library/register.py`. The model files live in `src/assets/library/` (gitignored).

- Building parts take the `bld_` prefix and have their own tab, **Building**, in the Campaign Builder's
  parts browser (tools/Level Editor/editor/NativeParts.h); the rest go to Mine, Caves and Lights by name.
- All of them stand in the Starter Map's **Parts Gallery** (maps 131 and 141, two levelled gravel yards
  built on the forest's maps there, so no map is renumbered). They are left out of the museum rows.
- Fixes after import: the rock pack's and the gem piles' textures were not linked in their files and came
  out as 4x4 swatches; they were replaced with the pack's own `T_RP_Rocks_*_2K.png`, `blue_gemstone_tiles.png`
  and `red_gemstones.png` at 256 px. The two industrial air conditioners were exported a few times too small
  and were scaled by 2.4. The boiler and the water tower were imported and dropped (9,000 and 7,000 triangles).

| slot | name | tab | size (w x d x h m) | triangles | source |
|---|---|---|---|---|---|
| 425 | bld_wall_a | Building | 4.00 x 0.40 x 3.00 | 6 | Models/GLB/Modular Structures/wall_rg_1.glb |
| 426 | bld_wall_b | Building | 4.00 x 0.40 x 3.00 | 6 | Models/GLB/Modular Structures/wall_hr_1.glb |
| 427 | bld_wall_c | Building | 4.00 x 0.40 x 3.00 | 6 | Models/GLB/Modular Structures/wall_hs_1.glb |
| 428 | bld_wall_d | Building | 4.00 x 0.40 x 3.00 | 6 | Models/GLB/Modular Structures/wall_rx_1.glb |
| 429 | bld_wall_house | Building | 4.09 x 0.18 x 3.00 | 48 | Models/GLB/Modular Structures/wall_1.glb |
| 430 | bld_wall_breach | Building | 4.00 x 4.30 x 3.00 | 1662 | Models/GLB/Modular Structures/wall_hr_1_hole_1.glb |
| 431 | bld_wall_tunnel_mouth | Building | 4.06 x 0.17 x 3.00 | 650 | Models/GLB/Modular Structures/wall_1_tunnel_hole.glb |
| 432 | bld_wall_window | Building | 4.06 x 0.52 x 3.00 | 298 | Models/GLB/Modular Structures/window_1_single.glb |
| 433 | bld_wall_doorway | Building | 4.00 x 0.42 x 3.00 | 42 | Models/GLB/Modular Structures/doorway_hr_1.glb |
| 434 | bld_pillar_a | Building | 0.60 x 0.60 x 3.00 | 10 | Models/GLB/Modular Structures/pillar_rg_2.glb |
| 435 | bld_pillar_broken | Building | 0.60 x 0.60 x 1.60 | 126 | Models/GLB/Modular Structures/pillar_hr_1_broken.glb |
| 436 | bld_pillar_tall | Building | 0.60 x 0.60 x 6.00 | 10 | Models/GLB/Modular Structures/pillar_hs_2.glb |
| 437 | bld_pillar_wide | Building | 1.00 x 1.00 x 3.00 | 22 | Models/GLB/Modular Structures/pillar_rtx_1.glb |
| 438 | bld_beam_concrete | Building | 4.00 x 0.26 x 0.46 | 16 | Models/GLB/Modular Structures/beam_1.glb |
| 439 | bld_beam_steel_post | Building | 0.60 x 0.50 x 3.00 | 256 | Models/GLB/Modular Structures/beam_hc_vertical_1.glb |
| 440 | bld_beam_pipes | Building | 0.74 x 4.00 x 0.52 | 1536 | Models/GLB/Modular Structures/beam_rtx_2_pipes.glb |
| 441 | bld_beam_lintel | Building | 4.00 x 0.30 x 0.26 | 26 | Models/GLB/Modular Structures/beam_hl_1.glb |
| 442 | bld_stairs_short | Building | 4.00 x 2.08 x 1.70 | 68 | Models/GLB/Modular Structures/stairs_3.glb |
| 443 | bld_stairs_concrete | Building | 2.00 x 4.08 x 3.20 | 132 | Models/GLB/Modular Structures/stairs_5_concrete.glb |
| 444 | bld_stairs_wood | Building | 2.00 x 4.04 x 3.20 | 172 | Models/GLB/Modular Structures/stairs_5_wood.glb |
| 445 | bld_stairs_landing | Building | 4.00 x 6.00 x 3.20 | 130 | Models/GLB/Modular Structures/stairs_mp_1.glb |
| 446 | bld_catwalk | Building | 4.00 x 2.08 x 1.31 | 240 | Models/GLB/Modular Structures/catwalk_1.glb |
| 447 | bld_railing | Building | 0.08 x 4.00 x 1.20 | 60 | Models/GLB/Modular Structures/platform_b_handrail_1.glb |
| 448 | bld_scaffold_deck | Building | 2.03 x 4.00 x 1.00 | 254 | Models/GLB/Modular Structures/platform_ax_1.glb |
| 449 | bld_ramp | Building | 1.40 x 4.00 x 1.00 | 10 | Models/GLB/Modular Structures/ramp_platform_slim_mx_1.glb |
| 450 | bld_pipe_run | Building | 0.68 x 4.00 x 0.68 | 796 | Models/GLB/Modular Structures/pipe_bx_1.glb |
| 451 | bld_pipe_riser | Building | 0.94 x 4.00 x 2.99 | 1378 | Models/GLB/Modular Structures/pipe_cx_1.glb |
| 452 | bld_pipe_riser_tall | Building | 0.94 x 4.00 x 5.59 | 1482 | Models/GLB/Modular Structures/pipe_dx_1.glb |
| 453 | bld_pipe_bracket | Building | 0.60 x 0.06 x 0.52 | 76 | Models/GLB/Modular Structures/pipes_holder_1.glb |
| 454 | bld_culvert | Building | 2.00 x 4.10 x 2.00 | 512 | Models/GLB/Modular Structures/concrete_pipe_hm_1_middle.glb |
| 455 | bld_culvert_grate | Building | 2.02 x 0.18 x 2.02 | 820 | Models/GLB/Modular Structures/concrete_pipe_hm_1_end_bars_1.glb |
| 456 | bld_service_tunnel | Building | 2.00 x 4.00 x 2.90 | 12 | Models/GLB/Modular Structures/tunnel_mp_1_straight.glb |
| 457 | bld_window_bars | Building | 1.12 x 0.14 x 1.60 | 996 | Models/GLB/Modular Structures/window_metal_bars_single.glb |
| 458 | bld_door_wood | Building | 1.00 x 0.18 x 2.10 | 712 | Models/GLB/Modular Structures/door_1_loose.glb |
| 459 | bld_door_metal | Building | 1.00 x 0.18 x 2.10 | 264 | Models/GLB/Modular Structures/door_mp_1_loose.glb |
| 460 | bld_door_heavy | Building | 1.00 x 0.05 x 2.10 | 12 | Models/GLB/Doors & Gates/door_hr_12.glb |
| 461 | bld_gate | Building | 5.00 x 0.07 x 1.60 | 116 | Models/GLB/Doors & Gates/gate_1.glb |
| 462 | bld_tank_rig | Building | 2.61 x 2.87 x 2.39 | 4966 | Models/GLB/Large Props & Machinery/tank_system_mx_1.glb |
| 463 | bld_pipe_stack | Building | 0.40 x 0.10 x 3.00 | 126 | Models/GLB/Large Props & Machinery/pipes_hr_1.glb |
| 464 | bld_pipe_stack_elbow | Building | 0.44 x 0.08 x 0.44 | 576 | Models/GLB/Large Props & Machinery/pipes_hr_1_elbow_1.glb |
| 465 | bld_cage | Building | 4.49 x 2.03 x 2.01 | 1962 | Models/GLB/Large Props & Machinery/cage_mx_1.glb |
| 466 | bld_switch_box | Building | 1.20 x 0.66 x 2.91 | 38 | Models/GLB/Large Props & Machinery/electrical_equipment_1.glb |
| 467 | bld_cable_run | Building | 4.10 x 0.08 x 0.45 | 1788 | Models/GLB/Large Props & Machinery/wires_hr_1.glb |
| 468 | bld_ladder | Building | 0.84 x 0.21 x 8.10 | 1208 | Models/GLB/Modular Props/ladder_hr_1_long.glb |
| 469 | bld_metal_fence | Building | 3.60 x 0.10 x 2.50 | 74 | Models/GLB/Modular Props/metal_fence_hr_1.glb |
| 470 | bld_barricade | Building | 2.94 x 0.40 x 0.85 | 140 | Models/GLB/Modular Props/barricade_a_1.glb |
| 471 | bld_brick_rubble | Building | 2.31 x 2.16 x 0.07 | 189 | Models/GLB/Debris & Misc/debris_bricks_mx_1.glb |
| 472 | bld_gravel_pile | Building | 1.18 x 1.90 x 0.26 | 56 | Models/GLB/Debris & Misc/gravel_pile_hr_1.glb |
| 473 | bld_brick_stack | Building | 0.50 x 0.35 x 0.18 | 68 | Models/GLB/Debris & Misc/bricks_stacked_mx_1.glb |
| 474 | bld_air_unit | Building | 1.40 x 0.85 x 0.50 | 12 | Models/GLB/Air Conditioner/Industrial_Air_Conditioner_1.glb |
| 475 | bld_vent_unit | Building | 1.69 x 1.93 x 0.52 | 68 | Models/GLB/Air Conditioner/Industrial_Air_Conditioner_Vertical_1.glb |
| 476 | bld_lamp_cage | Building | 0.19 x 0.25 x 0.23 | 1548 | Models/GLB/Light Sources/lamp_mx_1_a_on.glb |
| 477 | bld_lamp_hood | Building | 0.36 x 0.48 x 0.37 | 782 | Models/GLB/Light Sources/lamp_mx_2_on.glb |
| 478 | bld_ceiling_lamp | Building | 0.21 x 0.46 x 0.18 | 472 | Models/GLB/Lighting/ceiling_lamp_mp_1_on.glb |
| 479 | mine_lantern | Lights | 0.21 x 0.21 x 0.43 | 371 | Environments/Mine/Minas/Models/Mines.fbx |
| 480 | mine_barrel | Mine | 0.81 x 0.81 x 1.14 | 60 | Environments/Mine/Minas/Models/Mines.fbx |
| 481 | mine_barrel_wide | Mine | 0.90 x 0.90 x 1.17 | 60 | Environments/Mine/Minas/Models/Mines.fbx |
| 482 | mine_crate | Mine | 1.13 x 1.13 x 1.16 | 12 | Environments/Mine/Minas/Models/Mines.fbx |
| 483 | mine_crate_long | Mine | 1.27 x 1.79 x 0.70 | 12 | Environments/Mine/Minas/Models/Mines.fbx |
| 484 | mine_petrol_can | Mine | 0.14 x 0.41 x 0.58 | 99 | Environments/Mine/Minas/Models/Mines.fbx |
| 485 | mine_locker | Mine | 0.64 x 0.65 x 2.36 | 86 | Environments/Mine/Minas/Models/Mines.fbx |
| 486 | mine_shelving | Mine | 1.98 x 0.54 x 2.00 | 88 | Environments/Mine/Minas/Models/Mines.fbx |
| 487 | mine_tray | Mine | 0.59 x 0.60 x 0.52 | 89 | Environments/Mine/Minas/Models/Mines.fbx |
| 488 | mine_gravel_heap | Mine | 1.56 x 1.19 x 0.30 | 19 | Environments/Mine/Minas/Models/Mines.fbx |
| 489 | cave_rock_boulder_b | Caves | 1.31 x 1.91 x 1.04 | 30 | Environments/Mine/Minas/Models/Mines.fbx |
| 490 | cave_rock_lump | Caves | 1.12 x 1.61 x 0.98 | 38 | Environments/Mine/Minas/Models/Mines.fbx |
| 491 | cave_rock_pillar | Caves | 1.49 x 1.74 x 3.43 | 91 | Environments/Mine/Minas/Models/Mines.fbx |
| 492 | cave_rock_tall | Caves | 1.56 x 1.72 x 1.97 | 40 | Environments/Mine/Minas/Models/Mines.fbx |
| 493 | cave_rock_wall | Caves | 4.25 x 1.15 x 3.66 | 140 | Environments/Mine/Minas/Models/Mines.fbx |
| 494 | cave_rock_chunk | Caves | 1.09 x 1.01 x 0.70 | 33 | Environments/Mine/Minas/Models/Mines.fbx |
| 495 | cave_rock_2m_a | Caves | 3.47 x 2.36 x 1.82 | 778 | PSX_Large_Terrain_Rock_Pack.zip (RP/FBX_Exports) |
| 496 | cave_rock_2m_b | Caves | 3.19 x 2.70 x 2.31 | 780 | PSX_Large_Terrain_Rock_Pack.zip (RP/FBX_Exports) |
| 497 | cave_rock_4m_a | Caves | 6.37 x 5.77 x 4.00 | 782 | PSX_Large_Terrain_Rock_Pack.zip (RP/FBX_Exports) |
| 498 | cave_rock_4m_b | Caves | 7.27 x 5.35 x 4.02 | 782 | PSX_Large_Terrain_Rock_Pack.zip (RP/FBX_Exports) |
| 499 | cave_gem_pile_blue | Caves | 1.22 x 1.22 x 0.21 | 128 | Props/Gems (.blend, exported to GLB) |
| 500 | cave_gem_pile_red | Caves | 1.22 x 1.22 x 0.21 | 128 | Props/Gems (.blend, exported to GLB) |
