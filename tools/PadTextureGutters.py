"""Grow a texture atlas's islands out into its empty (near-black) gutters.

  python tools/PadTextureGutters.py IN.png OUT.png [threshold] [passes]

Creature sheets (the Snatcher's, the Chimera's) leave the space between UV
islands black. Bilinear filtering at a seam and the smaller mip levels average
that black in, which shows as dark holes and black edges on the model. Each
pass fills every empty texel that touches a filled one with the mean of its
filled neighbours, so the islands' own colours bleed outward instead.
"""
import sys
import numpy as np
from PIL import Image

src, dst = sys.argv[1], sys.argv[2]
threshold = int(sys.argv[3]) if len(sys.argv) > 3 else 14
passes = int(sys.argv[4]) if len(sys.argv) > 4 else 40
colour = np.asarray(Image.open(src).convert('RGB')).astype(np.float32)
filled = (colour.max(axis=2) >= threshold).astype(np.float32)
colour *= filled[..., None]
for _ in range(passes):
    if filled.all():
        break
    total = np.zeros_like(colour); count = np.zeros_like(filled)
    for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
        total += np.roll(np.roll(colour, dy, 0), dx, 1)
        count += np.roll(np.roll(filled, dy, 0), dx, 1)
    grow = (filled == 0) & (count > 0)
    colour[grow] = total[grow] / count[grow][:, None]
    filled[grow] = 1
Image.fromarray(colour.clip(0, 255).astype(np.uint8)).save(dst)
print(dst, 'empty texels left:', int((filled == 0).sum()))
