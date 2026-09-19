# Imported environment assets

Source library: `O:/retro official`. Originals were not changed.

- generator.fbx: Models/FBX/generator_1.fbx; generator.png: Textures/generator_1.png
- barrel.fbx: Models/FBX/Props/metal_barrel_hr_1.fbx; barrel.png: Textures/metal_barrel_hr_1.png
- crate.fbx: Models/FBX/Props/wooden_crate_1.fbx; crate.png: Textures/wooden_crate_hr_1.png
- concrete.png: Textures/concrete_floor_hr_1.png
- brick.png: Textures/bricks_hr_1.png
- straight-hazard-stripes.png: Props/Misc/Materials/caution_tape_3.png (straight parallel hazard stripes)
- chemical-warning.png: Props/Signs/Materials/sign_chemical_rusty.png
- machinery-warning.png: Props/Signs/Materials/sign_warning_01.png
- confined-warning.png: Props/Signs/Materials/sign_confined_space.png
- sign-rust.png: modern-retro-industrial-textures/Modern_Retro_Industrial_Textures/Textures/T_FF_Rusted_Metal_0.png
- panel-metal.png: Textures/metal_hr_1_dark_smooth.png
- terminal-panel.png: Props/Panels/Materials/control_panel.png
- warning-triangle.png: Props/Signs/Materials/sign_hazard_01.png

The foundry vessels, girders, overhead pipes, signs and threshold markings are engine geometry. Imported props retain their model UVs. Crates, barrels, generators and vessels occupy blocked navigation cells; all remaining walkable cells form a connected loop network.

Chemical-warning plates are mounted upright on the two southern coolant vessels, with square proportions preserved. The low service tunnel has a correctly oriented text plate. Door panels slide vertically, use the straight hazard tape, and block movement and enemy sight until raised. Raised foundry and storage floors share their actual heights with collision, props, enemies and route paint.

Sector plaques combine the supplied rust/metal surfaces with legible weathered stencil text and fasteners. Hazard stripes and safety labels use the authored pack textures. Route paint takes its wear pattern from the supplied rust texture. Decals clamp their UV edges and use distance-dependent mip levels to reduce distant shimmer; repeating paint permits wrapped UVs. Plates have shallow metal backs and sit apart from walls to avoid coplanar depth conflicts. The settings menu uses the same supplied rust and dark metal, with etched slider ticks and worn handles.

Threshold and machinery markings use one straight tape strip per contiguous opening or machine bank. The texture's aspect ratio determines the band's depth, so the diagonal stripes remain straight and continuous across tile boundaries. The earlier corner decal is no longer used.
