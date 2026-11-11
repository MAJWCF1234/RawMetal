"""Chapter 2 key art: eight stills, cover, banner, logo and a review board, in the Chapter 1 grade."""
import os
import sys
import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, 'N:/rawmetal/.build/promo-1.0')
import look

OUT = 'out'
os.makedirs(OUT, exist_ok=True)
SHOT = '.build/shot/%s.ppm'


def src2x(name):
    return Image.open(SHOT % name).convert('RGB').resize((1920, 1080), Image.NEAREST)


def crop_to(im, box, size):
    return im.resize(size, Image.LANCZOS, box=box)


LOGO = look.wordmark(2400, chapter=True, label='CHAPTER 2')
LOGO_BARE = look.wordmark(2400, chapter=False)
LOGO.save(os.path.join(OUT, 'depthworks-logo-chapter2.png'))

# ---- screenshots: 1920x1080, graded, one subject each, in the order a player meets them ------
SHOTS = [
    ('shot-01.png', 'ch2_theatre', 1.0, 'Intake theatre: the welcome film, seats for a shift that never came (chunk 68)'),
    ('shot-02.png', 'ch2_hall69', 1.0, 'Screening hall: the gates to the laboratories (chunk 69)'),
    ('shot-03.png', 'ch2_suits', 1.05, 'Suit room: the hazmat rack where the staff hung up their shift (chunk 69)'),
    ('shot-04.png', 'ch2_pan', 1.3, 'Calibration pan: standing water under the gantry (chunk 84)'),
    ('shot-05.png', 'ch2_plantbus', 1.15, 'The 11 kV plant bus, live and arcing on the floor (chunk 92)'),
    ('shot-06.png', 'ch2_towers', 1.1, 'Test bay: the tower rig over its pit (chunk 86)'),
    ('shot-07.png', 'ch2_breach2', 1.0, 'Containment breach: the growth and the crystals it feeds (chunk 102)'),
    ('shot-08.png', 'ch2_breach3', 1.0, 'Containment breach, the far wall (chunk 102)'),
]
for fn, name, exp, what in SHOTS:
    im = look.grade(src2x(name), exposure=exp, seed=11, grain=.03, scan=.05)
    im.save(os.path.join(OUT, fn))

# ---- cover 630x500: the breach crystals, wordmark over the dark growth --------------------------
src = src2x('ch2_breach2')
ch = 1040; cw = int(ch * 630 / 500)
x0 = int(.62 * 1920 - cw * .5); x0 = max(0, min(1920 - cw, x0)); y0 = 1080 - ch
cov = crop_to(src, (x0, y0, x0 + cw, y0 + ch), (630, 500))
cov = look.grade(cov, exposure=1.0, seed=3, grain=.03, scan=.05, vignette=.95, fringe=1)
a = np.asarray(cov).astype(np.float32)
ramp = np.clip(np.linspace(0.18, 1.15, 500), 0, 1)[:, None, None]
cov = Image.fromarray((a * ramp).clip(0, 255).astype(np.uint8))
lg = LOGO_BARE.resize((574, int(LOGO_BARE.height * 574 / LOGO_BARE.width)), Image.LANCZOS)
cov.paste(lg, (315 - lg.width // 2, 16), lg)
d = ImageDraw.Draw(cov)
look.draw_tracked(d, (315, 16 + lg.height - 6), 'CHAPTER 2   ·   ENERGY RESEARCH', look.mono(17), (222, 228, 222), 5, 'ma')
cov.save(os.path.join(OUT, 'cover-630x500.png'))
cov.resize((315, 250), Image.LANCZOS).save(os.path.join(OUT, 'cover-check-315x250.png'))

# ---- banner 1920x540: the breach's far wall, crystals right, wordmark left over the growth ----
src = src2x('ch2_breach3')
ban = crop_to(src, (0, 120, 1920, 660), (1920, 540))
ban = look.grade(ban, exposure=1.0, seed=5, grain=.03, scan=.05, vignette=.6)
a = np.asarray(ban).astype(np.float32)
ramp = np.clip(np.linspace(.2, 1.1, 1920) ** 1.2, 0, 1)[None, :, None]
ban = Image.fromarray((a * ramp).clip(0, 255).astype(np.uint8))
lg = LOGO.resize((820, int(LOGO.height * 820 / LOGO.width)), Image.LANCZOS)
ban.paste(lg, (70, 270 - lg.height // 2), lg)
ban.save(os.path.join(OUT, 'banner-1920x540.png'))

# ---- review board: everything on one sheet ------------------------------------------------------
tiles = [Image.open(os.path.join(OUT, fn)).resize((960, 540), Image.LANCZOS) for fn, *_ in SHOTS]
board = Image.new('RGB', (1920, 540 * 4 + 540 + 520), (12, 14, 13))
for i, t in enumerate(tiles):
    board.paste(t, ((i % 2) * 960, (i // 2) * 540))
board.paste(Image.open(os.path.join(OUT, 'banner-1920x540.png')), (0, 540 * 4))
board.paste(Image.open(os.path.join(OUT, 'cover-630x500.png')), (20, 540 * 5 + 10))
board.paste(Image.open(os.path.join(OUT, 'cover-check-315x250.png')), (680, 540 * 5 + 10))
lg = LOGO.resize((880, int(LOGO.height * 880 / LOGO.width)), Image.LANCZOS)
board.paste(lg, (1020, 540 * 5 + 120), lg)
board.save(os.path.join(OUT, 'review-board.png'))
board.resize((960, board.height // 2), Image.LANCZOS).save('review-small.png')
print('done', board.size)
