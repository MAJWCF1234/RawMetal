"""The Chapter 2 promo v2 treatment ("cold hold"): a new look for the trailer and stills.

Not the 1.0 grade (sodium amber, scanlines, typed terminal green). This one is a cold
bleach-bypass: silvered, desaturated steel with crushed blacks, the reds kept (blood is the only
warm thing in the frame), red-orange film halation round the lamps, heavy luminance grain, a
slight gate weave. Effects for the cuts: flash frames, an analog tear, chromatic smear on fast
motion, camera shake, a blood-spatter wipe and lens hits made from the game's own blood decal
sheets (src/assets/library/blood, greyscale, tinted here), and a film burn into black.
The wordmark and fonts come from the 1.0 look (look.py).
"""
import math, os, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

sys.path.insert(0, 'N:/rawmetal/.build/promo-1.0')
import look  # noqa: E402  (wordmark, fonts, tracked text)

BLOOD_DIR = 'N:/rawmetal/src/assets/library/blood'
RED = (150, 12, 10)
BONE = (226, 228, 226)
LUM = np.array([.2126, .7152, .0722], np.float32)

def load(path, size=(1920, 1080)):
    """An engine frame (960x540) up to `size`: 2x nearest first (the game's pixels), then smooth."""
    im = Image.open(path).convert('RGB').resize((1920, 1080), Image.NEAREST)
    if size != (1920, 1080): im = im.resize(size, Image.LANCZOS)
    return im

def crop(im, zoom=1.0, cx=.5, cy=.5, dx=0., dy=0., size=None):
    """Punch in: zoom about (cx, cy) in 0..1, offset by (dx, dy) pixels of the output (shake)."""
    W, H = size or im.size
    cw, ch = im.width / zoom, im.height / zoom
    x0 = min(max(cx * im.width - cw / 2 + dx / W * cw, 0), im.width - cw)
    y0 = min(max(cy * im.height - ch / 2 + dy / H * ch, 0), im.height - ch)
    return im.resize((W, H), Image.LANCZOS, box=(x0, y0, x0 + cw, y0 + ch))

_V = {}
def _vig(h, w, s):
    k = (h, w, s)
    if k not in _V:
        y, x = np.mgrid[0:h, 0:w].astype(np.float32)
        nx, ny = (x / w - .5) * 2, (y / h - .5) * 2
        r = np.sqrt(nx * nx * .7 + ny * ny)
        _V[k] = np.clip(1 - s * np.clip(r - .3, 0, None) ** 1.8, 0, 1)[..., None]
    return _V[k]

