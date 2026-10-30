"""Build the surface-set and structure-finish textures from the purchased packs.

  python tools/library/surfaces.py [--sheet out.png]

Writes src/assets/library/textures/surface_*.png (gitignored, shared through
the library junction), registered in src/assets.rc as resources 2100-2124 and 2150-2154, and
loaded by SoftwareRenderer (World.h Surface sets, structure materials 26-35).
Every texture starts from a tileable pack texture under
O:/retro official/Textures; the grades below only shift colour and level, so
tiling is preserved. Normal maps are derived at load (deriveSurfaceNormal).
"""
import colorsys, os, sys
import numpy as np
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OUT = os.path.join(ROOT, 'src', 'assets', 'library', 'textures')
PACKS = 'O:/retro official/Textures'
PSX = PACKS + '/256/Color Maps'          # PSX Textures v3.1 + PSX Textures II v1.6
EMX = PACKS + '/Color Maps'              # experimental / emx panels
LRT = PACKS + '/LRT - Texture Pack - RT28.8 - 128x/tile'
SIZE = 256


def load(path):
    image = Image.open(path).convert('RGB')
    if image.size != (SIZE, SIZE):
        image = image.resize((SIZE, SIZE), Image.BICUBIC)
    return np.asarray(image, dtype=np.float32) / 255.0


def luma(c):
    return c[..., 0] * .299 + c[..., 1] * .587 + c[..., 2] * .114


def grade(c, saturation=1.0, tint=(1, 1, 1), mean=None, contrast=1.0):
    """Desaturate toward luma, multiply by a tint, then set the mean luma."""
    y = luma(c)[..., None]
    c = y + (c - y) * saturation
    c = c * np.array(tint, dtype=np.float32)
    if contrast != 1.0:
        m = luma(c).mean()
        c = m + (c - m) * contrast
    if mean is not None:
        c = c * (mean / 255.0 / max(1e-4, luma(c).mean()))
    return np.clip(c, 0, 1)


def hue(c, degrees):
    """Rotate hue (YIQ), keeping luma."""
    a = np.radians(degrees)
    to = np.array([[.299, .587, .114], [.596, -.274, -.322], [.211, -.523, .312]], np.float32)
    back = np.linalg.inv(to)
    r = np.array([[1, 0, 0], [0, np.cos(a), -np.sin(a)], [0, np.sin(a), np.cos(a)]], np.float32)
    m = back @ r @ to
    return np.clip(c @ m.T, 0, 1)


def growth():
    # Resonance growth (material 34): fleshy folds veined with the faceted
    # amethyst crust, graded to a sick dark purple-red.
    crust = load(LRT + '/RT_amethyst_block.png')
    flesh = grade(load(PSX + '/intestine_1.png'), saturation=.6, tint=(.8, .4, .66))
    facets = luma(crust)[..., None]
    facets = (facets - facets.mean()) / (facets.std() + 1e-4)
    c = flesh * (1 + .4 * np.clip(facets, -2, 2)) + hue(grade(crust, saturation=.7), 15) * np.clip(facets, 0, 3) / 3 * .28
    return grade(c, saturation=.55, mean=50)


def crystal():
    # Crystal (material 35, self-lit): the amethyst facets recoloured cyan,
    # lifted so the facet edges read as light inside the mineral.
    hsv = np.asarray(Image.open(LRT + '/RT_amethyst_block.png').convert('RGB').resize((SIZE, SIZE), Image.BICUBIC).convert('HSV'), dtype=np.float32)
    hsv[..., 0] = 128 + (hsv[..., 0] - hsv[..., 0].mean()) * .3   # cyan, keeping a little hue play
    hsv[..., 1] *= .8
    c = np.asarray(Image.frombytes('HSV', (SIZE, SIZE), hsv.astype(np.uint8).tobytes()).convert('RGB'), dtype=np.float32) / 255.0
    return grade(c, mean=150, contrast=1.1)


