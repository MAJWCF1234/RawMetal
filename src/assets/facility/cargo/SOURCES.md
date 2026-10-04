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
