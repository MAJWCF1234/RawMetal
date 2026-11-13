"""Chapter 2 promo v3 (2:28): the frames. Three acts: the levels as they were left (cinematic, cold),
the game being played (first person with the HUD, each fight replayed from a camera in front of the
player, then back to first person for the next), and R-117's kill in Containment A. Text only in black.

  python compose_v3.py OUTDIR [--only=a,b] [--jobs=N] [--list]

Sources (all --film takes): env/ (shots.py), r96/ (film96.py), mach2/ (film-machine.txt), gp/ (gameplay.py:
NAME_a first person with the HUD, NAME_b the replay camera).
"""
import os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import look14 as L
import look

FPS = 30
W, H = 1920, 1080
BAND = int(round(1920 / 2.20)) // 2 * 2
TOP = (H - BAND) // 2
STORM_BEAT = 1.0215 * FPS        # act 2's pulse ("Storm - dark - heavy - lurking"), frames
EYES_BEAT = .6965 * FPS          # act 3's pulse after the drop ("What Eyes Can't See")

R96 = ('wake', 'bite2', 'pounce', 'bite', 'feedw', 'feed3', 'turn')
ENV = ('theatre', 'morgue', 'labs', 'suits', 'pan', 'redhall', 'rock', 'breach', 'ring', 'cages', 'rush', 'hall', 'ward', 'gallery')

def path(src, k):
    if src in R96: return os.path.join(HERE, 'r96/.build/film/%s_%03d.ppm' % (src, k))
    if src in ENV: return os.path.join(HERE, 'env/.build/film/%s_%03d.ppm' % (src, k))
    if src in ('mtop', 'mlow'): return os.path.join(HERE, 'mach2/.build/film/%s_%03d.ppm' % (src, k))
    return os.path.join(HERE, 'gp/.build/film/%s_%04d.ppm' % (src, k))

# ---- the edit ----------------------------------------------------------------------------------------
# segments: (start, end, kind, src, first, opts); kinds: 'cine' (2.20 band, cold hold), 'beast' (R-117:
# near-silhouette), 'fp' (first person, full frame, the HUD readable), 'rep' (the replay camera, band,
# HUD cropped off), 'black' (opts: line).
EDIT = []
MARK = {}
t = 0
def seg(n, kind, src=None, first=0, **o):
    global t
    EDIT.append((t, t + n, kind, src, first, o)); t += n
def beats(b, base, pulse):
    """Frames from the current cursor to the next grid point b pulses on from base."""
    return max(1, int(round(base + b * pulse)) - t)

# ACT 1: the research levels as they were left
seg(90, 'black', credit=True)
seg(110, 'cine', 'theatre', 8, exp=.95, zoom=1.08, zoom2=1.14, cx=.55)
seg(104, 'cine', 'morgue', 0, exp=.62, zoom=1.05, zoom2=1.1)
seg(96, 'cine', 'ward', 6, exp=.95, zoom=1.04, zoom2=1.1)
seg(75, 'black', line='THE RESEARCH LEVELS NEVER CLOSED.')
seg(94, 'cine', 'labs', 10, exp=1.25, zoom=1.12, cx=.42)
seg(80, 'cine', 'suits', 40, exp=1.0, zoom=1.06, zoom2=1.12)
seg(96, 'cine', 'pan', 0, exp=1.05, zoom=1.04, zoom2=1.1)
seg(90, 'cine', 'redhall', 0, exp=.9, zoom=1.04, zoom2=1.1)
seg(90, 'cine', 'rock', 10, exp=.95, zoom=1.06, zoom2=1.12, cx=.6)
seg(96, 'cine', 'breach', 10, exp=.85, zoom=1.04, zoom2=1.09)
seg(100, 'cine', 'gallery', 8, exp=.9, zoom=1.06, zoom2=1.12, cy=.55)
seg(84, 'black', line='WHAT THEY KEPT DOWN HERE IS AWAKE.')
MARK['act2'] = t

# ACT 2: playing it. Cuts on the act's pulse; first person, then the same moment from outside.
A2 = t
cum = 0.0
def g(b, kind, src, first, **o):
    global cum
    cum += max(.5, round(b * 2) / 2)   # every act 2 cut on a pulse or half-pulse of the Storm track
    seg(beats(cum, A2, STORM_BEAT), kind, src, first, **o)