def flagstone():
    # Ancient flagstone floor: the irregular cobble flags, worn down to the
    # grey-ochre of the carved wall stone with the PSX grit of that stone.
    flags = grade(load(LRT + '/RT_cobblestone.png'), saturation=.55, tint=(1.0, .97, .88), contrast=1.2)
    grit = luma(load(PSX + '/stone_wall_pt_1.png'))[..., None]
    c = flags * (.72 + .5 * grit / grit.mean() * .55)
    return grade(c, mean=104)


def luminous():
    # Lab luminous ceiling: the drop-ceiling grid with white diffuser panels
    # (the renderer derives the glow from the bright panel texels).
    c = load(PSX + '/hl_office_complex_style_drop_ceiling_1.png')
    y = luma(c) * 255
    panel = ((y.mean(0)[None, :] >= 70) & (y.mean(1)[:, None] >= 70)).astype(np.float32)[..., None]
    lit = grade(c, saturation=0, tint=(.94, .98, 1.0), mean=226, contrast=.3)
    frame = grade(c, saturation=0, tint=(.9, .92, .95), mean=96)
    return np.clip(lit * panel + frame * (1 - panel), 0, 1)



def dado(name):
    # Rows 134-255 of the pack's 256 px wall: the painted band and skirting.
    image = Image.open(PSX + '/' + name + '.png').convert('RGB').crop((0, 134, 256, 256)).resize((SIZE, SIZE), Image.BICUBIC)
    c = np.asarray(image, dtype=np.float32) / 255.0
    return np.clip(c * 1.35, 0, 1)



def curtain():
    c = grade(load(PACKS + '/Fabric_050.jpg'), saturation=0, tint=(.86, 1.0, .94), mean=176, contrast=.7)
    x = np.arange(SIZE, dtype=np.float32)
    pleat = .82 + .18 * np.sin(x / SIZE * 2 * np.pi * 4) ** 2
    return np.clip(c * pleat[None, :, None], 0, 1)


