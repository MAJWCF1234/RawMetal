# Freight scenery sources

Imported unchanged from the user's `O:\retro official` collection:

* `pallet.fbx`, `pallet.png`: PSX Bunkers v1.8.8, wood_pallet_1 / wood_1.
* `service-van.obj`, `service-van.mtl`, `service-van.png`, `van-metal.png`: Pizza Doggy's Mystery Package vol. 8, Rusty PSX Style Kidnapper's Van, OBJ van_3 and textures van_3 / metal_2_1. Repurposed as a parked receiving maintenance vehicle; it is not rolling stock.
* `office-carpet.png`: PSX Textures II v1.6, 256/Color Maps/carpet_pt_1_1.png.

The asset packer verifies original model bytes and exact RGBA pixels in the executable. Existing compressor resources 142/143 also serve facility machinery model 12; duplicate resources 257/260 were removed without altering geometry or pixels.

Depot locomotives and the inspection chassis are authored structure geometry in CampaignMaps.cpp, separate from the imported van.
