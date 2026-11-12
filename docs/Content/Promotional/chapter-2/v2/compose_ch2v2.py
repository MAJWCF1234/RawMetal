"""Chapter 2 promo v2: the trailer's frames (1920x1080, 30 fps, 2.20:1 picture band) in the
"cold hold" look (look14.py), cut to the score (score_ch2v2.py; the beat grid is shared).

  python compose_ch2v2.py OUTDIR [W H] [--only=a,b,c] [--jobs=N]

Sources are --film takes: r96/ (the kill in 96, film96.py), env/ (shots.py), mach2/ (the Machine).
"""
import os, sys, math
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import look14 as L
import look

FPS = 30
DROP = 810                      # frame of the score's drop (track 44.65 s)
BEAT = .6965 * FPS              # the score's pulse after the drop, in frames
STOP = 1499                     # the hard stop to silence
TITLE = 1530                    # the sting and the title
ENDCARD = 1650                  # the end card's lines join the title (0:55)
BUTTON = 1710                   # the Machine's lamp-eye in the dark (0:57)
END = 1785                      # out at 0:59.5
SRC = {
    **{k: 'r96/.build/film/%s_%%03d.ppm' % k for k in ('wake', 'bite2', 'pounce', 'bite', 'feed3', 'feedw', 'turn')},
    **{k: 'env/.build/film/%s_%%03d.ppm' % k for k in ('theatre', 'morgue', 'labs', 'rock', 'redhall', 'booth', 'breach', 'cages', 'rush', 'hall')},
    **{k: 'mach2/.build/film/%s_%%03d.ppm' % k for k in ('mlow', 'mtop')},
}

def b(n): return int(round(DROP + BEAT * n))   # frame of the n-th pulse after the drop

# ---- the edit ------------------------------------------------------------------------------------
# (start frame, end frame, source, first source frame, options)
# options: exp, zoom, zoom2 (push to), cx, cy, step (source frames per output frame), shake (amp px),
#          flash ([rel frames]), tear (n frames at the head), smear, hit (rel frame of blood on lens),
#          wipe (n frames: blood wipe in from the previous shot's last frame)
EDIT = []
def seg(a, z, src, first, **o): EDIT.append((a, z, src, first, o))

# R-117 is never shown whole and evenly lit (fix pass): its takes are shot in true dark with the
# lamps at 30% (film96.py, shots.py), and here every creature shot is graded down to a near-silhouette
# (exposure about half, blacks crushed, contrast up), cropped tight to a part (the jaw, the spine
# line, a foreleg, the head against the red door), whole-body views only a few frames in motion.
CREATURE = dict(crush=.15, contrast=1.3, sat=.4)
def cseg(a, z, src, first, exp=.85, **o):
    seg(a, z, src, first, exp=exp * 1.3, **CREATURE, **o)

