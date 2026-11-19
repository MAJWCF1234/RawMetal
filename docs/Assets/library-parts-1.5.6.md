# Library parts imported for 1.5.6

126 models from the purchased packs (`O:/retro official/`, `PSX Nature v1.7.3b`), imported with
`tools/library/import_batch.py tools/library/batches/parts-1.5.6.json` (Blender 5.2 headless, textures at 256 px) and
registered with `tools/library/register.py`. The model files live in `src/assets/library/` (gitignored).

- Each name's prefix says its tab in the Campaign Builder's parts browser (tools/Level Editor/editor/NativeParts.h):
  `fur_` Furniture, `home_` Kitchen, `store_` Storage, `mach_` Machines, `lab_`/`med_` Lab, `lamp_` Lights, `bld_` Building,
  `dress_` Props, `out_` Outdoor, `nat_` Nature, `hor_` Horror.
- They stand in the Starter Map's Parts Gallery 3 and 4 (maps 128 and 129, on the forest's cells there), and the
  Builder's level-design kits (tools/campaign/kits.py, the Kits tab and the Kits Gallery, maps 126 and 127) are built
  from them and the earlier parts. They stay out of the museum rows.
- `register.py` now embeds a texture once: a material whose texture file is byte-identical to one already embedded
  points at that resource (pack atlases shared by a sofa and an armchair).
- The importer gained `--scale` (the DINER and GGBot car packs are oversized: 0.7) and finds `TX_Name.png` for an
  `MT_Name` material (the MS_ props).
- Dropped after the 4x4-swatch check and a look: a glass display case (untextured glass), a CPU chip, a supply crate
  (3,600 triangles), a fuse box, a radio, a blast-door frame (7,700 triangles), a van (6,000), a power brick, a
  duplicate ceiling lamp, a toy road block and tyre, a spotlight, a branded car battery, a toilet stall whose
  texture is a baked lightmap, and a single firewood log.
- Textures are marked for neural storage in tools/neural_texture_plan.json (library globs by prefix); 32 passed
  the quality gate and ship as NTX2 packets, the rest stay lossless.

Sizes are the model's own (width x depth x height, metres).

## Furniture and office (22)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 501 | fur_office_chair | Furniture | 0.94 x 0.86 x 1.10 | 324 | Environments/OFFICE/chairs/office_chair_black.glb |
| 502 | fur_desk | Furniture | 2.21 x 1.09 x 0.89 | 206 | Environments/OFFICE/desks/desk2.glb |
| 503 | fur_desk_corner | Furniture | 2.21 x 2.21 x 0.89 | 82 | Environments/OFFICE/desks/desk2_corner.glb |
| 504 | fur_desk_worn | Furniture | 2.21 x 1.09 x 0.89 | 236 | Environments/OFFICE/desks/desk1_weathered.glb |
| 505 | fur_cubicle_wall | Furniture | 3.06 x 0.52 x 2.50 | 98 | Environments/OFFICE/dividers/divider_veneer.glb |
| 506 | fur_cubicle_desk | Furniture | 2.99 x 3.00 x 1.50 | 88 | Environments/OFFICE/dividers/divider_full_h_desk.glb |
| 507 | fur_file_cabinet | Furniture | 0.48 x 0.53 x 1.41 | 20 | Environments/OFFICE/file_cabinets/file_cabinet_small.glb |
| 508 | fur_file_drawers | Furniture | 1.44 x 0.70 x 0.71 | 20 | Environments/OFFICE/file_cabinets/file_cabinet_large.glb |
| 509 | fur_monitor | Furniture | 0.60 x 0.53 x 0.57 | 108 | Environments/OFFICE/computers/computer_monitor.glb |
| 510 | fur_keyboard | Furniture | 0.68 x 0.23 x 0.04 | 20 | Environments/OFFICE/computers/computer_keyboard.glb |
| 511 | fur_desk_phone | Furniture | 0.33 x 0.33 x 0.09 | 108 | Environments/OFFICE/extras/desk_phone.glb |
| 512 | fur_water_cooler | Furniture | 0.38 x 0.45 x 1.40 | 256 | Environments/OFFICE/extras/water_cooler.glb |
| 513 | fur_books | Furniture | 0.75 x 0.23 x 0.36 | 296 | Environments/OFFICE/book_things/books.glb |
| 514 | fur_armchair | Furniture | 1.50 x 0.99 x 1.07 | 592 | Models/GLB/Furniture/armchair_1.glb |
| 515 | fur_sofa | Furniture | 2.12 x 0.94 x 0.98 | 508 | Models/GLB/Furniture/sofa_3.glb |
| 516 | fur_bed | Furniture | 2.70 x 1.40 x 1.27 | 744 | Models/GLB/Furniture/bed_1.glb |
| 517 | fur_bedside_table | Furniture | 0.56 x 0.52 x 0.64 | 478 | Models/GLB/Furniture/bedside_table_1.glb |
| 518 | fur_dining_table | Furniture | 1.70 x 1.10 x 0.86 | 92 | Models/GLB/Furniture/dinner_table_1.glb |
| 519 | fur_stool_square | Furniture | 0.47 x 0.47 x 0.59 | 212 | Models/GLB/Furniture/chair_mp_1.glb |
| 520 | fur_wardrobe | Furniture | 1.34 x 0.64 x 2.10 | 1426 | Models/GLB/Furniture/wardrobe_mp_1.glb |
| 521 | fur_tv | Furniture | 0.51 x 0.42 x 0.79 | 670 | Models/GLB/Electronics & Misc/tv_mp_1.glb |
| 522 | fur_bench_low | Furniture | 0.67 x 0.32 x 0.33 | 144 | Models/GLB/Props/stool_mx_1.glb |

