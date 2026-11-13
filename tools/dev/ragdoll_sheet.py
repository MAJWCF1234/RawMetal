"""Before/after sheet of the ragdoll-fall strips: for each scenario a row of the v1.4.2 fall and a row of
the fixed one, from the side (or the front), every Nth frame of the 30 fps strip.

python tools/dev/ragdoll_sheet.py BEFORE_OUT AFTER_OUT SHEET.png [--view side|front] [--every 3] [--tiles 25]
(BEFORE_OUT / AFTER_OUT: lab --out folders that ran --feature=ragdoll-fall)
"""
import argparse, json, os, sys
from PIL import Image, ImageDraw

TILE_W, TILE_H, COLS, TOP = 240, 135, 15, 14


def tiles(path, every, count):
    strip = Image.open(path).convert("RGB")
    rows = (strip.height - TOP) // TILE_H
    out = []
    for i in range(0, rows * COLS, every):
        x, y = (i % COLS) * TILE_W, TOP + (i // COLS) * TILE_H
        out.append(strip.crop((x, y, x + TILE_W, y + TILE_H)))
        if len(out) == count:
            break
    return out


def metrics(out_dir, tube):
    try:
        data = json.load(open(os.path.join(out_dir, "metrics.json")))
    except OSError:
        return {}
    for t in data["tubes"]:
        if t["id"] == tube:
            return {k.split(".", 1)[1]: v for k, v in t["metrics"].items() if k.startswith("ragdoll-fall.")}
    return {}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("before"); ap.add_argument("after"); ap.add_argument("sheet")
    ap.add_argument("--view", default="side"); ap.add_argument("--every", type=int, default=3); ap.add_argument("--tiles", type=int, default=25)
    ap.add_argument("--scale", type=float, default=0.5)
    a = ap.parse_args()
    jobs = sorted(d for d in os.listdir(os.path.join(a.after, "jobs")) if d.startswith("ragdoll-fall@"))
    tw, th = int(TILE_W * a.scale), int(TILE_H * a.scale)
    label_h = 16
    rows = []
    for job in jobs:
        for name, root in (("v1.4.2", a.before), ("fixed", a.after)):
            p = os.path.join(root, "jobs", job, f"strip-{a.view}.png")
            if not os.path.exists(p):
                continue
            m = metrics(root, job)
            info = f"{job.split('@')[1]}  {name}"
            for key, unit in (("shape_core_cm", "cm shape"), ("settle_s", "s rest"), ("inside_rest", " in solid"), ("lowest_rest_m", "m lowest")):
                if key in m:
                    info += f"  {m[key]:.2f}{unit}" if isinstance(m[key], float) else f"  {m[key]}{unit}"
            rows.append((info, [t.resize((tw, th)) for t in tiles(p, a.every, a.tiles)]))
    width = tw * a.tiles
    sheet = Image.new("RGB", (width, len(rows) * (th + label_h) + 20), (16, 16, 16))
    d = ImageDraw.Draw(sheet)
    d.text((4, 4), f"ragdoll falls, {a.view} view, every {a.every} frames of 30 fps ({a.every / 30:.2f} s a tile): v1.4.2 above, fixed below", fill=(255, 255, 255))
    y = 20
    for info, ts in rows:
        d.text((4, y + 2), info, fill=(255, 220, 90) if "fixed" in info else (200, 200, 200))
        for i, t in enumerate(ts):
            sheet.paste(t, (i * tw, y + label_h))
        y += th + label_h
    os.makedirs(os.path.dirname(os.path.abspath(a.sheet)), exist_ok=True)
    sheet.save(a.sheet)
    print(a.sheet, sheet.size)


if __name__ == "__main__":
    sys.exit(main())