def grade(im, exposure=1.0, sat=.42, keep_red=1.0, crush=.06, contrast=1.0, halation=.5, vignette=.9,
          grain=.05, seed=0, weave=True, tint=(-.012, .004, .02), lift=0.0):
    a = np.asarray(im).astype(np.float32) / 255.0
    h, w, _ = a.shape
    a = a * exposure
    # halation: the hot parts bleed a red-orange glow into the dark, as on film stock
    if halation > 0:
        small = Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8)).resize((w // 6, h // 6), Image.BILINEAR)
        s = np.asarray(small).astype(np.float32) / 255.0
        hi = np.clip(((s * LUM).sum(2, keepdims=True) - .5) / .5, 0, 1) ** 2
        g = Image.fromarray((hi[..., 0] * 255).astype(np.uint8))
        hb = (np.asarray(g.filter(ImageFilter.GaussianBlur(4))).astype(np.float32) +
              np.asarray(g.filter(ImageFilter.GaussianBlur(14))).astype(np.float32)) / 510.0
        hb = np.asarray(Image.fromarray((np.clip(hb, 0, 1) * 255).astype(np.uint8)).resize((w, h), Image.BILINEAR)).astype(np.float32) / 255.0
        a = a + halation * hb[..., None] * np.array([1.0, .38, .16], np.float32)
    # bleach bypass: desaturate, but keep the reds (blood) where they are strongly red
    lum = (a * LUM).sum(2, keepdims=True)
    red = np.clip((a[..., :1] - np.maximum(a[..., 1:2], a[..., 2:3]) * 1.15) * 5, 0, 1) * keep_red
    s = sat + (1.15 - sat) * red
    a = lum + (a - lum) * s
    # silver: blend toward the luminance overlay (the skipped bleach) and crush
    a = a * .75 + (lum * 1.08) * .25
    a = np.clip((a - crush) / (1 - crush), 0, None) + lift
    a = a / (1 + a * .25) * 1.25
    a = np.clip(a, 0, 1)
    a = (a * a * (3 - 2 * a)) * (.6 * contrast) + a * (1 - .6 * contrast)
    # cold steel: blue-cyan in the shadows and mids, highlights left near neutral
    lum = (a * LUM).sum(2, keepdims=True)
    a = a + np.array(tint, np.float32) * np.clip(1.2 - lum * 1.4, 0, 1) * 1.6
    a = np.clip(a, 0, 1) * _vig(h, w, vignette)
    rng = np.random.default_rng(seed)
    if grain:
        n = rng.normal(0, grain, (h // 2 + 1, w // 2 + 1)).astype(np.float32)
        n = np.asarray(Image.fromarray(((n + .5) * 255).clip(0, 255).astype(np.uint8)).resize((w + 2, h + 2), Image.BILINEAR))[:h, :w].astype(np.float32) / 255 - .5
        a = a + n[..., None] * (.25 + .75 * np.clip(1 - lum * 1.2, 0, 1))
    out = Image.fromarray((np.clip(a, 0, 1) * 255 + .5).astype(np.uint8))
    if weave:  # gate weave: the frame never quite settles
        dy = int(round(rng.normal(0, .6))); dx = int(round(rng.normal(0, .35)))
        if dx or dy: out = out.transform(out.size, Image.AFFINE, (1, 0, dx, 0, 1, dy), Image.BILINEAR)
    return out

def flash(im, amount):
    """An exposure flash: the frame blown toward white-hot for a hit."""
    a = np.asarray(im).astype(np.float32) / 255.0
    a = 1 - (1 - a) * (1 - amount)
    a = a * (1 + amount * np.array([0, -.04, -.1], np.float32))
    return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))

def tear(im, strength, seed):
    """An analog tear: horizontal bands slipped sideways, the channels split in them, a dropout line."""
    rng = np.random.default_rng(seed)
    a = np.asarray(im).copy(); h, w, _ = a.shape
    y = 0
    while y < h:
        bh = int(rng.integers(6, 70))
        if rng.random() < .45 * strength:
            off = int(rng.normal(0, 60 * strength))
            band = a[y:y + bh]
            band = np.roll(band, off, axis=1)
            band[..., 0] = np.roll(band[..., 0], int(12 * strength), axis=1)
            band[..., 2] = np.roll(band[..., 2], -int(12 * strength), axis=1)
            a[y:y + bh] = band
        y += bh
    for _ in range(int(3 * strength)):
        r = int(rng.integers(0, h)); a[r:r + 2] = np.clip(a[r:r + 2].astype(int) + 120, 0, 255)
    return Image.fromarray(a)

def smear(cur, prev, prev2, amount=1.0):
    """Chromatic smear on fast motion: red is now, green a frame behind, blue two."""
    c, p, q = (np.asarray(x).astype(np.float32) for x in (cur, prev, prev2))
    out = c.copy()
    out[..., 1] = c[..., 1] * (1 - .55 * amount) + p[..., 1] * .55 * amount
    out[..., 2] = c[..., 2] * (1 - .7 * amount) + q[..., 2] * .7 * amount
    return Image.fromarray(np.clip(out, 0, 255).astype(np.uint8))

def shake(k, amp, seed=3):
    """Hand-held hit: decaying offsets (px) for frame k after the impact."""
    rng = np.random.default_rng(seed + k)
    d = amp * math.exp(-k / 6.0)
    return rng.normal(0, d), rng.normal(0, d * .7)

# ---- blood: the game's decal sheets, greyscale cells, tinted here ------------------------------
_CELLS = None
def cells():
    global _CELLS
    if _CELLS is None:
        _CELLS = []
        for fn, n in (('blood-burst.png', 8), ('blood-splat.png', 8), ('blood-pool.png', 6), ('blood-streak.png', 6), ('blood-drops.png', 6)):
            sh = Image.open(os.path.join(BLOOD_DIR, fn)).convert('RGBA')
            cw = sh.width // n
            for i in range(n):
                c = sh.crop((i * cw, 0, (i + 1) * cw, sh.height))
                al = c.getchannel('A')
                if al.getbbox(): _CELLS.append((fn, al.crop(al.getbbox())))
    return _CELLS

def splat_matte(size, seed, count=7, scale=(2.5, 5.0), kinds=('blood-burst.png', 'blood-splat.png', 'blood-pool.png')):
    """A greyscale matte of big splats thrown at the lens."""
    rng = np.random.default_rng(seed)
    pool = [c for c in cells() if c[0] in kinds]
    m = Image.new('L', size, 0)
    for _ in range(count):
        fn, al = pool[int(rng.integers(len(pool)))]
        s = rng.uniform(*scale) * size[0] / 960
        c = al.resize((max(1, int(al.width * s)), max(1, int(al.height * s))), Image.BILINEAR).rotate(rng.uniform(0, 360), expand=True)
        x = int(rng.uniform(-.1, .9) * size[0]); y = int(rng.uniform(-.15, .8) * size[1])
        m.paste(Image.new('L', c.size, 255), (x, y), c)
    return m

def blood_hit(im, k, seed, strength=1.0):
    """Blood on the lens: k frames after the hit. Spray and splats from the decal sheets, dark and
    wet (thick centres near black, thin red edges, a cold glint), sliding down and thinning."""
    W, H = im.size
    m = splat_matte((W, H), seed, count=9, scale=(1.4, 3.2), kinds=('blood-burst.png', 'blood-splat.png', 'blood-drops.png'))
    m = m.transform(m.size, Image.AFFINE, (1, 0, 0, 0, 1, -k * 1.6), Image.BILINEAR)  # runs down
    fade = max(0., 1 - k / 30.) ** .6 * strength
    al = np.asarray(m).astype(np.float32) / 255
    thick = np.asarray(m.filter(ImageFilter.GaussianBlur(6))).astype(np.float32) / 255
    glint = np.clip(np.asarray(m.filter(ImageFilter.GaussianBlur(2))).astype(np.float32) / 255 -
                    np.asarray(m.transform(m.size, Image.AFFINE, (1, 0, 2, 0, 1, 2), Image.BILINEAR).filter(ImageFilter.GaussianBlur(2))).astype(np.float32) / 255, 0, 1)
    a = np.asarray(im).astype(np.float32) / 255
    col = np.array([.42, .02, .02], np.float32) * (1.1 - .85 * thick[..., None])
    cover = (al * (.55 + .4 * thick) * fade)[..., None]
    a = a * (1 - cover) + col * cover + glint[..., None] * fade * .35
    return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))