# v3.1: first person only (no replays: the player's body has no third-person animation)
g(2.447, 'fp', 's71_a', 0, exp=1.25)                 # crouched in Ward C's corridor, a sharpshooter on his round
g(2.154, 'fp', 'k93_a', 0, exp=1.15)                 # Conversion Lab: the shotgun, two rounds, it goes down
g(1.75, 'fp', 't83_a', 0, exp=1.1)                      # the Lab 4 door reader
g(1.958, 'fp', 'f88_a', 0, exp=1.15)                 # Observation Ring, the pit: the torch on something crawling
g(3.5, 'fp', 'g95_a', 10, exp=1.0)                      # Containment Intake: the squad makes her out, ALERT, the firefight
g(2.5, 'cine', 'ring', 20, exp=1.0, zoom=1.06, zoom2=1.12)
g(2.154, 'fp', 'k79_a', 0, exp=1.05)                 # Materials Intake: the mutated man, the shotgun
g(1.25, 'fp', 'l71_a', 2, exp=1.15)                     # the staff room lockers: in, as a guard turns
g(1.5, 'fp', 'l71_a', 42, exp=1.3)                      # through the holes
g(2.25, 'fp', 'l71_a', 92, exp=1.15)                    # torn open, hauled out
g(2.023, 'fp', 'k98_a', 0, exp=1.05)                 # Containment B: the handgun among the crystals
g(2.5, 'beast', 'cages', 40, exp=1.6, zoom=1.6, zoom2=1.75, cx=.52, cy=.48)
g(2.5, 'fp', 'g71_a', 14, exp=1.1)                      # a Cleanup man in a lit doorway, down
g(1.566, 'fp', 'k105_a', 14, exp=1.3)                # the Core Sample Archive in the dark: the muzzle lights it
g(2.023, 'fp', 'k106_a', 0, exp=1.15)                # Geophysics: the handgun across the lab
g(2.154, 'fp', 'k107_a', 0, exp=1.15)                # the core pit: crystals, the mutated man
g(0.457, 'fp', 'k104_a', 0, exp=1.3)                 # a flash in the prep room
g(2.023, 'fp', 'k86_a', 0, exp=1.0)                  # the test bay: the handgun
# the cursed equipment wakes: its lamp-eye in the dark, then the arcs across its casing
g(4.0, 'cine', 'mtop', 36, exp=1.0, zoom=1.3, zoom2=1.45, cx=.42, cy=.33, step=.7)
MARK['act3'] = t

# ACT 3: Containment A. Silence, a line, the slow build, the drop on the kill.
seg(96, 'black', line='SOMETHING HUNTS THESE HALLS.')
MARK['eyes_in'] = t
seg(96, 'beast', 'hall', 0, exp=1.2, zoom=1.7, zoom2=1.85, cx=.33, cy=.5)
seg(84, 'beast', 'wake', 71, exp=1.0, zoom=1.9, zoom2=2.05, cx=.58, cy=.3, step=.25)
seg(58, 'beast', 'pounce', 137, exp=1.2, zoom=1.5, zoom2=1.7, cx=.45, cy=.38, step=.4)
DROP = t
MARK['drop'] = DROP
def d(b, kind, src=None, first=0, **o):
    seg(max(1, int(round(DROP + b * EYES_BEAT)) - t), kind, src, first, **o)
d(3, 'beast', 'wake', 92, exp=1.0, zoom=1.9, zoom2=2.2, cx=.58, cy=.3, flash=[0, 1], tear=3)
d(5, 'beast', 'bite2', 149, zoom=1.6, cx=.43, cy=.38)
d(6, 'beast', 'pounce', 161, exp=1.2, zoom=1.8, cx=.45, cy=.35, shake=14, smear=True, flash=[0])
d(7, 'beast', 'bite', 199, exp=.8, zoom=1.6, cx=.64, cy=.36, hit=8, flash=[8, 9], shake=10)
d(10, 'beast', 'feedw', 259, exp=1.0, zoom=2.0, zoom2=2.2, cx=.54, cy=.45, hit=-12)
d(13, 'beast', 'feed3', 300, exp=1.0, zoom=1.7, zoom2=1.9, cx=.5, cy=.42)
d(15, 'beast', 'turn', 431, exp=.9, zoom=1.7, zoom2=1.9, cx=.6, cy=.5, smear=True)
d(18, 'beast', 'rush', 84, exp=1.15, zoom=1.5, zoom2=1.8, cx=.42, cy=.35, wipe=12)
d(19, 'beast', 'rush', 148, exp=.8, zoom=2.0, cx=.4, cy=.3, shake=22, smear=True)
d(20, 'black')
d(23, 'beast', 'hall', 100, exp=1.1, zoom=2.1, zoom2=2.3, cx=.47, cy=.3)
d(24, 'beast', 'hall', 165, exp=.9, zoom=2.0, cx=.55, cy=.3, shake=18, flash=[0])
d(25, 'beast', 'turn', 476, exp=.9, zoom=2.2, cx=.55, cy=.4, shake=26, smear=True, flash=[0, 1])
STOP = t
MARK['stop'] = STOP
seg(60, 'black')
TITLE = t
MARK['title'] = TITLE
seg(180, 'black', title=True)
ENDCARD = TITLE + 90
seg(15, 'black')
BUTTON = t
MARK['button'] = BUTTON
seg(116, 'cine', 'mlow', 24, exp=1.1, zoom=1.5, zoom2=1.75, cx=.42, cy=.3, step=.8)
seg(90, 'black', endcard=True)
END = t
MARK['end'] = END

