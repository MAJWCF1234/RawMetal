"""Evidence frames for placement_check.py faults.

  python tools/dev/placement_shots.py CHECK_JSON DUMPDIR EXE OUTDIR [--codes A,B] [--maps 68,69]

For every fault, renders the item from the room side at eye height (two
distances), then writes OUTDIR/sheet-NN.png contact sheets labelled with the
fault, so each one can be confirmed or dismissed by eye at full size.
"""
import json, math, os, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import placement_check as pc
from PIL import Image, ImageDraw


def floor_under(mp, x, y, bottom):
    """The walkable surface at (x, y) on the item's storey: the highest deck
    top no higher than the item's bottom and no more than 2.6 m below it."""
    best = max([l['z'] for l in mp.layers if l['z'] <= bottom + .05] or [mp.floor])
    for s in mp.structs:
        if s['x1'] <= x <= s['x2'] and s['y1'] <= y <= s['y2'] and bottom - 2.6 < s['top'] <= bottom + .05 and s['top'] > best:
            best = s['top']
    return best


def main():
    args = sys.argv[1:]
    opts = {}
    for key in ('--codes', '--maps'):
        if key in args:
            i = args.index(key); opts[key] = args[i + 1].split(','); del args[i:i + 2]
    check, dumpdir, exe, out = args[:4]
    out = os.path.abspath(out); os.makedirs(out, exist_ok=True)
    faults = json.load(open(check))['faults']
    if '--codes' in opts: faults = [f for f in faults if f['code'] in opts['--codes']]
    if '--maps' in opts: faults = [f for f in faults if str(f['map']) in opts['--maps']]
    names = pc.load_names(); fronts = {k: v for k, v in json.load(open(os.path.join(pc.ROOT, 'tools', 'dev', 'model_fronts.json'))).items() if not k.startswith('_')}
    maps, lines, shots = {}, [], []
    for n, f in enumerate(faults):
        mp = maps.setdefault(f['map'], pc.Map(os.path.join(dumpdir, 'map-dump-%d.txt' % f['map'])))
        item = next(it for it in mp.fixtures + mp.props if it['prop'] == f['prop'] and abs(it['x'] - f['x']) < .006
                    and abs(it['y'] - f['y']) < .006 and names.get(it['model']) == f['model'])
        z = (item['z'] if item['prop'] else mp.floor + item['base']) + item['h'] * .5
        entry = fronts.get(f['model'], {'front': '+z'})
        front = pc.local_to_world(pc.AXES[entry['front']], item['yaw'], item['prop'])
        # Look from whichever of the four sides has the most room.
        sides = [front, (-front[0], -front[1]), (-front[1], front[0]), (front[1], -front[0])]
        # A seat reads best side-on (backrest against seat), anything else from the room.
        if entry.get('kind') == 'seat': sides = sides[2:]
        best = max(sides, key=lambda d: mp.gap(item, d, z))
        for k, dist in enumerate((1.4, 2.6)):
            room = min(dist, mp.half_extent(item, best) + max(mp.gap(item, best, z) - .25, .3))
            cx, cy = item['x'] + best[0] * room, item['y'] + best[1] * room
            yaw = math.degrees(math.atan2(-best[1], -best[0])) % 360
            pitch = math.atan2(z - (mp.floor + 1.42), room) * 140
            name = 'pf%03d%s' % (n, 'ab'[k])
            feet = floor_under(mp, cx, cy, z - item['h'] * .5)
            pitch = math.atan2(z - (feet + 1.42), room) * 140
            lines.append('%s %d %.2f %.2f %.2f %.1f %.0f' % (name, f['map'], cx, cy, feet, yaw, pitch))
            shots.append((name, n))
        # Where the recorded front points in this frame, so a reviewer can check it.
        look = (-best[0], -best[1]); right = (-look[1], look[0])
        fwd, side = front[0] * look[0] + front[1] * look[1], front[0] * right[0] + front[1] * right[1]
        f['front_in_frame'] = ('AWAY' if fwd > .7 else 'AT CAMERA' if fwd < -.7 else 'RIGHT' if side > 0 else 'LEFT')
    listfile = os.path.join(out, 'shots.txt')
    open(listfile, 'w').write('\n'.join(lines) + '\n')
    subprocess.run([os.path.abspath(exe), '--map-shots=' + listfile, '--clean-shots'], cwd=out, check=False)
    tiles = []
    for n, f in enumerate(faults):
        frames = []
        for k in 'ab':
            p = os.path.join(out, '.build', 'shot', 'pf%03d%s.ppm' % (n, k))
            frames.append(Image.open(p).convert('RGB').resize((640, 360)) if os.path.exists(p) else Image.new('RGB', (640, 360)))
        tile = Image.new('RGB', (1280, 390), (30, 30, 30)); tile.paste(frames[0], (0, 30)); tile.paste(frames[1], (640, 30))
        ImageDraw.Draw(tile).text((6, 8), '#%d map %d %s  %s  [front points %s]' % (n, f['map'], f['code'], f['text'], f.get('front_in_frame'))[:180], fill=(255, 230, 0))
        tiles.append(tile)
    for s in range(0, len(tiles), 5):
        sheet = Image.new('RGB', (1280, 390 * len(tiles[s:s + 5])))
        for i, t in enumerate(tiles[s:s + 5]): sheet.paste(t, (0, 390 * i))
        sheet.save(os.path.join(out, 'sheet-%02d.png' % (s // 5)))
    json.dump(faults, open(os.path.join(out, 'faults.json'), 'w'), indent=1)
    print('%d faults, %d sheets in %s' % (len(faults), (len(tiles) + 4) // 5, out))


if __name__ == '__main__':
    main()
