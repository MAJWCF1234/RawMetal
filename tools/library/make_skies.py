#!/usr/bin/env python3
"""More skies for MAP.skybox: five from the purchased packs, five made here.

Every sky ships the way the Brutal panoramas do (register_skyboxes.py): an
equirectangular panorama brought to 2048x512 and stored as a neural x2 packet
(NTX1, texture-x2 model, resource 348) the engine reconstructs at load.

  Purchased (O:/retro official). The packets and the PNG masters go to
  src/assets/library/skies/, which is gitignored: the repository is public.
  A worktree without its own library writes the main checkout's.
  Made here (numpy, seeded). The packets go to src/assets/ai/skies/, tracked.

The catalog rows (SkyboxCatalog.h), the assets.rc block (resources 2900+) and
each sky's own air (haze colour and density, used when the MAP leaves its haze
empty) come from EXTRA_SKIES in register_skyboxes.py, the one list.

  python tools/library/make_skies.py            make every sky, write the rows
  python tools/library/make_skies.py --check    verify packets and rows, write nothing
  python tools/library/make_skies.py --preview DIR   also save each panorama as a JPEG
"""
from __future__ import annotations

import argparse
import io
import struct
import subprocess
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
import register_skyboxes as reg  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
PACKS = Path(r"O:\retro official")
EMBED = (2048, 512)
MASTER = (2048, 1024)  # equirectangular 2:1: latitude +90 at the top, -90 at the bottom

# ---- the purchased panoramas -------------------------------------------------------------------


def read_hdr(path: Path) -> np.ndarray:
    import cv2
    a = cv2.imread(str(path), cv2.IMREAD_ANYDEPTH | cv2.IMREAD_COLOR)
    if a is None:
        raise SystemExit(f"cannot read {path}")
    return a[:, :, ::-1].astype(np.float32)


def srgb(linear: np.ndarray) -> np.ndarray:
    x = np.clip(linear, 0, 1)
    return np.where(x <= .0031308, 12.92 * x, 1.055 * np.power(x, 1 / 2.4) - .055)


def opaque(path: Path) -> np.ndarray:
    im = Image.open(path).convert("RGBA")
    a = np.asarray(im).astype(np.float32) / 255
    rgb, alpha = a[:, :, :3], a[:, :, 3:]
    if alpha.min() < 1:  # a transparent part of a pack sky: lay it on the sky's own average
        back = (rgb * alpha).sum((0, 1)) / max(1e-6, alpha.sum())
        rgb = rgb * alpha + back * (1 - alpha)
    return rgb


def resized(rgb: np.ndarray, size) -> np.ndarray:
    im = Image.fromarray((np.clip(rgb, 0, 1) * 255 + .5).astype(np.uint8))
    return np.asarray(im.resize(size, Image.Resampling.LANCZOS)).astype(np.float32) / 255


def pack_day():
    hdr = read_hdr(PACKS / "Textures/PSX_Daysky_HDRI/DAYSKY.hdr")
    return srgb(hdr * 1.15)


def pack_night():
    hdr = read_hdr(PACKS / "Textures/PSX_Nightsky_HDRI/PSX_NIGHTSKY.hdr")
    mapped = hdr * 1.6 / (1 + hdr * 1.6)  # Reinhard: the moon keeps its face, the stars stay points
    return srgb(mapped)


def pack_dusk():
    return opaque(PACKS / "Environments/Bus_stop/Skybox/Skybox.png")


def pack_cloudy_night():
    rgb = opaque(PACKS / "Environments/DINER/Skybox/Skybox.png")
    return np.clip(rgb * 1.7, 0, 1) ** .9  # the pack's own exposure is for a lit diner; lift the clouds


def pack_treeline():
    return opaque(PACKS / "Environments/6twelve/Skybox/pano.png")


# ---- the panoramas made here --------------------------------------------------------------------


def directions(size=MASTER):
    w, h = size
    u = (np.arange(w) + .5) / w
    v = (np.arange(h) + .5) / h
    lon = (u - .5) * 2 * np.pi
    lat = np.pi * .5 - v * np.pi
    lon, lat = np.meshgrid(lon, lat)
    ring = np.cos(lat)
    return np.stack([ring * np.cos(lon), ring * np.sin(lon), np.sin(lat)], -1), lat, lon


