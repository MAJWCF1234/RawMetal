# Architectural materials

Copied from the purchased library; originals in `O:/retro official` are unchanged.
All these PNGs are the supplied 256-pixel versions, without colour darkening.

| Runtime material | Source pack | Source colour / matching normal |
| --- | --- | --- |
| wall-grey | PSX Textures v3.1 | concrete_wall_10.png / concrete_wall_10_normal.png |
| wall-painted | PSX Textures II v1.6 | concrete_wall_pt_2.png / concrete_wall_pt_2_normal.png |
| bulkhead | PSX Textures v3.1 | metal_floor_1.png / metal_floor_1_normal.png |
| grating | PSX Textures v3.1 | metal_floor_4.png / metal_floor_4_normal.png |
| concrete-floor | PSX Textures II v1.6 | concrete_pt_2.png (colour only) |

Files come from each pack's `256/Color Maps` and `256/Normal Maps` directories.
The first map uses grey/green painted concrete; the other maps use pale concrete.
Bulkheads use rectangular sliding leaves and square jambs, with riveted steel.

`renderer/NormalMapping.cpp` decodes tangent-space RGB vectors, converts green-up
to the renderer's downward texture V axis, and builds renormalized mip levels.
`Scene3D.cpp` derives the tangent basis from geometry and UVs and evaluates the
two strongest nearby ceiling lights per surface patch. Occlusion and distance
attenuate each light. Per-pixel normal response modulates the existing lighting;
it does not displace geometry or alter collision. Distant detail uses mip maps.

The normal-map diagnostic checks matching dimensions, normalized vectors,
opposed lighting, and neutral normals preserving the underlying shading.