# Act 1, cold: the research levels as they were left
seg(90, 195, 'theatre', 0, exp=.95, zoom=1.08, zoom2=1.14, cx=.55)
seg(195, 300, 'morgue', 0, exp=.62, zoom=1.05, zoom2=1.1)
seg(300, 390, 'labs', 10, exp=1.25, zoom=1.12, cx=.42)
seg(390, 480, 'redhall', 0, exp=.9, zoom=1.04, zoom2=1.1)
seg(480, 570, 'rock', 10, exp=.95, zoom=1.06, zoom2=1.12, cx=.6)
seg(570, 645, 'breach', 10, exp=.85, zoom=1.04, zoom2=1.08)
cseg(645, 720, 'cages', 20, exp=1.6, zoom=1.35, cx=.55, cy=.48)
cseg(720, DROP, 'hall', 0, exp=1.3, zoom=1.6, zoom2=1.75, cx=.33, cy=.5)
# The drop: R-117's kill in Containment A, cut to the pulse
cseg(DROP, b(2), 'wake', 86, exp=1.0, zoom=1.9, zoom2=2.2, cx=.58, cy=.3, flash=[0, 1], tear=3)
cseg(b(2), b(3), 'bite2', 149, zoom=1.6, cx=.43, cy=.38)
cseg(b(3), b(4), 'pounce', 161, exp=1.2, zoom=1.8, cx=.45, cy=.35, shake=14, smear=True, flash=[0])
cseg(b(4), b(5), 'bite', 199, exp=.8, zoom=1.6, cx=.64, cy=.36, hit=8, flash=[8, 9], shake=10)
cseg(b(5), b(7), 'feedw', 259, exp=1.0, zoom=2.0, zoom2=2.2, cx=.54, cy=.45, hit=-12)
cseg(b(7), b(9), 'turn', 431, exp=.9, zoom=1.7, zoom2=1.9, cx=.6, cy=.5, smear=True)
cseg(b(9), b(12), 'rush', 84, exp=1.15, zoom=1.5, zoom2=1.8, cx=.42, cy=.35, wipe=12)
cseg(b(12), b(12) + 18, 'rush', 132, exp=.8, zoom=2.0, cx=.4, cy=.3, shake=22, smear=True)
seg(b(12) + 18, b(13), None, 0)
cseg(b(13), b(15), 'cages', 40, exp=1.6, zoom=1.6, zoom2=1.75, cx=.52, cy=.48, tear=2)
cseg(b(15), b(17), 'hall', 100, exp=1.1, zoom=2.1, zoom2=2.3, cx=.47, cy=.3)
cseg(b(17), b(18), 'hall', 150, exp=.9, zoom=2.0, cx=.55, cy=.3, shake=18, flash=[0])
# the Machine: casing, lamp-eye and the arcs across it, never its legs
seg(b(18), b(19), 'mtop', 70, exp=.85, zoom=1.35, cx=.42, cy=.32)
seg(b(19), b(20), 'mtop', 84, exp=.85, zoom=1.6, cx=.4, cy=.3, tear=2)
seg(b(20), b(21), 'breach', 40, exp=.85, zoom=1.15)
# the run: half-pulse cuts; R-117 only in parts
RUN = [('redhall', 40, .9, 1.2, False), ('bite', 203, .8, 1.8, True), ('labs', 60, 1.25, 1.2, False), ('turn', 450, .9, 1.9, True),
       ('morgue', 50, .62, 1.2, False), ('hall', 175, 1.0, 1.9, True), ('rock', 50, .95, 1.15, False), ('rush', 128, 1.25, 1.8, True),
       ('mtop', 76, .85, 1.5, False), ('feedw', 300, 1.0, 2.2, True), ('wake', 110, 1.0, 2.3, True), ('pounce', 170, .85, 2.0, True),
       ('cages', 70, 1.6, 1.9, True), ('hall', 150, 1.0, 1.9, True), ('mtop', 88, .85, 1.6, False), ('bite2', 200, .85, 1.9, True)]
for i, (src, first, exp, zoom, beast) in enumerate(RUN):
    a = int(round(b(21) + BEAT / 2 * i)); z = int(round(b(21) + BEAT / 2 * (i + 1)))
    (cseg if beast else seg)(a, z, src, first, exp=exp, zoom=zoom, cx=.5, cy=.4, flash=[0] if i % 2 == 0 else [], tear=1 if i % 4 == 1 else 0, smear=i % 3 == 0)
RUN_END = int(round(b(21) + BEAT / 2 * len(RUN)))
cseg(RUN_END, STOP - 21, 'turn', 412, exp=.9, zoom=1.6, zoom2=2.1, cx=.6, cy=.45)
cseg(STOP - 21, STOP, 'hall', 168, exp=1.0, zoom=1.8, cx=.5, cy=.35, shake=26, smear=True, flash=[0, 1])
# after the end card: the Machine's lamp-eye in the dark, then the floor goes live and shows it
seg(BUTTON, END - 27, 'mtop', 30, exp=1.1, zoom=1.5, zoom2=1.7, cx=.42, cy=.3, step=1.25)