## Kitchen and bathroom (15)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 523 | home_stove | Kitchen | 0.68 x 0.61 x 1.20 | 32 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^Stove$) |
| 524 | home_fridge | Kitchen | 0.73 x 0.83 x 2.12 | 374 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^Refrigerator) |
| 525 | home_kitchen_counter | Kitchen | 1.60 x 2.49 x 1.18 | 186 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^kitchen$) |
| 526 | home_cupboard | Kitchen | 1.50 x 2.47 x 0.71 | 36 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^cupboard$) |
| 527 | home_microwave | Kitchen | 0.36 x 0.55 x 0.31 | 12 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^Microwave$) |
| 528 | home_trash_can | Kitchen | 0.82 x 0.84 x 1.02 | 150 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^trash_can_metal) |
| 532 | home_fryer | Kitchen | 0.85 x 0.71 x 1.09 | 492 | Environments/DINER/Models/Objects.fbx (object ^Fryer$), scaled 0.7 |
| 533 | home_range | Kitchen | 1.85 x 0.84 x 0.99 | 292 | Environments/DINER/Models/Objects.fbx (object ^Stove$), scaled 0.7 |
| 534 | home_cooker_hood | Kitchen | 2.02 x 0.86 x 0.70 | 34 | Environments/DINER/Models/Objects.fbx (object ^cooker_hood$), scaled 0.7 |
| 535 | home_prep_table | Kitchen | 1.65 x 0.85 x 0.92 | 80 | Environments/DINER/Models/Objects.fbx (object ^Table_01$), scaled 0.7 |
| 536 | home_hand_dryer | Kitchen | 0.46 x 0.34 x 0.37 | 214 | Environments/DINER/Models/Objects.fbx (object ^Hand_dryer$), scaled 0.7 |
| 537 | home_soap_dispenser | Kitchen | 0.20 x 0.15 x 0.33 | 136 | Environments/DINER/Models/Objects.fbx (object ^Soap_dispenser$), scaled 0.7 |
| 541 | home_toilet | Kitchen | 0.96 x 0.65 x 0.96 | 504 | Environments/PSX Bathroom Set/FBXs/toilet.fbx |
| 542 | home_sink | Kitchen | 0.45 x 0.60 x 0.47 | 432 | Environments/PSX Bathroom Set/FBXs/sink.fbx |
| 543 | home_toilet_roll | Kitchen | 0.12 x 0.17 x 0.11 | 262 | Environments/PSX Bathroom Set/FBXs/paper_roll.fbx |

