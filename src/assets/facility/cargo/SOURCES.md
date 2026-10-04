# Freight scenery sources

Imported unchanged from the user's `O:\retro official` collection:

* `pallet.fbx`, `pallet.png`: PSX Bunkers v1.8.8, wood_pallet_1 / wood_1.
* `service-van.obj`, `service-van.mtl`, `service-van.png`, `van-metal.png`: Pizza Doggy's Mystery Package vol. 8, Rusty PSX Style Kidnapper's Van, OBJ van_3 and textures van_3 / metal_2_1. Repurposed as a parked receiving maintenance vehicle; it is not rolling stock.
* `office-carpet.png`: PSX Textures II v1.6, 256/Color Maps/carpet_pt_1_1.png.
* `wooden-crate.obj`, `wooden-crate.png`: PSX Bunkers v1.8.8, OBJ wooden_crate_8 and its original texture. Used as loaded pallet and warehouse rack contents; original geometry and texture resolution retained.
* `chair-wooden.fbx`: PSX Bunkers v1.8.8, `PSX Bunkers/Models/FBX/chair_wooden_1.fbx`, unchanged. Resource 284, shared facility fixture 18. Its `wood_1.png` is an exact 256x256 RGBA match for the existing pallet texture (resource 266), so that texture is reused.
* `table-wooden.fbx`: PSX Bunkers v1.8.8, `PSX Bunkers/Models/FBX/table_large_3.fbx`, unchanged. Resource 285, shared facility fixture 19. Uses the same original `wood_1` texture already embedded as resource 266.

The asset packer verifies original model bytes and exact RGBA pixels in the executable. Existing compressor resources 142/143 also serve facility machinery model 12; duplicate resources 257/260 were removed without altering geometry or pixels.

Depot locomotives and the inspection chassis are authored structure geometry in CampaignMaps.cpp, separate from the imported van.

The runtime van now uses the supplied FBX/van_3.fbx assembly, copied unchanged
as service-van.fbx. The OBJ export places all four wheels at the origin; its
files are retained as provenance but are no longer embedded. FBX node transforms
position the four wheels at their actual axles.

To avoid embedding a second copy of the FBX's textures, resource 265 contains
service-van-assembled.obj, exported from the FBX using RawMetalClutterImport:
RawMetalClutterImport service-van.fbx --assembly service-van-assembled.obj --skin-rest
This bakes each node's world transform into its original triangle positions and
UVs, retains material assignments and every triangle, and keeps all four wheels
in place. The untouched source FBX is preserved. Runtime textures remain the
original van and metal PNG resources. This is a static assembly export, not mesh
simplification or texture downsampling.

The wooden chair retains its 490 source triangles and Y-up bounds
`(-0.270, 0, -0.270)` to `(0.270, 1.230851, 0.410510)` metres. The
backrest is on +Z, so its seated front faces -Y at authored fixture yaw 0,
+Y at yaw pi, and -X at yaw pi/2. Scale its width, depth and height together
to preserve the source proportions; a 0.95 m chair is 0.417 x 0.525 x 0.95 m.
Source FBX SHA-256: `61bb0d2909291243bc0069eff16252f84518d9c418f51331770f9a9bf8295f84`.

The wooden table retains its 220 source triangles and Y-up bounds
`(-0.850, 0, -0.450)` to `(0.850, 0.970, 0.450)` metres. Its long edge
lies on world X at fixture yaw 0, on world Y at yaw pi/2, and its two long
seating sides are symmetric. A uniform 0.80 m table is 1.402 x 0.742 x 0.80 m;
the tabletop reaches exactly the authored fixture base plus height.
Source FBX SHA-256: `09dac1af3dcc2d33fc10edfeeb201fbe9b9e1a4e82844e3a550d88114ac6a02d`.

## Reference-led industrial fixtures