def frame_of(f):
    for a, z, kind, src, first, o in EDIT:
        if a <= f < z: return a, z, kind, src, first, o
    return None

# ---- picture -------------------------------------------------------------------------------------------
def src_img(src, k):
    p = path(src, k)
    while not os.path.exists(p) and k > 0:
        k -= 1; p = path(src, k)
    return Image.open(p).convert('RGB')

BEAST = dict(crush=.15, contrast=1.3, sat=.4)
GAME = dict(sat=.85, crush=.02, contrast=.45, halation=.18, vignette=.3, grain=.022, weave=False, tint=(-.004, .001, .006))

def band(big, zoom, cx, cy, dx=0, dy=0):
    cw = 1920 / zoom; ch = cw / 2.20
    x0 = min(max(cx * 1920 - cw / 2 + dx, 0), 1920 - cw); y0 = min(max(cy * 1080 - ch / 2 + dy, 0), 1080 - ch)
    return big.resize((W, BAND), Image.LANCZOS, box=(x0, y0, x0 + cw, y0 + ch))

def unsub(im):
    """Paint out a radio/dialogue subtitle box (dark bar, red left edge, rows 405-427 of the 960x540 frame):
    the rows are filled from the picture just above and below it, blended, then softened."""
    a = np.asarray(im).astype(np.float32)
    red = (a[..., 0] > 140) & (a[..., 1] < 70) & (a[..., 2] < 70)
    cols = red[400:432].sum(0)
    xs = np.where(cols[:480] >= 12)[0]
    if not len(xs): return im
    x0 = max(0, int(xs[0]) - 4); x1 = min(960, 960 - int(xs[0]) + 6); y0, y1 = 401, 432
    top = a[y0 - 3:y0, x0:x1].mean(0); bot = a[y1:y1 + 3, x0:x1].mean(0)
    u = np.linspace(0, 1, y1 - y0)[:, None, None]
    a[y0:y1, x0:x1] = top[None] * (1 - u) + bot[None] * u
    out = Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))
    patch = out.crop((x0, y0 - 4, x1, y1 + 4)).filter(ImageFilter.GaussianBlur(3))
    out.paste(patch, (x0, y0 - 4))
    return out

def picture(f, seed):
    """(image, top) for frame f, or None for black."""
    hit = frame_of(f)
    if not hit or hit[2] == 'black': return None
    a, z, kind, src, first, o = hit
    rel = f - a
    k = first + int(rel * o.get('step', 1))
    im = src_img(src, k)
    if o.get('smear'): im = L.smear(im, src_img(src, max(first, k - 1)), src_img(src, max(first, k - 2)), 1.0)
    if kind == 'fp': im = unsub(im)
    big = im.resize((1920, 1080), Image.NEAREST)
    if kind == 'rep':  # the replay is not the player's view: paint out the crosshair the HUD left at the centre
        box = (930, 510, 990, 570); patch = big.crop((900, 480, 1020, 600)).filter(ImageFilter.GaussianBlur(14)).crop((30, 30, 90, 90))
        big.paste(patch, box[:2])
    u = rel / max(1, z - a - 1)
    if kind == 'fp':
        pic = L.grade(big.resize((W, H), Image.BILINEAR), exposure=o.get('exp', 1.0), seed=seed, **GAME)
        top = 0
    else:
        zoom = o.get('zoom', 1.3 if kind == 'rep' else 1.0); zoom += (o.get('zoom2', zoom) - zoom) * u
        dx = dy = 0
        if o.get('shake'): dx, dy = L.shake(rel, o['shake'], seed=a)
        pic = band(big, zoom, o.get('cx', .5), o.get('cy', (.5 if src == 'k93_b' else .41) if kind == 'rep' else .5), dx, dy)
        if kind == 'beast': pic = L.grade(pic, exposure=o.get('exp', 1.0) * 1.3, seed=seed, **BEAST)
        elif kind == 'rep': pic = L.grade(pic, exposure=o.get('exp', 1.0), seed=seed, sat=.6, crush=.04, contrast=.8, halation=.3, vignette=.6, grain=.03, weave=False)
        else: pic = L.grade(pic, exposure=o.get('exp', 1.0), seed=seed)
        top = TOP
        if 'hit' in o:
            h = rel - o['hit']
            if 0 <= h < 30: pic = L.blood_hit(pic, h, seed=o.get('hitseed', 1))
        if o.get('wipe') and rel < o['wipe']:
            prev = picture(a - 1, seed)
            if prev is not None and prev[1] == TOP: pic = L.blood_wipe(prev[0], pic, (rel + 1) / (o['wipe'] + 1), seed=a)
    if rel in o.get('flash', []): pic = L.flash(pic, .6 if rel == o['flash'][0] else .25)
    if rel < o.get('tear', 0): pic = L.tear(pic, 1.0 - rel * .3, seed + 9)
    return pic, top