## Storage and shelving (14)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 538 | store_box_stack | Storage | 0.68 x 0.91 x 1.11 | 160 | Environments/DINER/Models/Objects.fbx (object ^Boxes_Refreshment _04), scaled 0.7 |
| 545 | store_shelf_metal | Storage | 1.60 x 0.55 x 1.80 | 1120 | Models/GLB/Props/shelf_mx_1.glb |
| 546 | store_shelf_wood | Storage | 0.74 x 0.44 x 1.40 | 168 | Models/GLB/Furniture/shelf_mp_2.glb |
| 547 | store_shelf_tall | Storage | 1.22 x 0.32 x 1.69 | 46 | Models/GLB/Furniture/shelf_mp_5.glb |
| 548 | store_hanging_shelf | Storage | 1.00 x 0.31 x 0.25 | 564 | Models/GLB/Props/hanging_shelf_mx_1.glb |
| 549 | store_supply_crate_open | Storage | 0.67 x 0.43 x 0.20 | 260 | Models/GLB/Large Props/supply_crate_1_empty.glb |
| 550 | store_wooden_crate | Storage | 0.72 x 0.50 x 0.36 | 184 | Models/GLB/Large Props/wooden_crate_1.glb |
| 551 | store_barrel_metal | Storage | 0.75 x 0.75 x 1.08 | 384 | Models/GLB/Large Props/metal_barrel_mp_1.glb |
| 552 | store_barrel_rusty | Storage | 0.67 x 0.67 x 1.00 | 380 | Models/GLB/Props/metal_barrel_hr_2.glb |
| 553 | store_cement_pallet | Storage | 1.23 x 1.23 x 1.10 | 3604 | Models/GLB/Props/cement_bags_mp_1_pallet_1.glb |
| 554 | store_pallet | Storage | 1.44 x 1.31 x 0.20 | 312 | Models/GLB/wood_pallet_1.glb |
| 555 | store_jerrycan | Storage | 0.45 x 0.24 x 0.62 | 414 | Models/GLB/Props/jerrycan_mx_1.glb |
| 556 | store_gas_cylinder | Storage | 0.33 x 0.33 x 1.58 | 480 | Models/GLB/Props/gas_cylinder_mx_1.glb |
| 557 | store_toolbox | Storage | 0.60 x 0.34 x 0.45 | 172 | Models/GLB/Props/toolbox_mx_1.glb |

## Industrial and machinery (6)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 558 | mach_storage_tank | Machines | 4.53 x 2.04 x 2.74 | 2550 | Models/GLB/Large Props & Machinery/storage_tank_mx_1.glb |
| 559 | mach_generator | Machines | 3.40 x 1.00 x 1.80 | 172 | Models/GLB/generator_1.glb |
| 560 | mach_machinery | Machines | 0.58 x 0.70 x 1.12 | 572 | Models/GLB/machinery_3.glb |
| 561 | mach_control_panel | Machines | 0.71 x 0.45 x 1.19 | 1142 | Models/GLB/control_panel_etx_1.glb |
| 562 | mach_wall_box | Machines | 0.49 x 0.24 x 0.67 | 384 | Models/GLB/wall_box_1.glb |
| 563 | mach_pipe_valve | Machines | 0.28 x 0.18 x 0.28 | 294 | Models/GLB/Large Props/pipe_valve_holder_large_1.glb |