def value_noise(x, y, seed):
    xi, yi = np.floor(x).astype(np.int64), np.floor(y).astype(np.int64)
    fx, fy = x - xi, y - yi
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)

    def h(i, j):
        n = (i * 374761393 + j * 668265263 + seed * 2147483647) & 0xFFFFFFFF
        n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
        return ((n ^ (n >> 16)) & 0xFFFF) / 65535.

    a, b, c, d = h(xi, yi), h(xi + 1, yi), h(xi, yi + 1), h(xi + 1, yi + 1)
    return (a * (1 - fx) + b * fx) * (1 - fy) + (c * (1 - fx) + d * fx) * fy


def fbm(x, y, seed, octaves=6, gain=.5):
    total, amp, norm = 0., 1., 0.
    for o in range(octaves):
        total = total + amp * value_noise(x, y, seed + o * 97)
        norm += amp
        x, y, amp = x * 2.03 + 17.1, y * 2.03 - 5.3, amp * gain
    return total / norm


def cloud_plane(d, height=1.):
    """Where a view direction meets a flat cloud deck: the clouds shrink to the horizon."""
    z = np.maximum(d[..., 2], .02)
    return d[..., 0] / z * height, d[..., 1] / z * height, z


def mix(a, b, t):
    t = np.asarray(t)[..., None]
    return np.asarray(a) * (1 - t) + np.asarray(b) * t


def smooth(a, b, x):
    t = np.clip((x - a) / (b - a), 0, 1)
    return t * t * (3 - 2 * t)


def sun_dir(azimuth_deg, elevation_deg):
    az, el = np.radians(azimuth_deg), np.radians(elevation_deg)
    return np.array([np.cos(el) * np.cos(az), np.cos(el) * np.sin(az), np.sin(el)])


def ground(d, lat, colour, horizon):
    """Below the horizon: the distant land under haze, under the terrain the engine draws."""
    t = smooth(0, -.35, lat)
    return mix(horizon, colour, t)


def make_overcast():
    d, lat, lon = directions()
    up = np.clip(d[..., 2], 0, 1)
    sky = mix([.70, .72, .74], [.50, .53, .57], up ** .7)
    px, py, z = cloud_plane(d, 1.)
    deck = fbm(px * 1.6, py * 1.6, 11)
    lumps = fbm(px * 5, py * 5, 23, 5)
    shade = .82 + .3 * (deck - .5) + .14 * (lumps - .5)
    sky = sky * shade[..., None]
    sky = mix(sky, [.72, .73, .74], smooth(.25, 0, d[..., 2]))  # the low cloud melts into haze
    return np.where((lat < 0)[..., None], ground(d, lat, [.36, .38, .38], [.70, .71, .72]), sky)


def make_dawn():
    d, lat, lon = directions()
    s = sun_dir(95, 2.5)
    up = np.clip(d[..., 2], 0, 1)
    toward = np.clip(d @ s, -1, 1)
    sky = mix([.95, .62, .46], [.20, .28, .50], smooth(0, .55, up))
    sky = mix(sky, [.09, .12, .27], smooth(.45, 1, up))
    glow = np.exp((toward - 1) * 7) * (1 - smooth(0, .5, up))
    sky = sky + np.array([1., .55, .25]) * glow[..., None] * .55
    sky = sky + np.array([1., .9, .7]) * (np.exp((toward - 1) * 900) * 1.5)[..., None]   # the sun's disc
    px, py, z = cloud_plane(d, 1.)
    streak = fbm(px * .9, py * 6, 41, 5)                                              # cirrus drawn out by wind
    cirrus = smooth(.55, .78, streak) * smooth(.02, .2, up) * (1 - smooth(.7, 1, up))
    lit = mix([.55, .38, .52], [1., .66, .55], np.clip(toward * .5 + .5, 0, 1) ** 2)
    sky = mix(sky, lit, cirrus * .7)
    return np.where((lat < 0)[..., None], ground(d, lat, [.14, .12, .15], [.80, .58, .50]), sky)