def black(seed):
    rng = np.random.default_rng(seed)
    a = np.full((H, W, 3), 4.0, np.float32) + rng.normal(0, 2.2, (H, W, 1))
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8))

LOGO = look.wordmark(1080, chapter=False)
F_CREDIT = L.cap_font(38)
F_LINE = L.cap_font(64)
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

def line_on(img, text, t, n):
    """A story line in the black: typed on fast, held, dropped out with a flicker."""
    if t < 6: return img
    u = t - 6; left = n - 8 - t
    if left < 0: return img
    shown = text[:min(len(text), int(u * 2.2) + 1)]
    al = 1.0 if left > 5 else [0, .25, 0, .6, .3, .8][left]
    return text_layer(img, (W / 2, H / 2 - 30), shown, F_LINE, L.BONE, 8, al)

def overlays(img, f):
    hit = frame_of(f)
    if hit and hit[2] == 'black':
        a, z, kind, src, first, o = hit
        if o.get('credit') and 20 <= f < 84:
            t = f - 20; al = min(1, t / 10, (84 - f) / 10) * (.4 if t < 6 and t % 2 else 1)
            img = text_layer(img, (W / 2, H / 2 - 20), 'MAJWCF1234  PRESENTS', F_CREDIT, L.BONE, 10, al)
        if o.get('line'): img = line_on(img, o['line'], f - a, z - a)
        if o.get('title'):
            t = f - a
            img = logo_on(img, t, H / 2 - 70, 1 + .03 * t / 150)
            if t >= 24:
                u = t - 24; al = min(1, u / 8) * (.35 if u < 6 and u % 2 else 1)
                img = text_layer(img, (W / 2, H / 2 + 85), 'CHAPTER 2   ·   ENERGY RESEARCH', F_SUB, (222, 228, 222), 9, al)
            if t >= 54: img = L.caption(img, 'COMING SOON', None, t - 54, 10 ** 6, H / 2 + 150)
            if f >= ENDCARD:
                u = f - ENDCARD; al = min(1, u / 10)
                img = text_layer(img, (W / 2, H / 2 + 300), 'CHAPTER 1   —   FREE ON ITCH.IO', F_SMALL, (170, 176, 172), 8, al)
            if t < 2: img = L.tear(img, .8, f)
            if f >= z - 22: img = L.burn(img, (f - (z - 22)) / 22.0, seed=4)
    return img

def render(f):
    img = black(f)
    p = picture(f, f)
    if p is not None:
        pic, top = p
        img.paste(pic, (0, top))
    if 90 <= f < 104: img = Image.blend(black(f), img, (f - 90) / 14)
    if BUTTON <= f < BUTTON + 8: img = Image.blend(black(f), img, (f - BUTTON) / 8)
    return overlays(img, f)

def work(args):
    f, out = args
    render(f).save(os.path.join(out, 'f%04d.png' % f), compress_level=1)
    return f

def shot_list():
    rows = []
    for a, z, kind, src, first, o in EDIT:
        what = o.get('line') or ('credit' if o.get('credit') else 'title' if o.get('title') else 'end' if o.get('endcard') else '')
        rows.append('%6.2f-%6.2f s  %-5s %-8s %s' % (a / FPS, z / FPS, kind, src or '-', ('from %d' % first if src else what)))
    return rows

if __name__ == '__main__':
    if '--list' in sys.argv:
        print('\n'.join(shot_list())); print('END', END, END / FPS, MARK); sys.exit()
    out = sys.argv[1]; os.makedirs(out, exist_ok=True)
    only = None; jobs = 8
    for a in sys.argv[2:]:
        if a.startswith('--only='): only = [int(v) for v in a[7:].split(',') if v]
        if a.startswith('--jobs='): jobs = int(a[7:])
    frames = only or list(range(END))
    from multiprocessing import Pool
    with Pool(jobs) as p:
        for i, _ in enumerate(p.imap_unordered(work, [(f, out) for f in frames], chunksize=4)):
            if i % 400 == 0: print(i, flush=True)
    print('frames', len(frames), 'end', END / FPS, MARK)
