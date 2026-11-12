"""Chapter 2 promo v2: stills, cover, banner, YouTube thumbnail, logo and the review board,
in the "cold hold" look (look14.py). Run from .build/promo-1.4 after shots.py, film96.py and the
Machine take (mach2/). Writes into OUT (default: docs/Content/Promotional/chapter-2/v2).
"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import look14 as L
import look

OUT = sys.argv[1] if len(sys.argv) > 1 else 'N:/rawmetal/docs/Content/Promotional/chapter-2/v2'
os.makedirs(OUT, exist_ok=True)

def src(folder, name, k, size=(1920, 1080)):
    return L.load(os.path.join(HERE, folder, '.build', 'film', '%s_%03d.ppm' % (name, k)), size)

BEAST = dict(crush=.15, contrast=1.3, sat=.4)   # R-117: a near-silhouette, lit only at the edges
def still(folder, name, k, exp, zoom=1.0, cx=.5, cy=.5, seed=11, beast=False):
    im = src(folder, name, k)
    if zoom != 1: im = L.crop(im, zoom, cx, cy)
    if beast: return L.grade(im, exposure=exp, grain=.035, seed=seed, weave=False, vignette=.85, **BEAST)
    return L.grade(im, exposure=exp, sat=.5, grain=.035, seed=seed, weave=False, vignette=.85)

STILLS = [
    ('shot-01.png', ('env', 'theatre', 60, .95, 1.0), 'Intake theatre: the empty rows under the welcome film (chunk 68)'),
    ('shot-02.png', ('env', 'redhall', 90, .9, 1.0), 'Containment approach: the red hall, a door left open on the dark (chunk 100)'),
    ('shot-03.png', ('env', 'labs', 40, .95, 1.0), 'Energy Test Labs: the glass test chambers (chunk 93)'),
    ('shot-04.png', ('env', 'rock', 90, .95, 1.0), 'Containment B: the rock and the growth come through the wall (chunk 98)'),
    ('shot-05.png', ('env', 'cages', 60, 2.1, 1.3, .5, .5), 'R-117 in the dark behind the bars of the specimen cages (chunk 99)'),
    ('shot-06.png', ('r96', 'wake', 92, 1.3, 1.6, .58, .35), 'R-117 against the red work lamp of the service-run door, Containment A (chunk 96)'),
    ('shot-07.png', ('r96', 'pounce', 173, 1.5, 1.6, .45, .38), 'R-117 rearing over the post man, jaw open (chunk 96)'),
    ('shot-08.png', ('mach2', 'mlow', 55, 1.3, 1.25, .5, .38), 'Experimental Power: the cursed equipment, its lamp lit in the dark (chunk 92)'),
]
made = {}
for fn, (folder, name, k, exp, *z), what in STILLS:
    zoom = z[0] if z else 1.0; cx = z[1] if len(z) > 1 else .5; cy = z[2] if len(z) > 2 else .5
    im = still(folder, name, k, exp, zoom, cx, cy, beast=fn in ('shot-05.png', 'shot-06.png', 'shot-07.png'))
    im.save(os.path.join(OUT, fn)); made[fn] = im

LOGO = look.wordmark(2400, chapter=True, label='CHAPTER 2')
LOGO_BARE = look.wordmark(2400, chapter=False)
LOGO.save(os.path.join(OUT, 'depthworks-logo-chapter2-v2.png'))

def paste_logo(canvas, logo, width, x, y):
    lg = logo.resize((width, int(logo.height * width / logo.width)), Image.LANCZOS)
    canvas.paste(lg, (int(x - lg.width / 2), int(y)), lg)
    return lg.size

def red_tag(d, xy, text, fnt, tr, anchor='m'):
    tw = look.text_width(text, fnt, tr); x, y = xy
    if anchor == 'm': x -= tw / 2
    asc = fnt.getmetrics()[0]
    d.rectangle([x - 10, y - 5, x + tw + 10, y + asc + 7], fill=L.RED)
    look.draw_tracked(d, (x, y), text, fnt, (240, 228, 224), tr)

# ---- cover 630x500: R-117 a shape against the red work lamp of the service-run door; wordmark on the dark floor -----
s = src('r96', 'wake', 92)
ch = 980; cw = int(ch * 630 / 500); x0 = int(.52 * 1920 - cw / 2); y0 = 0
cov = s.resize((630, 500), Image.LANCZOS, box=(x0, y0, x0 + cw, y0 + ch))
cov = L.grade(cov, exposure=1.3, grain=.03, seed=3, weave=False, vignette=.9, **BEAST)
a = np.asarray(cov).astype(np.float32)
ramp = np.clip(np.linspace(1.15, -.05, 500), .12, 1)[:, None, None] ** .8
cov = Image.fromarray((a * ramp).clip(0, 255).astype(np.uint8))
lg = LOGO_BARE.resize((540, int(LOGO_BARE.height * 540 / LOGO_BARE.width)), Image.LANCZOS)
cov.paste(lg, (315 - lg.width // 2, 330), lg)
d = ImageDraw.Draw(cov)
look.draw_tracked(d, (315, 330 + lg.height - 4), 'CHAPTER 2   ·   ENERGY RESEARCH', look.mono(17), L.BONE, 5, 'ma')
red_tag(d, (315, 462), 'COMING SOON', look.mono(17), 6)
cov.save(os.path.join(OUT, 'cover-630x500.png'))
cov.resize((315, 250), Image.LANCZOS).save(os.path.join(OUT, 'cover-check-315x250.png'))

# ---- banner 1920x540: the breach (102), the growth through the floor; wordmark on the dark left ---------------
s = src('env', 'breach', 60)
ban = s.resize((1920, 540), Image.LANCZOS, box=(0, 200, 1920, 740))
ban = L.grade(ban, exposure=1.0, sat=.5, grain=.03, seed=5, weave=False, vignette=.6)
a = np.asarray(ban).astype(np.float32)
lr = np.clip(np.linspace(-.25, 1.6, 1920), .08, 1)[None, :, None]
ban = Image.fromarray((a * lr).clip(0, 255).astype(np.uint8))
lg = LOGO.resize((760, int(LOGO.height * 760 / LOGO.width)), Image.LANCZOS)
ban.paste(lg, (90, 270 - lg.height // 2), lg)
d = ImageDraw.Draw(ban)
look.draw_tracked(d, (112, 270 + lg.height // 2 + 8), 'ENERGY RESEARCH   ·   COMING SOON', look.mono(24), L.BONE, 7)
ban.save(os.path.join(OUT, 'banner-1920x540.png'))

# ---- YouTube thumbnail 1280x720: R-117 rearing over the post man in the dark, jaw lit; big CHAPTER 2 -------------------------------
s = src('r96', 'pounce', 173)
th = L.crop(s, 1.5, .3, .4, size=(1280, 720))
th = L.grade(th, exposure=1.5, grain=.03, seed=7, weave=False, vignette=.7, **BEAST)
a = np.asarray(th).astype(np.float32)
lr = np.clip(np.linspace(-.1, 1.6, 1280), .1, 1)[None, :, None]
th = Image.fromarray((a * lr).clip(0, 255).astype(np.uint8))
lg = LOGO_BARE.resize((560, int(LOGO_BARE.height * 560 / LOGO_BARE.width)), Image.LANCZOS)
th.paste(lg, (36, 60), lg)
d = ImageDraw.Draw(th)
f = look.font(132)
look.draw_tracked(d, (58, 60 + lg.height + 6), 'CHAPTER 2', f, L.BONE, 8)
red_tag(d, (62, 60 + lg.height + 170), 'IT EATS WHERE IT KILLS', look.mono(28), 5, anchor='l')
th.save(os.path.join(OUT, 'youtube-thumbnail-1280x720.png'))

# ---- review board ------------------------------------------------------------------------------------------
W = 1920
rows = []
pairs = [(made['shot-%02d.png' % i], made['shot-%02d.png' % (i + 1)]) for i in (1, 3, 5, 7)]
CS = os.path.join(OUT, 'trailer-contact-sheet.png')
cs_img = Image.open(CS).convert('RGB') if os.path.exists(CS) else None
H = 4 * 540 + 540 + 520 + 60 + (cs_img.height + 20 if cs_img else 0)
board = Image.new('RGB', (W, H), (14, 14, 15)); d = ImageDraw.Draw(board)
y = 0
for a, b in pairs:
    board.paste(a.resize((960, 540), Image.LANCZOS), (0, y)); board.paste(b.resize((960, 540), Image.LANCZOS), (960, y)); y += 540
board.paste(ban, (0, y)); y += 540 + 20
board.paste(cov, (20, y)); board.paste(cov.resize((315, 250), Image.LANCZOS), (670, y))
board.paste(th.resize((640, 360), Image.LANCZOS), (1010, y))
if cs_img: board.paste(cs_img, (0, y + 540))
look.draw_tracked(d, (20, H - 44), 'DEPTHWORKS  ·  CHAPTER 2 PROMO V2  ·  COLD HOLD', look.mono(20), (150, 156, 152), 4)
board.save(os.path.join(OUT, 'review-board.png'))
for fn, _, what in STILLS: print(fn, '-', what)
