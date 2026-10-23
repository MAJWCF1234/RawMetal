Industrial set pieces for campaign maps 0-2, from the purchased PSX Mega Pack
library at `O:/retro official/Models/OBJ`. The OBJ files are byte-identical,
and models bind to textures through their `usemtl` names.

| Facility model | File | Source | Use |
| --- | --- | --- | --- |
| 39 | `converter.obj` | `Large Props & Machinery/distillery_mx_1` | Foundry converter vessels |
| 40 | `flue.obj` | `Large Props & Machinery/chimney_a_1` | Foundry flues |
| 41 | `stock-cage.obj` | `Large Props & Machinery/cage_mx_1` | Foundry stock cage |
| 42 | `pendant-lamp.obj` | `Light Sources/lamp_mx_2_on` | Pendant work lamp |
| 43 | `ibeam.obj` | `Modular Structures/beam_hc_vertical_1` | I-beam columns |
| 44 | `pipe-riser.obj` | `Large Props & Machinery/pipes_hr_1` | Pressure Works riser bundles |
| 45 | `valve-wheel.obj` | `Large Props/pipe_valve_holder_large_1` | Valve wheels |
| 46 | `turbine-casing.obj` | `Large Props & Machinery/portal_mx_1` | Turbine Gantry turbines |

Textures come from `O:/retro official/Textures`, except `valve_1.png`, which
comes from `Models/DAE`. Files wider than 256 px were downsampled to 256 px to
match the other PSX atlases and to save executable size:

- `metal_hr_1`
- `metal_hr_2`
- `metal_hr_1_dark_smooth`
- `metal_floor_mx_1`
- `grate_hm_1_1`
- `steel_beams_hr_1`
- `pipe_mp_1`
- `valve_1`

`metal_hr_6_1` and `lamp_1_on` were already registered and are reused.

`boiler_hx_4` was considered as a cupola furnace and rejected: it is 9,150
triangles.
