"""Old against new prop physics, side by side, from a lab run of the `rigid` and `rigid-legacy` tubes.

  python tools/dev/lab.py run --feature=rigid,rigid-legacy --out=.build/lab-out-rigid
  python tools/dev/rigid_sheet.py [.build/lab-out-rigid]

Writes OUT/rigid-ab-<scene>.png for each scene (rows: the shots a-start .. d-end; left the old physics,
right the rigid core) and OUT/rigid-ab-sheet.png with every scene.
"""
import os, sys
from PIL import Image, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SCENES = ['stack', 'ramp', 'chairs', 'door']
SHOTS = ['a-start', 'b-early', 'c-mid', 'd-end']

def main():
    out = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, '.build', 'lab-out-rigid'))
    pages = []
    for scene in SCENES:
        rows = []
        for shot in SHOTS:
            pair = []
            for feature in ('rigid-legacy', 'rigid'):
                path = os.path.join(out, 'jobs', '%s@rigid-%s' % (feature, scene), shot + '.png')
                pair.append(Image.open(path).convert('RGB') if os.path.exists(path) else None)
            if all(p is None for p in pair):
                continue
            w, h = next(p for p in pair if p).size
            row = Image.new('RGB', (w * 2 + 6, h), (20, 20, 20))
            for i, p in enumerate(pair):
                if p: row.paste(p, (i * (w + 6), 0))
            d = ImageDraw.Draw(row)
            d.text((6, 4), 'OLD  %s  %s' % (scene, shot), fill=(255, 220, 120))
            d.text((w + 12, 4), 'NEW (rigid core)  %s  %s' % (scene, shot), fill=(140, 255, 160))
            rows.append(row)
        if not rows:
            print('no frames for', scene); continue
        page = Image.new('RGB', (rows[0].width, sum(r.height + 4 for r in rows)), (0, 0, 0))
        y = 0
        for r in rows:
            page.paste(r, (0, y)); y += r.height + 4
        path = os.path.join(out, 'rigid-ab-%s.png' % scene)
        page.save(path); pages.append(page); print(path)
    if pages:
        sheet = Image.new('RGB', (max(p.width for p in pages), sum(p.height + 12 for p in pages)), (0, 0, 0))
        y = 0
        for p in pages:
            sheet.paste(p, (0, y)); y += p.height + 12
        path = os.path.join(out, 'rigid-ab-sheet.png'); sheet.save(path); print(path)

if __name__ == '__main__':
    main()
