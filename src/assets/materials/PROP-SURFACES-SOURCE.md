Surfaces that previously rendered as flat placeholder colours now use
purchased textures from `O:/retro official`.

PSX Textures v3.1 / PSX Textures, 256 px, colour and normal maps unmodified:

- `iron-plate.png` / `iron-plate-normal.png`: `metal_floor_1`. Iron structures
  (material 8): gantries, steps, posts, rails, dark-finish pipes.
- `dark-metal.png` / `dark-metal-normal.png`: `metal_floor_2`. Flashlight
  housing and reflector, medical kit stand.
- `rubber.png` / `rubber-normal.png`: `rubber_tsk_1`. Rubber parts, field
  terminal case, medical kit interior and feet.

Environments / All / Textures, JPEG decoded once to lossless PNG:

- `leather.png`: `Leather_01.jpg`. Straps and the viewmodel sleeve.
- `id-fabric.png`: `Fabric_07.jpg`. Flashlight identification band.

Environments / abandoned_hospital / medkit / `medkit_base_color.png`, cropped
without resampling:

- `../pickups/medkit-case.png`: white panel, pixels (37,4)-(141,92).
- `../pickups/medkit-label.png`: "H / FIRST AID KIT" panel, (37,130)-(141,218).

Any facility material that still has no texture renders as a pink and black
checker and is listed in `RawMetal-missing-textures.txt`.