## Medical and lab (5)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 564 | lab_test_machine | Lab | 2.66 x 3.40 x 3.52 | 1762 | Models/GLB/Large Props & Machinery/test_machine_mx_1.glb |
| 565 | lab_monitoring_screen | Lab | 0.53 x 0.50 x 0.37 | 494 | Models/GLB/Electronics & Misc/monitoring_screen_mp_1.glb |
| 566 | med_syringe | Lab | 0.06 x 0.20 x 0.02 | 254 | Models/GLB/Items & Weapons/syringe_mp_1.glb |
| 567 | med_pills | Lab | 0.07 x 0.07 x 0.11 | 80 | Models/GLB/Items & Weapons/pills_bottle_1.glb |
| 568 | med_first_aid_kit | Lab | 0.40 x 0.22 x 0.08 | 12 | Models/GLB/Props/first_aid_kit_hr_1.glb |

## Lighting fixtures (9)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 531 | lamp_street | Lights | 0.49 x 1.14 x 6.45 | 228 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^lamppost$) |
| 540 | lamp_strip | Lights | 0.21 x 1.39 x 0.11 | 144 | Environments/DINER/Models/Objects.fbx (object ^Lights$), scaled 0.7 |
| 544 | lamp_bathroom_tube | Lights | 0.11 x 1.20 x 0.13 | 288 | Environments/PSX Bathroom Set/FBXs/flu_light.fbx |
| 569 | lamp_ceiling_tube | Lights | 1.20 x 0.30 x 0.10 | 36 | Models/GLB/Lighting/ceiling_lamp_4_on.glb |
| 570 | lamp_ceiling_bar | Lights | 0.75 x 0.13 x 0.34 | 778 | Models/GLB/Lighting/lamp_1_on.glb |
| 571 | lamp_bulkhead | Lights | 0.36 x 0.12 x 0.18 | 522 | Models/GLB/Lighting/lamp_2_on.glb |
| 572 | lamp_wall | Lights | 0.51 x 0.71 x 0.42 | 854 | Models/GLB/Light Sources/lamp_mx_3_on.glb |
| 573 | lamp_gooseneck | Lights | 0.51 x 0.84 x 0.56 | 1064 | Models/GLB/Light Sources/lamp_mx_4_on.glb |
| 574 | lamp_pendant | Lights | 0.18 x 0.18 x 0.60 | 728 | Models/GLB/Electronics & Misc/ceiling_fan_mp_1_1_light_on.glb |

## Doors, windows and building (14)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 575 | bld_doorway_frame | Building | 4.09 x 0.20 x 3.00 | 456 | Models/GLB/Modular Structures/doorway_1.glb |
| 576 | bld_doorway_wide | Building | 4.00 x 0.42 x 3.00 | 54 | Models/GLB/Modular Structures/doorway_hr_1_wide.glb |
| 577 | bld_doorway_double | Building | 4.09 x 0.20 x 3.00 | 208 | Models/GLB/Modular Structures/double_single_doorway_1.glb |
| 578 | bld_door_panel | Building | 1.00 x 0.18 x 2.10 | 712 | Models/GLB/Modular Structures/door_2_loose.glb |
| 579 | bld_door_steel | Building | 1.00 x 0.19 x 2.10 | 184 | Models/GLB/Modular Structures/door_mp_6_loose.glb |
| 580 | bld_lift_doors | Building | 4.09 x 0.29 x 3.00 | 682 | Models/GLB/Modular Structures/elevator_doorway_1.glb |
| 581 | bld_lift_car | Building | 2.60 x 2.91 x 3.00 | 926 | Models/GLB/Modular Structures/elevator_1.glb |
| 582 | bld_window_double | Building | 4.09 x 0.52 x 3.00 | 502 | Models/GLB/Modular Structures/window_2_double.glb |
| 583 | bld_window_broken | Building | 4.09 x 0.20 x 3.00 | 268 | Models/GLB/Modular Structures/window_2_single_no_glass.glb |
| 584 | bld_garage_frame | Building | 8.00 x 0.40 x 6.00 | 44 | Models/GLB/Modular Structures/garage_door_frame_hr_1.glb |
| 585 | bld_tunnel_tee | Building | 6.00 x 4.00 x 2.90 | 32 | Models/GLB/Modular Structures/tunnel_mp_1_tee.glb |
| 586 | bld_tunnel_cross | Building | 6.00 x 6.00 x 2.90 | 52 | Models/GLB/Modular Structures/tunnel_mp_1_cross_tee.glb |
| 587 | bld_tunnel_elbow | Building | 4.00 x 4.00 x 2.90 | 24 | Models/GLB/Modular Structures/tunnel_mp_1_elbow.glb |
| 588 | bld_lift_button | Building | 0.12 x 0.03 x 0.44 | 232 | Models/GLB/Electronics & Misc/elevator_call_button_1.glb |