def make_storm():
    d, lat, lon = directions()
    up = np.clip(d[..., 2], 0, 1)
    px, py, z = cloud_plane(d, 1.)
    warp = fbm(px * .7, py * .7, 61, 4)
    body = fbm(px * 1.8 + warp * 2.5, py * 1.8 - warp * 2.5, 67, 7)
    roll = fbm(px * 4 + warp, py * 4, 71, 5)
    sky = mix([.36, .39, .42], [.15, .17, .20], smooth(.0, .35, up))
    dark = smooth(.35, .7, body)
    sky = sky * (1.15 - .55 * dark - .2 * (roll - .5))[..., None]
    gap = np.exp(-(d[..., 2] / .07) ** 2)                                             # pale light under the front
    sky = mix(sky, [.48, .51, .52], gap * .55)
    flash = np.exp(-((lon - 1.9) ** 2 / .05 + (lat - .22) ** 2 / .01)) * smooth(.4, .75, body)   # far lightning in the cloud
    sky = sky + np.array([.55, .6, .8]) * flash[..., None] * .8
    return np.where((lat < 0)[..., None], ground(d, lat, [.10, .11, .12], [.40, .43, .44]), sky)


def stars(d, seed, density=.0016, size=2048):
    w, h = MASTER
    rng = np.random.default_rng(seed)
    field = np.zeros((h, w))
    n = int(w * h * density)
    ys = (np.arccos(rng.uniform(-1, 1, n)) / np.pi * h).astype(int).clip(0, h - 1)   # even over the sphere
    xs = rng.integers(0, w, n)
    field[ys, xs] = rng.uniform(.15, 1, n) ** 3
    blur = field + .35 * (np.roll(field, 1, 0) + np.roll(field, -1, 0) + np.roll(field, 1, 1) + np.roll(field, -1, 1))
    return blur


def make_ember_eclipse():
    d, lat, lon = directions()
    up = np.clip(d[..., 2], 0, 1)
    s = sun_dir(70, 24)
    toward = np.clip(d @ s, -1, 1)
    angle = np.degrees(np.arccos(toward))
    sky = mix([.42, .16, .08], [.10, .04, .04], smooth(0, .6, up))
    px, py, z = cloud_plane(d, 1.)
    ash = fbm(px * 1.2, py * 1.2, 81, 6)
    sky = sky * (.8 + .45 * (ash - .5))[..., None]
    corona = np.exp(-((angle - 2.4) / .35) ** 2) * 1.4 + np.exp(-np.maximum(angle - 2.2, 0) / 2.5) * .35
    sky = sky + np.array([1., .45, .15]) * corona[..., None] * (angle > 2.15)[..., None]
    sky = np.where((angle < 2.15)[..., None], np.array([.015, .01, .01]), sky)        # the black disc
    sky = sky + np.array([.6, .18, .05]) * (smooth(.12, 0, d[..., 2]) * smooth(-.05, 0, d[..., 2]))[..., None] * .5
    return np.where((lat < 0)[..., None], ground(d, lat, [.06, .03, .03], [.38, .16, .09]), sky)


def make_resonance_aurora():
    d, lat, lon = directions()
    up = np.clip(d[..., 2], 0, 1)
    sky = mix([.07, .06, .10], [.01, .01, .025], smooth(0, .5, up))
    sky = sky + stars(d, 7)[..., None] * np.array([.85, .85, 1.])
    # Curtains: a bright lower hem that wavers along the sky, the light thinning upward from it in rays.
    hem = .30 + .22 * (fbm(lon * 1.1 + 3, np.zeros_like(lon) + .5, 91, 4) - .5)
    above = lat - hem
    hang = np.where(above > 0, np.exp(-above / .2), np.exp(-(above / .025) ** 2))
    rays = .45 + .55 * smooth(.3, .75, fbm(lon * 9, lat * .6, 97, 4))
    curtain = hang * rays * smooth(.03, .2, up)
    hem2 = .62 + .16 * (fbm(lon * 1.6 + 9, np.zeros_like(lon), 93, 3) - .5)
    above2 = lat - hem2
    second = np.where(above2 > 0, np.exp(-above2 / .12), np.exp(-(above2 / .02) ** 2)) * smooth(.35, .8, fbm(lon * 7 + 4, lat * .5, 99, 4)) * .55
    top = np.clip(above / .35, 0, 1)[..., None]
    glow = (np.array([.62, .25, 1.]) * (1 - top) + np.array([.95, .3, .75]) * top) * curtain[..., None] + np.array([.9, .3, .8]) * second[..., None]
    sky = sky + glow * .8
    sky = sky + np.array([.22, .10, .30]) * (smooth(.15, 0, d[..., 2]) * smooth(-.05, 0, d[..., 2]))[..., None] * .5
    return np.where((lat < 0)[..., None], ground(d, lat, [.02, .02, .03], [.12, .08, .16]), sky)