TEXTURES = [
    # name, builder
    ('surface_concrete_wall', lambda: grade(load(PSX + '/concrete_wall_3.png'), saturation=.6, mean=112)),
    ('surface_concrete_floor', lambda: grade(load(PSX + '/concrete_pt_1.png'), saturation=.7, mean=108)),
    ('surface_concrete_ceiling', lambda: grade(load(PSX + '/concrete_tx_6.png'), saturation=.6, mean=104)),
    ('surface_brick_wall', lambda: grade(load(PSX + '/brick_wall_pt_9.png'), saturation=.4, tint=(1.0, .97, .88), mean=128)),
    ('surface_brick_floor', lambda: grade(load(PSX + '/old_concrete_pt_1.png'), saturation=.7, mean=100)),
    ('surface_brick_ceiling', lambda: grade(load(PSX + '/concrete_wall_pt_10_2.png'), saturation=.6, mean=100)),
    ('surface_clinic_wall', lambda: grade(load(PSX + '/tiles_pt_1.png'), saturation=0, tint=(.95, 1.0, .96), mean=182, contrast=1.2)),
    ('surface_clinic_floor', lambda: grade(load(PSX + '/tiles_floor_pt_3.png'), saturation=0, tint=(.95, 1.0, .96), mean=150, contrast=1.25)),
    ('surface_clinic_ceiling', lambda: grade(load(PSX + '/hl_office_complex_style_drop_ceiling_1.png'), saturation=0, tint=(1.0, 1.0, .96), mean=168)),
    ('surface_lab_wall', lambda: grade(load(EMX + '/wall_emx_2_2.png'), saturation=.12, tint=(.97, .99, 1.0), mean=190)),
    ('surface_lab_floor', lambda: grade(load(EMX + '/tiles_emx_1_2.png'), saturation=.15, tint=(.95, .97, 1.0), mean=150)),
    ('surface_lab_ceiling', luminous),
    ('surface_rock_wall', lambda: grade(load(PSX + '/rock_pt_4.png'), saturation=.8, mean=104)),
    ('surface_rock_floor', lambda: grade(load(PSX + '/dirt_3.png'), saturation=.8, mean=88)),
    ('surface_rock_ceiling', lambda: grade(load(PSX + '/rock_pt_1.png'), saturation=.8, mean=90)),
    ('surface_corrugated_wall', lambda: grade(load(PACKS + '/roof_hr_4.png'), saturation=1.0, mean=100)),
    ('surface_corrugated_floor', lambda: grade(load(PSX + '/dirt_pt_8.png'), saturation=.8, mean=92)),
    ('surface_corrugated_ceiling', lambda: grade(load(PSX + '/corrugated_steel_pt_4.png'), saturation=.8, mean=92)),
    ('surface_timber_wall', lambda: grade(load(EMX + '/wall_emx_8_1.png'), saturation=.45, tint=(1.0, .95, .86), mean=90)),
    ('surface_timber_floor', lambda: grade(load(PACKS + '/gravel_hr_1.png'), saturation=.75, mean=96)),
    ('surface_stone_wall', lambda: grade(load(PSX + '/stone_wall_pt_2.png'), saturation=.75, mean=112)),
    ('surface_stone_floor', flagstone),
    ('surface_stone_ceiling', lambda: grade(load(PSX + '/stone_wall_pt_1.png'), saturation=.6, mean=94)),
    ('surface_growth', growth),
    ('surface_crystal', crystal),
    # Painted clinic walls: the plain upper paint, and the dado bands (colour
    # and skirting) cut from the bottom of the pack's two-tone walls. A band is
    # drawn once from the floor up (World.h painted surfaces, materials 36-38).
    ('surface_paint_upper', lambda: grade(load(PSX + '/painted_concrete_wall_pt_5_plain.png'), saturation=.2, tint=(1.0, .99, .96), mean=168)),
    ('surface_paint_band_teal', lambda: dado('painted_concrete_wall_pt_5_2')),
    ('surface_paint_band_green', lambda: dado('painted_concrete_wall_pt_5_3')),
    ('surface_paint_band_maroon', lambda: dado('painted_concrete_wall_pt_5_1')),
    # Ward privacy curtains: the woven fabric, pale hospital green, pleated.
    ('surface_curtain', curtain),
]
FIRST_RESOURCE = 2100
# Finishes past the first 25 continue in the free block at 2150.
SECOND_RESOURCE, FIRST_BLOCK = 2150, 25


def resource(index):
    return FIRST_RESOURCE + index if index < FIRST_BLOCK else SECOND_RESOURCE + index - FIRST_BLOCK


def main(args):
    os.makedirs(OUT, exist_ok=True)
    built = []
    for index, (name, build) in enumerate(TEXTURES):
        c = build()
        image = Image.fromarray(np.round(c * 255).astype(np.uint8))
        path = os.path.join(OUT, name + '.png')
        image.save(path, optimize=True)
        built.append((name, image))
        print('%d RCDATA "assets/library/textures/%s.png"  mean %.0f' % (resource(index), name, luma(c).mean() * 255))
    if len(args) == 2 and args[0] == '--sheet':
        # Each texture tiled 2x2 so seams show.
        cell = 160
        sheet = Image.new('RGB', (cell * 2 * 5, cell * 2 * ((len(built) + 4) // 5)), (40, 40, 40))
        for i, (name, image) in enumerate(built):
            small = image.resize((cell, cell), Image.LANCZOS)
            x, y = (i % 5) * cell * 2, (i // 5) * cell * 2
            for a in range(2):
                for b in range(2):
                    sheet.paste(small, (x + a * cell, y + b * cell))
        sheet.save(args[1])


if __name__ == '__main__':
    main(sys.argv[1:])