## Dressing (no signs) (9)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 529 | dress_plant | Props | 1.01 x 0.81 x 1.00 | 434 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^Plants$) |
| 530 | dress_radio | Props | 0.36 x 0.62 x 0.61 | 90 | Environments/Trailer_Park/Models/Trailer_Park_Props.fbx (object ^Radio_01$) |
| 539 | dress_garbage_bag | Props | 0.67 x 0.80 x 0.79 | 83 | Environments/DINER/Models/Objects.fbx (object ^Garbage_bag_01$), scaled 0.7 |
| 589 | dress_bucket | Props | 0.43 x 0.44 x 0.47 | 732 | Models/GLB/Props/bucket_mx_1.glb |
| 590 | dress_paint_can | Props | 0.18 x 0.18 x 0.25 | 188 | Models/GLB/Props/paint_can_mx_1.glb |
| 591 | dress_scrap_metal | Props | 0.32 x 0.36 x 0.11 | 1020 | Models/GLB/Props/scrap_metal_mx_1.glb |
| 592 | dress_carpet | Props | 3.23 x 2.04 x 0.01 | 18 | Models/GLB/Large Props/carpet_mp_1.glb |
| 593 | dress_cactus | Props | 0.18 x 0.18 x 0.27 | 398 | Models/GLB/Small Props/potted_cactus_mp_1.glb |
| 594 | dress_planter | Props | 0.23 x 0.23 x 0.21 | 252 | Models/GLB/Small Props/planter_pot_mp_1_full.glb |

