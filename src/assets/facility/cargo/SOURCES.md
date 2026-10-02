# Freight scenery sources

Imported unchanged from the user's `O:\retro official` collection:

* `pallet.fbx`, `pallet.png`: PSX Bunkers v1.8.8, wood_pallet_1 / wood_1.
* `service-van.obj`, `service-van.mtl`, `service-van.png`, `van-metal.png`: Pizza Doggy's Mystery Package vol. 8, Rusty PSX Style Kidnapper's Van, OBJ van_3 and textures van_3 / metal_2_1. Repurposed as a parked receiving maintenance vehicle; it is not rolling stock.
* `office-carpet.png`: PSX Textures II v1.6, 256/Color Maps/carpet_pt_1_1.png.

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