CAPTIONS = [  # (from, to, tag, line)
    (112, 188, 'INTAKE THEATRE', 'THE WELCOME FILM IS STILL RUNNING.'),
    (214, 292, 'MEDICAL DEEP', 'NOBODY CAME BACK FOR THEM.'),
    (330, 450, 'ENERGY TEST LABS', 'THE WORK WENT ON WITHOUT THEM.'),
    (652, 770, 'SPECIMEN R-117', 'IT IS NOT IN ITS CELL.'),
    (b(5) + 4, b(7) - 2, 'CONTAINMENT A', 'IT EATS WHERE IT KILLS.'),
    (b(13) + 3, b(15) + 6, 'CONTAINMENT B', 'AND IT COMES BACK.'),
    (b(18), b(21) - 2, 'EXPERIMENTAL POWER', 'THE EQUIPMENT WAKES UP TOO.'),
]

def frame_of(f):
    for a, z, src, first, o in EDIT:
        if a <= f < z: return a, z, src, first, o
    return None

# ---- picture --------------------------------------------------------------------------------------
W, H = 1920, 1080
BAND = int(round(1920 / 2.20)) // 2 * 2
TOP = (H - BAND) // 2

def src_img(src, k):
    p = os.path.join(HERE, SRC[src] % k)
    if not os.path.exists(p):  # hold the last frame there is
        while k > 0 and not os.path.exists(os.path.join(HERE, SRC[src] % k)): k -= 1
        p = os.path.join(HERE, SRC[src] % k)
    return Image.open(p).convert('RGB')

def picture(f, seed):
    hit = frame_of(f)
    if not hit or not hit[2]: return None
    a, z, src, first, o = hit
    rel = f - a
    k = first + int(rel * o.get('step', 1))
    im = src_img(src, k)
    if o.get('smear'):
        im = L.smear(im, src_img(src, max(first, k - 1)), src_img(src, max(first, k - 2)), 1.0)
    u = rel / max(1, z - a - 1)
    zoom = o.get('zoom', 1.0) + (o.get('zoom2', o.get('zoom', 1.0)) - o.get('zoom', 1.0)) * u
    dx = dy = 0
    if o.get('shake'): dx, dy = L.shake(rel, o['shake'], seed=a)
    big = im.resize((1920, 1080), Image.NEAREST)
    # the band: crop a 2.20 window out of the 16:9 frame, punched in
    cw = 1920 / zoom; ch = cw / 2.20
    cx, cy = o.get('cx', .5), o.get('cy', .5)
    x0 = min(max(cx * 1920 - cw / 2 + dx, 0), 1920 - cw); y0 = min(max(cy * 1080 - ch / 2 + dy, 0), 1080 - ch)
    pic = big.resize((W, BAND), Image.LANCZOS, box=(x0, y0, x0 + cw, y0 + ch))
    pic = L.grade(pic, exposure=o.get('exp', 1.0), seed=seed, **{k: o[k] for k in ('crush', 'contrast', 'sat') if k in o})
    if 'hit' in o:
        h = rel - o['hit']
        if 0 <= h < 30: pic = L.blood_hit(pic, h, seed=o.get('hitseed', 1))
    if o.get('wipe') and rel < o['wipe']:
        prev = picture(a - 1, seed)
        if prev is not None: pic = L.blood_wipe(prev, pic, (rel + 1) / (o['wipe'] + 1), seed=a)
    if rel in o.get('flash', []): pic = L.flash(pic, .6 if rel == o['flash'][0] else .25)
    if rel < o.get('tear', 0): pic = L.tear(pic, 1.0 - rel * .3, seed + 9)
    return pic

def black(seed):
    rng = np.random.default_rng(seed)
    a = np.full((H, W, 3), 4.0, np.float32) + rng.normal(0, 2.2, (H, W, 1))
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

LOGO = look.wordmark(1080, chapter=False)
F_CREDIT = L.cap_font(38)
F_SUB = look.mono(40)
F_SMALL = look.mono(30)

def tube(t, seq=(1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1)):
    return seq[t] if 0 <= t < len(seq) else (1 if t >= 0 else 0)