## Outdoor and urban (23)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 595 | out_car_sedan | Outdoor | 1.79 x 4.30 x 1.52 | 438 | Vehicles/PSX_Style_Cars_by_GGBot_(August2023)/Car 01/Car.obj, scaled 0.7 |
| 596 | out_car_hatch | Outdoor | 2.03 x 4.57 x 1.54 | 312 | Vehicles/PSX_Style_Cars_by_GGBot_(August2023)/Car 02/Car2.obj, scaled 0.7 |
| 597 | out_minivan | Outdoor | 2.38 x 4.41 x 1.92 | 476 | Vehicles/PSX_Style_Cars_by_GGBot_(August2023)/Car 04/Car4.obj, scaled 0.7 |
| 598 | out_container | Outdoor | 2.50 x 10.80 x 2.50 | 12 | Models/GLB/Large Props & Machinery/shipping_container_mx_1.glb |
| 599 | out_container_open | Outdoor | 3.46 x 11.94 x 2.50 | 84 | Models/GLB/Large Props & Machinery/shipping_container_mx_1_hollow_1.glb |
| 600 | out_concrete_fence | Outdoor | 3.60 x 0.20 x 2.50 | 74 | Models/GLB/Modular Props/concrete_fence_hr_1.glb |
| 601 | out_metal_fence_tall | Outdoor | 3.60 x 0.10 x 5.00 | 146 | Models/GLB/Modular Props/metal_fence_hr_1_tall.glb |
| 602 | out_fence_post | Outdoor | 0.40 x 0.20 x 2.50 | 10 | Models/GLB/Modular Props/metal_fence_hr_1_pillar_1.glb |
| 603 | out_wood_fence | Outdoor | 2.21 x 0.20 x 1.20 | 144 | Props/all_fences/Fences/low_wooden_fence/wooden_fence_closed.glb |
| 604 | out_wood_fence_broken | Outdoor | 2.11 x 0.29 x 1.27 | 108 | Props/all_fences/Fences/low_wooden_fence/wooden_fence_broken.glb |
| 605 | out_brick_wall | Outdoor | 4.00 x 0.30 x 2.08 | 44 | Props/all_fences/Fences/brick_fence/brick_wall.glb |
| 606 | out_fence_gate | Outdoor | 1.19 x 0.04 x 1.67 | 56 | Props/all_fences/Fences/metal_fence/metal_fence_gate_with_gate.glb |
| 607 | out_bench | Outdoor | 0.40 x 2.00 x 0.76 | 98 | Environments/URBAN/Bus stops/busstop_bench.glb |
| 608 | out_drain_cover | Outdoor | 0.60 x 0.60 x 0.01 | 222 | Models/GLB/drain_cover_2.glb |
| 609 | out_debris_bricks | Outdoor | 2.33 x 2.00 x 0.07 | 189 | Models/GLB/Debris & Misc/debris_bricks_mx_2.glb |
| 610 | out_concrete_block | Outdoor | 0.60 x 2.20 x 1.10 | 130 | Models/GLB/Props/concrete_block_mx_1.glb |
| 611 | out_water_barrel | Outdoor | 0.73 x 0.73 x 1.12 | 1180 | Models/GLB/water_barrel_1.glb |
| 612 | nat_boulder | Nature | 2.52 x 2.67 x 2.12 | 152 | PSX Nature v1.7.3b/PSX Nature v1.7.3b/PSX Nature/Models/GLB/stone_3.glb |
| 613 | nat_stone_round | Nature | 0.27 x 0.29 x 0.18 | 188 | PSX Nature v1.7.3b/PSX Nature v1.7.3b/PSX Nature/Models/GLB/stone_5.glb |
| 614 | nat_mushroom | Nature | 0.14 x 0.14 x 0.15 | 206 | PSX Nature v1.7.3b/PSX Nature v1.7.3b/PSX Nature/Models/GLB/mushroom_n_3.glb |
| 624 | out_tent | Outdoor | 2.27 x 2.27 x 1.23 | 822 | Environments/Tent_Civilian/MS_Tent_Civilian.fbx |
| 625 | out_campfire | Outdoor | 1.38 x 1.30 x 0.19 | 1131 | Environments/Campfire/MS_Campfire.fbx |
| 626 | out_sawbuck | Outdoor | 1.40 x 1.40 x 1.00 | 120 | Environments/Sawbuck/MS_Sawbuck.fbx |

## Horror dressing (9)

| slot | name | tab | size (m) | triangles | source |
|---|---|---|---|---|---|
| 615 | hor_cobweb | Horror | 1.17 x 0.00 x 0.80 | 4 | Models/GLB/Decals/cobweb_2.glb |
| 616 | hor_cobweb_wide | Horror | 0.88 x 0.00 x 0.80 | 4 | Models/GLB/Decals/cobweb_4.glb |
| 617 | hor_mask | Horror | 0.23 x 0.32 x 0.06 | 232 | Models/GLB/Masks/mask_mx_1.glb |
| 618 | hor_cross | Horror | 0.26 x 0.39 x 0.04 | 26 | Models/GLB/Small Props/cross_1.glb |
| 619 | hor_butcher_knife | Horror | 0.17 x 0.51 x 0.03 | 112 | Models/GLB/Items & Weapons/butcher_knife_mp_1.glb |
| 620 | hor_meat | Horror | 0.34 x 0.26 x 0.08 | 272 | Models/GLB/Items & Weapons/meat_1.glb |
| 621 | hor_mattress | Horror | 2.62 x 1.35 x 0.20 | 328 | Models/GLB/Furniture/old_mattress_mp_1.glb |
| 622 | hor_bed_frame | Horror | 2.70 x 1.40 x 1.27 | 416 | Models/GLB/Furniture/bed_1_frame.glb |
| 623 | hor_saw_blade | Horror | 0.59 x 0.59 x 0.02 | 968 | Models/GLB/Props/saw_blade_1.glb |
