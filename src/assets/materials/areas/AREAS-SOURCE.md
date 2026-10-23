Area finishes for campaign maps 0-2, from the purchased PSX Textures packs
(256 px). Normal maps are unmodified; `pressure-floor-normal` was downsampled
from 512 px to 256 px.

| Map | Walls | Floor |
| --- | --- | --- |
| 0 Foundry | `foundry-wall`: PSX Textures II `brick_wall_pt_1` | `foundry-floor`: PSX Textures `metal_floor_tx_1` |
| 1 Pressure Works | `pressure-wall`: PSX Textures II `metal_panels_pt_1` | `pressure-floor`: PSX Textures II `concrete_pt_2` |
| 2 Turbine Gantry | `turbine-wall`: PSX Textures `metal_wall_2` | `turbine-floor`: PSX Textures `concrete_tx_2` |

The colour maps are gamma-lifted copies: black and white points are kept and
the mean luminance is raised to about 80/255. The source art averages 39-71,
which under linear lighting left these halls far darker than the rest of the
facility. Gammas used:

- foundry wall 0.75, foundry floor 0.76
- pressure wall 0.62, pressure floor 0.85
- turbine wall 0.67, turbine floor 0.70

Floors are laid as 2 m tiles. Grating tiles and Pressure Works' raised plate
keep their functional textures.