MAKERS = {
    "psx_clear_day": pack_day, "psx_starry_night": pack_night, "dusk_afterglow": pack_dusk,
    "cloudy_night": pack_cloudy_night, "treeline_night": pack_treeline,
    "grey_overcast": make_overcast, "cold_dawn": make_dawn, "storm_front": make_storm,
    "ember_eclipse": make_ember_eclipse, "resonance_aurora": make_resonance_aurora,
}

# ---- packets ------------------------------------------------------------------------------------


def packet(rgb: np.ndarray) -> bytes:
    """The Brutal packet format: header, then the half-size panorama as lossless WebP."""
    image = Image.fromarray((np.clip(rgb, 0, 1) * 255 + .5).astype(np.uint8))
    if image.size != EMBED:
        image = image.resize(EMBED, Image.Resampling.LANCZOS)
    w, h = image.size
    f = reg.NEURAL_FACTOR
    low = image.resize(((w + f - 1) // f, (h + f - 1) // f), Image.Resampling.BICUBIC)
    stream = io.BytesIO()
    low.save(stream, format="WEBP", lossless=True, quality=100, method=6, exact=True)
    return struct.pack("<4s7I", b"NTX1", w, h, low.size[0], low.size[1], reg.NEURAL_MODEL_RESOURCE, f, 0) + stream.getvalue()


def library_root() -> Path:
    own = ROOT / "src/assets/library"
    if own.exists():
        return own
    common = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--git-common-dir"], capture_output=True, text=True).stdout.strip()
    main = (ROOT / common).resolve().parent if common else ROOT
    return main / "src/assets/library"


def destination(sky) -> Path:
    rel = sky["ntx"]
    if rel.startswith("assets/library/"):
        return library_root() / rel[len("assets/library/"):]
    return ROOT / "src" / rel


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--preview", type=Path)
    parser.add_argument("--only", nargs="*")
    args = parser.parse_args()
    problems = []
    for sky in reg.EXTRA_SKIES:
        name = sky["id"]
        path = destination(sky)
        if args.only and name not in args.only:
            continue
        if args.check:
            if not path.exists() or path.read_bytes()[:4] != b"NTX1":
                problems.append(f"{name}: no packet at {path}")
            continue
        rgb = MAKERS[name]()
        if rgb.shape[:2] != (MASTER[1], MASTER[0]):
            rgb = resized(rgb, MASTER)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(packet(rgb))
        if sky["ntx"].startswith("assets/library/"):  # keep the purchased master beside its packet
            Image.fromarray((np.clip(rgb, 0, 1) * 255 + .5).astype(np.uint8)).save(path.with_name(name + ".png"))
        if args.preview:
            args.preview.mkdir(parents=True, exist_ok=True)
            Image.fromarray((np.clip(rgb, 0, 1) * 255 + .5).astype(np.uint8)).resize((1024, 512)).save(args.preview / f"{name}.jpg", quality=90)
        print(f"{name}: {path.stat().st_size} bytes -> {path}")
    for target, text in ((ROOT / "src/world/SkyboxCatalog.h", reg.catalog_text()),
                         (ROOT / "src/assets.rc", reg.extra_resource_text((ROOT / "src/assets.rc").read_text(encoding="utf-8")))):
        try:
            reg.write_text(target, text, args.check)
        except ValueError as error:
            problems.append(str(error))
    for p in problems:
        print("FAIL", p)
    print(("FAIL: %d problems" % len(problems)) if problems else ("PASS" if args.check else "done") + f": {len(reg.EXTRA_SKIES)} extra skies")
    sys.exit(1 if problems else 0)


if __name__ == "__main__":
    main()