def text_layer(img, xy, text, fnt, fill, tracking, alpha, anchor='ma'):
    if alpha <= 0: return img
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0))
    look.draw_tracked(ImageDraw.Draw(layer), xy, text, fnt, fill + (255,), tracking, anchor)
    if alpha < 1: layer.putalpha(layer.getchannel('A').point(lambda v: int(v * alpha)))
    out = img.convert('RGBA'); out.alpha_composite(layer); return out.convert('RGB')

def logo_on(img, t, y, scale=1.0, alpha=1.0):
    al = tube(t) * alpha
    if al <= 0: return img
    lg = LOGO if scale == 1 else LOGO.resize((int(LOGO.width * scale), int(LOGO.height * scale)), Image.LANCZOS)
    if al < 1:
        lg = lg.copy(); lg.putalpha(lg.getchannel('A').point(lambda v: int(v * al)))
    out = img.convert('RGBA'); out.alpha_composite(lg, (W // 2 - lg.width // 2, int(y - lg.height / 2))); return out.convert('RGB')

def overlays(img, f):
    # the studio credit over the cold open
    if 20 <= f < 84:
        t = f - 20; al = min(1, t / 10, (84 - f) / 10) * (.4 if t < 6 and t % 2 else 1)
        img = text_layer(img, (W / 2, H / 2 - 20), 'MAJWCF1234  PRESENTS', F_CREDIT, L.BONE, 10, al)
    for a, z, tag, line in CAPTIONS:
        if a <= f <= z: img = L.caption(img, line, tag, f - a, z - a, TOP + BAND - 150)
    # the title (0:51), COMING SOON (0:53.3), the end card's Chapter 1 line joining it (0:55),
    # burnt out into the Machine's tease (0:56.2-0:57)
    if TITLE <= f < BUTTON:
        t = f - TITLE
        img = logo_on(img, t, H / 2 - 70, 1 + .03 * t / 180)
        if t >= 24:
            u = t - 24; al = min(1, u / 8) * (.35 if u < 6 and u % 2 else 1)
            img = text_layer(img, (W / 2, H / 2 + 85), 'CHAPTER 2   ·   ENERGY RESEARCH', F_SUB, (222, 228, 222), 9, al)
        if t >= 70:
            img = L.caption(img, 'COMING SOON', None, t - 70, 10 ** 6, H / 2 + 150)
        if f >= ENDCARD:
            u = f - ENDCARD; al = min(1, u / 10)
            img = text_layer(img, (W / 2, H / 2 + 300), 'CHAPTER 1   —   FREE ON ITCH.IO', F_SMALL, (170, 176, 172), 8, al)
        if t < 2: img = L.tear(img, .8, f)
        if f >= BUTTON - 24: img = L.burn(img, (f - (BUTTON - 24)) / 24.0, seed=4)
    return img

def render(f):
    img = black(f)
    pic = picture(f, f)
    if pic is not None: img.paste(pic, (0, TOP))
    # the first picture comes up out of the dark; the button fades up from nothing
    if 90 <= f < 104: img = Image.blend(black(f), img, (f - 90) / 14)
    if BUTTON <= f < BUTTON + 8: img = Image.blend(black(f), img, (f - BUTTON) / 8)
    img = overlays(img, f)
    return img

def work(args):
    f, out, size = args
    img = render(f)
    if size != (W, H): img = img.resize(size, Image.LANCZOS)
    img.save(os.path.join(out, 'f%04d.png' % f), compress_level=1)
    return f

if __name__ == '__main__':
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    size = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 and not sys.argv[2].startswith('--') else (W, H)
    only = None; jobs = 8
    for a in sys.argv[2:]:
        if a.startswith('--only='): only = [int(v) for v in a[7:].split(',') if v]
        if a.startswith('--jobs='): jobs = int(a[7:])
    frames = only or list(range(END))
    from multiprocessing import Pool
    with Pool(jobs) as p:
        for i, _ in enumerate(p.imap_unordered(work, [(f, out, size) for f in frames], chunksize=4)):
            if i % 200 == 0: print(i, flush=True)
    print('frames', len(frames), 'drop', DROP / FPS, 'stop', STOP / FPS, 'title', TITLE / FPS, 'end', END / FPS)
