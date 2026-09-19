# Purchased facility models

Source archives in `O:/retro official` remain unchanged.

- Modular Retro FPS Kit v1.5: column_6, vent_fps_1, ceiling_lamp_fps_1 and their
  named materials provide wall columns, vents and ceiling fixtures.
- PSX Bunkers v1.8.8: generator_1, metal_shelf_1 and wall_box_2 supply generators,
  shelving, wall cabinets and control-terminal cabinets. Personnel logs use the
  separate computer_1 FBX with its original pc_1 and keyboard_1 textures from
  the same pack. The computer keeps uniform 0.65 scale and sits on a pedestal;
  electrical cabinets remain switchgear. Mesh UVs and per-face
  material assignments are retained, including generator_1 and metal_4 on the
  generator. Generator FBX is stored in `../environment/generator.fbx`.
- Generator colour texture: the purchased 845x764 atlas is copied to a 384-pixel
  maximum dimension runtime version with Lanczos filtering to meet the build
  size limit. Its aspect ratio and all UV content are retained; metal_4 is copied
  unchanged. The original atlas remains in `../environment/generator.png`.

Generators preserve their source 3.4 x 1.0 x 1.8 proportions. Each contiguous
pair of G tiles, plus an odd remaining tile, creates a complete machine fixture.
Rendering, collision and sound emitters use the same fixture placements.

Previously imported wall_6, wall_8, doorway_wide_1 and machinery_2 source files
remain for reference. The main walls now use the PSX Texture pack materials,
doors are square procedural bulkheads, and machinery_2 is no longer substituted
for generators. Imported wall/door modules still have diagnostic previews.

Ceiling lamps use the supplied Modular Retro FPS Kit lamp_1_emission.png as an additive emission channel, independent of diffuse illumination. Original 0.8 x 0.8 x 0.09 proportions are retained; the housing top stays 0.02 m below the supporting ceiling/deck. The light emitter sits 0.04 m below the luminous face.
