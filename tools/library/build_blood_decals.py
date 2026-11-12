"""Blood decals from the Resource Boy blood pack (O:/retro official/Resource-Boy-Blood-Textures.zip,
royalty-free, no attribution required): the pack's black splatters, cut into one sheet per job.

Each sheet is a row of square cells (tall drips get tall cells). A pixel's alpha is the pack's
coverage; its grey is how wet the blood is there, darker where it lies thick (the middle of a pool,
the head of a drip) and brighter at the thin edges, so the engine's tint (blood colour times grey,
SoftwareRenderer m_bloodVariants) gives a wet core and a lighter rim, and the room lights it.

  python tools/library/build_blood_decals.py    -> src/assets/library/blood/blood-<job>.png
"""
import io
import os
import zipfile
import numpy as np
from PIL import Image, ImageFilter

PACK = 'O:/retro official/Resource-Boy-Blood-Textures.zip'
OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src', 'assets', 'library', 'blood'))
# job -> (pack numbers, cell width, cell height)
JOBS = {
    'pool':   ([70, 73, 110, 111, 112, 98], 128, 128),            # under a body, wide and heavy
    'splat':  ([66, 79, 86, 104, 103, 77, 67, 76], 96, 96),        # a drop or a gout landing on the floor
    'burst':  ([132, 125, 124, 90, 101, 129, 123, 130], 96, 96),   # a round's spray on a wall, radial
    'streak': ([36, 29, 81, 16, 45, 46], 128, 128),                # thrown spray, one way
    'drip':   ([53, 54, 55, 56, 59, 60, 105, 106], 32, 128),       # a run down a wall from a hit
    'drops':  ([115, 108, 82, 37, 87, 97], 64, 64),                # a scatter of small drops
}


def cell(img, w, h):
    """The pack image fitted (aspect kept) into a w x h cell, alpha = coverage, grey = wetness."""
    a = np.asarray(img.convert('RGBA'), np.float32)[..., 3] / 255
    ys, xs = np.nonzero(a > .02)
    a = a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]
    src = Image.fromarray((a * 255).astype(np.uint8))
    pad = 2
    scale = min((w - 2 * pad) / src.width, (h - 2 * pad) / src.height)
    src = src.resize((max(1, round(src.width * scale)), max(1, round(src.height * scale))), Image.LANCZOS)
    alpha = Image.new('L', (w, h))
    alpha.paste(src, ((w - src.width) // 2, (h - src.height) // 2))
    al = np.asarray(alpha, np.float32) / 255
    # thickness: how far inside the shape a pixel is, from a blur of the coverage
    thick = np.asarray(alpha.filter(ImageFilter.GaussianBlur(max(1.5, min(w, h) / 24))), np.float32) / 255
    grey = np.clip(1.0 - .28 * thick ** 1.4, .72, 1.0)
    rgba = np.zeros((h, w, 4), np.uint8)
    rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = (grey * 255).astype(np.uint8)
    rgba[..., 3] = (np.clip(al, 0, 1) * 255).astype(np.uint8)
    return rgba


def main():
    os.makedirs(OUT, exist_ok=True)
    z = zipfile.ZipFile(PACK)
    names = {os.path.splitext(os.path.basename(n))[0]: n for n in z.namelist() if n.lower().endswith('.png')}
    for job, (numbers, w, h) in JOBS.items():
        sheet = np.zeros((h, w * len(numbers), 4), np.uint8)
        for i, n in enumerate(numbers):
            img = Image.open(io.BytesIO(z.read(names[str(n)])))
            sheet[:, i * w:(i + 1) * w] = cell(img, w, h)
        Image.fromarray(sheet, 'RGBA').save(os.path.join(OUT, f'blood-{job}.png'), optimize=True)
        print(job, len(numbers), 'cells', f'{w}x{h}')


if __name__ == '__main__':
    main()
