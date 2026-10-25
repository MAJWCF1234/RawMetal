"""Render an entry view and a reverse view of each chunk into one contact sheet.

  python tools/dev/overview.py EXE OUTDIR FIRST LAST [--scale 0.4]

Views come from the chunk table (tools/campaign/plan.py): one at the spawn
looking in, one from the room's far side looking back toward the entry.
"""
import math, os, subprocess, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'campaign'))
import plan
from PIL import Image, ImageDraw

exe, out, first, last = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
scale = float(sys.argv[sys.argv.index('--scale') + 1]) if '--scale' in sys.argv else .4
os.makedirs(os.path.join(out, '.build', 'shot'), exist_ok=True)
lines, names = [], []
for c in plan.CHUNKS:
    if not first <= c['idx'] <= last: continue
    (px, py), z, yaw = plan.spawn(c)
    deg = math.degrees(yaw)
    fx, fy = px + math.cos(yaw) * 9, py + math.sin(yaw) * 9
    for tag, (x, y, d) in (('in', (px, py, deg)), ('back', (fx, fy, deg + 180))):
        n = 'ov%d_%s' % (c['idx'], tag)
        lines.append('%s %d %.2f %.2f %.2f %.1f -12' % (n, c['idx'], x, y, z, d)); names.append((c['idx'], c['name'], n))
open(os.path.join(out, 'overview.txt'), 'w').write('\n'.join(lines) + '\n')
subprocess.run([os.path.abspath(exe), '--map-shots=overview.txt'], cwd=out, timeout=1800)
w, h = int(960 * scale), int(540 * scale)
rows = (len(names) + 1) // 2
sheet = Image.new('RGB', (w * 2, (h + 14) * rows), 'black'); draw = ImageDraw.Draw(sheet)
for i, (idx, name, n) in enumerate(names):
    p = os.path.join(out, '.build', 'shot', n + '.ppm')
    x, y = (i % 2) * w, (i // 2) * (h + 14)
    draw.text((x + 4, y + 1), '%d %s (%s)' % (idx, name, n.split('_')[1]), fill='yellow')
    if os.path.exists(p): sheet.paste(Image.open(p).resize((w, h)), (x, y + 14))
path = os.path.join(out, 'overview-%d-%d.png' % (first, last)); sheet.save(path); print(path)
