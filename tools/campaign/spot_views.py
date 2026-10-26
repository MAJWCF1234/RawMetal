"""Contact sheets of the descent chunks as a player finds them.

  python tools/campaign/spot_views.py EXE RUNDIR FIRST LAST

For each chunk, six walkable spots spread across it (map-views-N.txt, written
by RawMetal.exe --map-audit=N into RUNDIR), each looking down its longest open
sightline on its own level. Every view carries the states a player has set on
first arriving (ARRIVE), so switched lamps and hazards look as they will in a
playthrough rather than as a fresh map. Frames are clean (--clean-shots: no
hands or HUD); sheets of two chunks go to RUNDIR/spots-A-B.png.
"""
import math, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plan
from PIL import Image, ImageDraw

# Chapter 1, played straight through: the black start, the hall's restrike,
# the vent supply, MCC-2 isolated, the shear pin, the gate and the override.
P = ['pwr_gen_online', 'pwr_sub_a', 'pwr_feeder9']
M = P + ['mh_vent_supply', 'mh_sf2_isolated', 'mh_ah_dropped']
W = M + ['water_isolated']
R = W + ['maint_shutter_pin', 'perim_gate']
ARRIVE = {37: ['pwr_gen_online'], 40: ['pwr_gen_online']}
for i in range(41, 45): ARRIVE[i] = P + ['mh_bay_a', 'mh_bay_b', 'mh_bay_c', 'mh_bay_d']
for i in (45, 46): ARRIVE[i] = P
for i in range(47, 50): ARRIVE[i] = M
for i in range(50, 57): ARRIVE[i] = W
for i in range(57, 60): ARRIVE[i] = W + ['maint_shutter_pin']
for i in (60, 61): ARRIVE[i] = R
ARRIVE[62] = ARRIVE[63] = R + ['delta_override']
ARRIVE[64] = ARRIVE[65] = R + ['delta_override', 'ere_util_damper', 'ere_util_supply']
ARRIVE[66] = ARRIVE[67] = ARRIVE[64] + ['decon_cycle']

COUNT, W_PX, H_PX = 6, 480, 270

def spots(c, run):
    pts = [tuple(map(float, l.split())) for l in open(os.path.join(run, 'map-views-%d.txt' % c['idx'])) if l.strip()]
    # Spread the spots: start nearest the spawn, then always the farthest from those taken.
    (sx, sy), _, _ = plan.spawn(c)
    pick = [min(pts, key=lambda p: (p[0] - sx) ** 2 + (p[1] - sy) ** 2)]
    while len(pick) < min(COUNT, len(pts)):
        pick.append(max(pts, key=lambda p: min((p[0] - q[0]) ** 2 + (p[1] - q[1]) ** 2 + ((p[2] - q[2]) * 3) ** 2 for q in pick)))
    for x, y, z in pick:
        level = [p for p in pts if abs(p[2] - z) < 2.5] or pts
        tx, ty, _ = max(level, key=lambda p: (p[0] - x) ** 2 + (p[1] - y) ** 2)
        yield x, y, z, math.degrees(math.atan2(ty - y, tx - x))

def main():
    exe, run, first, last = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    lines, chunks = [], []
    for c in plan.CHUNKS:
        if not first <= c['idx'] <= last: continue
        names, states = [], ARRIVE.get(c['idx'])
        for k, (x, y, z, yaw) in enumerate(spots(c, run)):
            n = 'sv_%d_%d' % (c['idx'], k)
            lines.append('%s %d %.2f %.2f %.2f %.1f -4%s' % (n, c['idx'], x, y, z, yaw, ' set ' + ','.join(states) if states else ''))
            names.append((n, '%.0f,%.0f,%.0f>%.0f' % (x, y, z, yaw)))
        chunks.append((c['idx'], c['name'], names))
    open(os.path.join(run, 'spotviews.txt'), 'w').write('\n'.join(lines) + '\n')
    subprocess.run([os.path.abspath(exe), '--map-shots=spotviews.txt', '--clean-shots'], cwd=run, timeout=3600)
    for s in range(0, len(chunks), 2):
        group = chunks[s:s + 2]
        sheet = Image.new('RGB', (W_PX * 3, (H_PX + 14) * 2 * len(group)), 'black'); d = ImageDraw.Draw(sheet)
        for r, (idx, name, names) in enumerate(group):
            for k, (n, where) in enumerate(names):
                x, y = (k % 3) * W_PX, (r * 2 + k // 3) * (H_PX + 14)
                p = os.path.join(run, '.build', 'shot', n + '.ppm')
                if os.path.exists(p): sheet.paste(Image.open(p).convert('RGB').resize((W_PX, H_PX)), (x, y + 14))
                d.text((x + 3, y + 1), '%d %s  #%d %s' % (idx, name, k, where), fill='yellow')
        out = os.path.join(run, 'spots-%d-%d.png' % (group[0][0], group[-1][0])); sheet.save(out); print(out)

main()