def blood_wipe(a_img, b_img, u, seed):
    """A red-black spatter wipe from A to B: splats grow over A, red at the leading edge, B under them."""
    W, H = a_img.size
    rng = np.random.default_rng(seed)
    m = Image.new('L', (W, H), 0)
    pool = cells()
    n = 26
    for i in range(n):
        fn, al = pool[int(rng.integers(len(pool)))]
        x = rng.uniform(-.1, 1.0) * W; y = rng.uniform(-.1, .95) * H
        born = i / n * .55
        g = np.clip((u - born) / .45, 0, 1)
        if g <= 0: continue
        s = (1.0 + 7.0 * g) * W / 960
        c = al.resize((max(1, int(al.width * s)), max(1, int(al.height * s))), Image.BILINEAR)
        m.paste(Image.new('L', c.size, 255), (int(x - c.width / 2), int(y - c.height / 2)), c)
    mm = np.asarray(m).astype(np.float32) / 255
    if u > .85: mm = np.maximum(mm, (u - .85) / .15)
    edge = np.clip(np.asarray(m.filter(ImageFilter.GaussianBlur(4))).astype(np.float32) / 255 - mm, 0, 1) * 2
    A = np.asarray(a_img).astype(np.float32) / 255; B = np.asarray(b_img).astype(np.float32) / 255
    under = np.array([.10, .0, .005], np.float32)
    rise = float(np.clip((u - .45) / .5, 0, 1))
    fill = under * (1 - rise) + B * rise
    out = A * (1 - mm[..., None]) + fill * mm[..., None]
    edge = np.clip(edge, 0, 1)
    out = out * (1 - edge[..., None] * .85) + np.array([.36, .01, .01], np.float32) * edge[..., None] * .85
    inner = np.clip(mm - np.asarray(m.filter(ImageFilter.MinFilter(9))).astype(np.float32) / 255, 0, 1)
    out = out * (1 - inner[..., None] * .7)
    return Image.fromarray((np.clip(out, 0, 1) * 255).astype(np.uint8))