* `extraction-fan.obj`, `extraction-fan.png`: PSX Bunkers v1.8.8,
  `PSX Bunkers/Models/FBX/vent_4.fbx` and `PSX Bunkers/Textures/vent_4.png`.
  Fixture 20, model resource 286, texture 287. The source fan's 542 triangles
  are exported with RawMetalClutterImport `--assembly`, then rotated from its
  wall-facing source coordinates `(X,Y,Z)` to `(X,-Z,Y)`. This rigid rotation
  places the caged face downward for a ceiling installation, retaining all
  triangles, UVs and material assignments. The `metal_4` rim material reuses
  the identical existing resource 182 atlas; both files have SHA-256
  `d3100ad3e8e4953a51169e1884594c1cef68b21031d4f6d41a2ce07ba39cb8db`.
  Runtime bounds are 1.100 x 1.100 x 0.18375 m (width, depth, height).
  Place the fixture's base at the bottom of the fan housing; its face points
  down at every yaw. A uniform 0.92 m diameter installation is 0.1537 m deep.
* `packing-crate.fbx`, `packing-crate.png`: PSX Mega Pack II v1.8,
  `PSX Mega Pack II/Models/other-formats/FBX/Props/wooden_crate_hx_6.fbx`
  and `PSX Mega Pack II/Textures/wooden_crate_hx_2_1.png`, unchanged.
  Fixture 21, resources 288/289. The complete source mesh has 132 triangles
  and bounds 1.401 x 1.400 x 1.000 m. Runtime applies a pale painted packing
  finish to the original grain/wear atlas, retaining dark markings and seams.
  The embedded 700 x 800 RGBA source texture is neither resized nor altered;
  the paint variant is generated after loading, before normal/mip preparation.
  Uniform 1.2 m height is 1.681 x 1.680 m in plan.
  Source FBX SHA-256:
  `1dda742b4626746eef19be2b2804387dde60e8aa5f8816eb945586483a286afa`.
* `control-console.fbx`, `control-console.png`,
  `control-console-emission.png`: PSX Tech v1.2.3,
  `PSX Tech/Models/other-formats/FBX/control_panel_etx_1.fbx` and original
  `Textures/electronics_etx_part_1` color/emission atlases, unchanged.
  Fixture 22, model 290, color 291, emission 292. Its 1,142 source triangles
  include the slanted CRT, physical levers, knobs and buttons. Source bounds
  are 0.711 x 0.44847 x 1.188 m; controls face world +Y at yaw 0, -Y at pi.
  A uniform 1.2 m height is 0.718 x 0.453 m in plan. The screen material uses
  engine-authored green CRT status graphics inside its original UV island;
  its bezel and body retain the supplied atlas. The FBX's extra lever keypad
  material uses `control-console-keypad.png`, the unchanged 128 x 128
  `PSX Bunkers/Textures/keypad_1.png` (resource 293). Its unnamed untextured
  lever housing uses the console body atlas. Source FBX SHA-256:
  `138deedec7e0fbf6a6aa82400d4c0bb4250ad2320c1971f3520790309e83395c`.

* `forklift.obj`, `VEHICLE_Forklift.mtl` and `fl_*.png`: the user's newly
  provided `O:\retro official\forklift.zip`, nested
  `source/VEHICLE_Forklift.zip`. The original `VEHICLE_Forklift.obj` is
  copied byte-identically to `forklift.obj`, resource 294 / fixture 23.
  All 373 source triangles, original UVs and eight material assignments
  are retained. Original Y-up metre bounds are
  `(-0.600000,-0.001389,-0.877000)` to `(0.599800,2.266073,2.854318)`.
  Runtime applies only a rigid 180-degree Y rotation to normalize the
  source +Z fork direction to world -Y at fixture yaw 0. Width, depth and
  height remain proportional: 2.20 m height is 1.1641 x 3.6204 m in plan.
  The fixture base lies at the source tyre bottoms. Source OBJ SHA-256:
  `ffc04ec24811090c81cdf4cff6465f8cf5c6e4a6be16dcdcc5d6bd55daa03c77`.
  Resources 295..302 contain, in order, the unchanged `fl_tire.png`,
  `fl_back.png`, `fl_front.png`, `fl_grey.png`, `fl_lift.png`, `fl_top.png`,
  `fl_side.png` and `fl_gate.png`. Their RGBA pixels and source dimensions
  are preserved exactly; no texture atlas resize, repaint or simplification
  is applied. Normal/mip maps are generated by the common material system,
  with low tyre gloss for matte rubber. The original accompanying MTL is
  retained for provenance and editing. The archive supplies no separate
  license/readme metadata, so no additional source attribution is invented.

The provided forklift replaces the earlier engine-authored prototype.
Its original model and all eight textures are used together, rather than
substituting generic structural boxes or wheel atlases for its components.
