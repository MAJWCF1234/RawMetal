Office furniture, computers and surfaces from the purchased library at
`O:/retro official`. Used first by the Security Office prologue.

Environments / OFFICE. OBJ models are byte-identical; JPEG textures are decoded
once to lossless PNG. Models bind to textures through their OBJ `usemtl` names.

| Facility model | File | Source | Material / texture |
| --- | --- | --- | --- |
| 24 | `desk.obj` | `desks/desk2.obj` | `desk_2` / `desk_2.jpg` |
| 25 | `desk-corner.obj` | `desks/desk2_corner.obj` | `desk_2` |
| 26 | `chair.obj` | `chairs/office_chair_black.obj` | `office_chair_black` |
| 27 | `crt-monitor.obj` | `computers/computer_monitor.obj` | `computer` / `computer.jpg` |
| 28 | `keyboard.obj` | `computers/computer_keyboard.obj` | `computer` |
| 29 | `pc-tower.obj` | `computers/computer_tower.obj` | `computer` |
| 30 | `cctv-tv.obj` | `computers/TV.obj` | `tv` / `tv.jpg` |
| 31 | `file-cabinet-wide.obj` | `file_cabinets/file_cabinet_large.obj` | `file_cabinet_large` / `file_cabinet_big.png` |
| 32 | `file-cabinet-tall.obj` | `file_cabinets/file_cabinet_small.obj` | `file_cabinet` / `file_cabinet_Small.jpg` |
| 33 | `desk-phone.obj` | `extras/desk_phone.obj` | `desk_phone` / `deskphone.jpg` |
| 34 | `mug.obj` | `extras/mug.obj` | `mug` / `mugs.jpg` |
| 35 | `water-cooler.obj` | `extras/water_cooler.obj` | `watercooler` |
| 36 | `coffee-machine.obj` | `extras/coffee_machine.obj` | `coffee_machine` |
| 37 | `bookcase.obj` | `book_things/book_case_small.obj` | `bookcase` |
| 38 | `binder.obj` | `book_things/file_binder.obj` | `Binders` / `file_binder.jpg` |

Measured sizes in metres (width x height x depth) are listed in the prologue
generator and in the FIXTURE records. Pass those to keep proportions.

PSX Textures v3.1 / 256, colour and normal maps unmodified, used as structure
materials:

| STRUCT material | File | Source |
| --- | --- | --- |
| 21 office wall | `office-wall.png` | `hl_office_complex_style_concrete_wall_1` |
| 22 drop ceiling | `drop-ceiling.png` | `hl_office_complex_style_drop_ceiling_1_1` |
| 23 office carpet | `office-carpet.png` | `hl_office_complex_style_carpet_floor_1` |
| 24 office tile | `office-tile.png` | `hl_office_complex_style_tile_floor_1` |

A TERMINAL placed inside a solid fixture's footprint, such as a desk, draws no
console of its own. The furniture's computer models are its visible hardware.

Derived screen art:

- `computer-emission.png`: an authored green-phosphor terminal glow, lit only
  on the monitor glass (atlas pixels 15..113 x 17..95).
- `tv-emission.png`: an authored CCTV corridor feed with scanlines, a timestamp
  strip and a REC dot, masked to the TV glass.
- `tv.png`: the purchased `tv.jpg` with only the glass darkened, so the lit
  feed reads as a powered CRT. All other pixels are unchanged.