def burn(im, u, seed=5):
    """Film burn into black: a hot orange-white bloom eating in from one edge, then nothing."""
    W, H = im.size
    y, x = np.mgrid[0:H, 0:W].astype(np.float32)
    rng = np.random.default_rng(seed)
    cx, cy = W * 1.05, H * rng.uniform(.3, .7)
    d = np.sqrt(((x - cx) / W) ** 2 + ((y - cy) / H * .8) ** 2)
    noise = np.asarray(Image.fromarray((rng.random((H // 135 + 2, W // 135 + 2)) * 255).astype(np.uint8)).resize((W, H), Image.BICUBIC).filter(ImageFilter.GaussianBlur(25))).astype(np.float32) / 255
    front = u * 1.6
    hot = np.clip(1 - (d + noise * .25 - front + .35) / .35, 0, 1)
    gone = np.clip((front - d - noise * .25 - .1) / .2, 0, 1)
    a = np.asarray(im).astype(np.float32) / 255
    col = np.stack([np.ones_like(hot), .55 + .4 * hot, .2 + .6 * hot ** 3], -1)
    a = a * (1 - hot[..., None]) + col * hot[..., None]
    a = a * (1 - gone[..., None])
    return Image.fromarray((np.clip(a, 0, 1) * 255).astype(np.uint8))

# ---- captions: an incident-report voice ---------------------------------------------------------
def cap_font(size): return look.font(size, b'SemiLight Condensed')
def tag_font(size): return look.mono(size)

def caption(img, text, tag, t, dur, y, scale=1.0):
    """A line set like a stamped report: a small red tag (the file), then the line revealed from
    under a black redaction bar that slides off; it drops out with a two-frame flicker."""
    if t < 0 or t > dur: return img
    W = img.width
    f = cap_font(int(69 * scale)); ft = tag_font(int(27 * scale))
    tr = int(7 * scale)
    tw = look.text_width(text, f, tr)
    x0 = W / 2 - tw / 2
    layer = Image.new('RGBA', img.size, (0, 0, 0, 0)); d = ImageDraw.Draw(layer)
    # soft dark halo for legibility
    halo = Image.new('RGBA', img.size, (0, 0, 0, 0))
    look.draw_tracked(ImageDraw.Draw(halo), (x0, y), text, f, (0, 0, 0, 230), tr)
    halo = halo.filter(ImageFilter.MaxFilter(9)).filter(ImageFilter.GaussianBlur(int(18 * scale)))
    layer.alpha_composite(halo)
    if tag:
        tagw = look.text_width(tag, ft, int(4 * scale))
        tx = W / 2 - tagw / 2; ty = y - int(44 * scale)
        d.rectangle([tx - 10 * scale, ty - 4 * scale, tx + tagw + 10 * scale, ty + 29 * scale], fill=RED + (230,))
        look.draw_tracked(d, (tx, ty), tag, ft, (240, 226, 222, 255), int(4 * scale))
    look.draw_tracked(d, (x0, y), text, f, BONE + (255,), tr)
    # redaction bar sliding off to the right over the first 10 frames
    u = min(1.0, t / 10.0)
    if u < 1:
        bx = x0 - 10 * scale + (tw + 20 * scale) * (u ** .6)
        d.rectangle([bx, y - 6 * scale, x0 + tw + 14 * scale, y + 80 * scale], fill=(6, 6, 7, 255))
    al = 1.0
    left = dur - t
    if left < 6: al = [0, .2, 0, .6, .3, .8][int(left)]
    if t < 3 and t % 2: al *= .5
    if al < 1: layer.putalpha(layer.getchannel('A').point(lambda v: int(v * al)))
    out = img.convert('RGBA'); out.alpha_composite(layer)
    return out.convert('RGB')
