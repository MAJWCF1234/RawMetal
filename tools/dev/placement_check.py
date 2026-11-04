"""Placement checks the engine audit cannot make: which way things face.

  python tools/dev/placement_check.py DUMPDIR LEVEL [LEVEL ...] [--json OUT]

Reads map-dump-N.txt files (RawMetal.exe --map-dump=N) and the verified front
of every directional model in tools/dev/model_fronts.json, and reports:

  SEAT AWAY      a seat whose front is not toward the table or desk beside it
  SEAT TO WALL   a seat facing a wall closer than a person's knees
  FRONT TO WALL  a cabinet, locker, sink, screen or machine facing a wall
  SIDE TO WALL   a wall-backed piece turned sideways against its wall
  BED REVERSED   a bed with its foot to the wall and its head in the room
  SCREEN AWAY    a monitor turned away from, or side-on to, the seat it serves
  DESK AWAY      a desk showing its plain back to the chair pulled up to it
  DUPLICATE      the same model twice in the same place
  IN DOORWAY     a non-solid fixture standing in a door opening
  UNKNOWN FRONT  a directional-looking model with no verified front yet

Deliberate exceptions live in tools/dev/placement_exempt.json, each with a reason.

Fronts are OBJ axes ('+z', '-z', '+x', '-x'), decided from four-side renders
(tools/dev/model_views.py). A fixture at yaw 0 draws OBJ +z toward world +y
and OBJ +x toward world +x (Scene3D facility(): X = x + px*c + pz*s,
Y = y - px*s + pz*c); a model prop turns the other way (Clutter::rotate).
Exit status 1 when any fault is found.
"""
import json, math, os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
AXES = {'+z': (0.0, 1.0), '-z': (0.0, -1.0), '+x': (1.0, 0.0), '-x': (-1.0, 0.0)}
# Names that look directional; any of these without an entry in model_fronts.json
# is reported so the table keeps up with new imports.
DIRECTIONAL = re.compile(r'chair|couch|sofa|wheelchair|bed\b|bed_|monitor|crt|_tv|terminal|console|control_board|display|'
                         r'viewer|vending|photocopier|cabinet|locker|bookcase|shelf|sink|notice|clock|fridge|cooler|coffee|'
                         r'first_aid|extinguisher|mirror|breaker|switch|scanner|reader|gauge|horn|grille|toilet|stall|'
                         r'painting|desk|keyboard|machine|mainframe|ticket|phone')
TABLE = re.compile(r'desk|table|counter|workstation|clean_bench|reception|lectern')


def load_names():
    names = {}
    for line in open(os.path.join(ROOT, 'src', 'world', 'ModelIds.h'), encoding='utf-8'):
        m = re.match(r'\s*inline constexpr int (\w+)=(\d+);', line)
        if m: names[int(m.group(2))] = m.group(1)
    return names


def local_to_world(d, yaw, prop):
    c, s = math.cos(yaw), math.sin(yaw)
    if prop: return (d[0] * c - d[1] * s, d[0] * s + d[1] * c)
    return (d[0] * c + d[1] * s, -d[0] * s + d[1] * c)


class Map:
    def __init__(self, path):
        self.fixtures, self.props, self.structs, self.doors, self.layers = [], [], [], [], []
        layer = None
        for line in open(path):
            p = line.split()
            if not p: continue
            if p[0] == 'F':
                self.fixtures.append(dict(model=int(p[1]), x=float(p[3]), y=float(p[4]), base=float(p[6]), w=float(p[8]),
                                          d=float(p[9]), h=float(p[10]), yaw=float(p[12]), solid=p[14] == '1', prop=False))
            elif p[0] == 'C':
                self.props.append(dict(model=int(p[1]), x=float(p[3]), y=float(p[4]), z=float(p[6]), w=float(p[8]),
                                       d=float(p[9]), h=float(p[10]), yaw=float(p[12]), prop=True))
            elif p[0] == 'S':
                self.structs.append(dict(x1=float(p[1]), y1=float(p[2]), x2=float(p[3]), y2=float(p[4]),
                                         bottom=float(p[6]), top=float(p[7]), rail=p[9] == '1'))
            elif p[0] == 'D':
                left, right = p[1].split('-', 1) if not p[1].startswith('-') else (p[1], p[1])
                self.doors.append(dict(left=float(left), right=float(right), y=float(p[3])))
            elif p[0] == 'LAYER':
                layer = dict(name=' '.join(p[1:-2]), z=float(p[-1]), rows=[]); self.layers.append(layer)
            elif layer is not None and len(p) == 1 and len(line.rstrip('\n')) >= 24:
                layer['rows'].append(line.rstrip('\n'))
        self.floor = self.layers[0]['z'] if self.layers else 0.0

    def wall_at(self, x, y, z):
        """Is (x, y) inside a wall at height z (absolute)?"""
        tx, ty = int(math.floor(x)), int(math.floor(y))
        if not (0 <= tx < 24 and 0 <= ty < 24): return True
        # Walls come from the storey the height is on: the highest layer at or below z.
        storey = max((l for l in self.layers if l['z'] <= z + .05), key=lambda l: l['z'], default=self.layers[0] if self.layers else None)
        if storey and len(storey['rows']) > ty and len(storey['rows'][ty]) > tx and storey['rows'][ty][tx] == '#': return True
        for s in self.structs:
            # Any solid at this height: a full wall, a lintel over a door, a canopy face.
            if not s['rail'] and s['x1'] <= x <= s['x2'] and s['y1'] <= y <= s['y2'] and s['bottom'] <= z <= s['top']:
                return True
        return False

    def gap(self, item, direction, z):
        """Clear distance from the item's face to the first wall along direction."""
        half = self.half_extent(item, direction)
        t = half
        while t < half + 3.0:
            if self.wall_at(item['x'] + direction[0] * t, item['y'] + direction[1] * t, z): return t - half
            t += .04
        return 9.0

    @staticmethod
    def half_extent(item, direction):
        # Half the item's footprint measured along a world direction.
        yaw = item['yaw']
        ax = local_to_world((1, 0), yaw, item['prop']); az = local_to_world((0, 1), yaw, item['prop'])
        return abs(direction[0] * ax[0] + direction[1] * ax[1]) * item['w'] * .5 + \
            abs(direction[0] * az[0] + direction[1] * az[1]) * item['d'] * .5


def edge_point(item, px, py):
    """Nearest point of an item's (rotated) footprint to (px, py)."""
    yaw = item['yaw']; ax = local_to_world((1, 0), yaw, item['prop']); az = local_to_world((0, 1), yaw, item['prop'])
    dx, dy = px - item['x'], py - item['y']
    u = max(-item['w'] * .5, min(item['w'] * .5, dx * ax[0] + dy * ax[1]))
    v = max(-item['d'] * .5, min(item['d'] * .5, dx * az[0] + dy * az[1]))
    return (item['x'] + u * ax[0] + v * az[0], item['y'] + u * ax[1] + v * az[1])


def check(level, mp, names, fronts):
    faults, unknown = [], set()
    items = mp.fixtures + mp.props

    def label(it):
        kind = 'prop' if it['prop'] else 'fixture'
        return '%s %s at (%.2f,%.2f) yaw %.2f' % (kind, names.get(it['model'], it['model']), it['x'], it['y'], it['yaw'])

    def fault(code, it, text):
        faults.append(dict(map=level, code=code, model=names.get(it['model'], str(it['model'])), x=round(it['x'], 2),
                           y=round(it['y'], 2), yaw=round(it['yaw'], 3), prop=it['prop'], text='%s: %s' % (label(it), text)))

    def z_of(it):
        return it['z'] if it['prop'] else mp.floor + it['base']

    # Anything table-sized with a top a seated person reaches: furniture, or a low
    # thin slab (counters and desk tops built from structure). Heights are absolute,
    # so a seat on an upper deck only meets the tables on that deck.
    tables = []
    for it in items:
        if TABLE.search(names.get(it['model'], '')) and it['w'] * it['d'] > .3:
            tables.append(dict(it, top=z_of(it) + it['h']))
    for s in mp.structs:
        if s['top'] - s['bottom'] < .13 and (s['x2'] - s['x1']) * (s['y2'] - s['y1']) > .25 and s['top'] - mp.floor > .5:
            tables.append(dict(model=-1, x=(s['x1'] + s['x2']) * .5, y=(s['y1'] + s['y2']) * .5, w=s['x2'] - s['x1'],
                               d=s['y2'] - s['y1'], yaw=0.0, prop=False, top=s['top']))
    seats = [it for it in items if fronts.get(names.get(it['model'], ''), {}).get('kind') == 'seat']

    for it in items:
        name = names.get(it['model'], '')
        entry = fronts.get(name)
        if entry is None:
            if DIRECTIONAL.search(name) and not name.startswith('item_'): unknown.add(name)
            continue
        kind = entry['kind']
        if kind == 'free': continue
        front = local_to_world(AXES[entry['front']], it['yaw'], it['prop'])
        back = (-front[0], -front[1]); side = (-front[1], front[0])
        z = z_of(it) + min(max(it['h'] * .5, .3), 1.2)
        ahead, behind = mp.gap(it, front, z), mp.gap(it, back, z)
        left, right = mp.gap(it, side, z), mp.gap(it, (-side[0], -side[1]), z)
        if kind == 'seat':
            near = None
            for t in tables:
                if t is it or not .55 < t['top'] - z_of(it) < 1.2: continue
                q = edge_point(t, it['x'], it['y']); dist = math.hypot(q[0] - it['x'], q[1] - it['y'])
                if dist < 1.0 and (near is None or dist < near[0]): near = (dist, q, t)
            if near:
                dist, q, t = near
                if dist < .05: q = (t['x'], t['y']); dist = max(math.hypot(q[0] - it['x'], q[1] - it['y']), 1e-3)
                v = ((q[0] - it['x']) / dist, (q[1] - it['y']) / dist)
                dot = front[0] * v[0] + front[1] * v[1]
                if dot < .3 and not entry.get('lounge'):
                    tname = names.get(t['model'], 'counter top')
                    fault('SEAT AWAY', it, '%s the %s %.2f m away (dot %.2f)' % ('back to' if dot < -.3 else 'sideways to', tname, dist, dot))
            if ahead < .3 and behind > ahead + .2:
                fault('SEAT TO WALL', it, 'faces a wall %.2f m from its front' % ahead)
        elif kind in ('wall', 'screen', 'desk'):
            if ahead < .25 and behind > ahead + .25:
                fault('FRONT TO WALL', it, 'front %.2f m from a wall, back %.2f m clear' % (ahead, behind))
            elif kind == 'wall' and behind > .6 and min(left, right) < .08 and ahead > .3 and not entry.get('sideways_ok'):
                fault('SIDE TO WALL', it, 'side against a wall (%.2f m) with its back %.2f m from any wall' % (min(left, right), behind))
            # A screen on a desk faces the seat at it, and a desk shows its drawer side to
            # that seat (the nearest one: a visitor's chair stands further off). A screen
            # knocked to the floor serves nobody.
            if (kind in ('screen', 'desk') or entry.get('serves_seat')) and (kind == 'desk' or z_of(it) > mp.floor + .3):
                best = None
                for s in seats:
                    if fronts[names[s['model']]].get('lounge'): continue  # an armchair is nobody's desk seat
                    if abs(z_of(s) - z_of(it)) > 1.2: continue  # another storey
                    q = edge_point(it, s['x'], s['y']) if kind == 'desk' else (it['x'], it['y'])
                    dist = math.hypot(s['x'] - q[0], s['y'] - q[1])
                    if dist < (.9 if kind == 'desk' else 1.3) and (best is None or dist < best[0]): best = (dist, s)
                if best:
                    d = math.hypot(best[1]['x'] - it['x'], best[1]['y'] - it['y'])
                    v = ((best[1]['x'] - it['x']) / max(d, 1e-3), (best[1]['y'] - it['y']) / max(d, 1e-3))
                    dot = front[0] * v[0] + front[1] * v[1]
                    # A chair at a desk's end is normal; at its back is not.
                    if d > .05 and dot < (-.3 if kind == 'desk' else .3):
                        fault('DESK AWAY' if kind == 'desk' else 'SCREEN AWAY', it, '%s the seat %.2f m away (dot %.2f)' % (
                            'turned away from' if dot < -.3 else 'side-on to', d, dot))
        elif kind == 'bed':
            # front = the foot end; the head belongs against the wall.
            if ahead < .3 and behind > .6:
                fault('BED REVERSED', it, 'foot %.2f m from a wall, head %.2f m out in the room' % (ahead, behind))

    for i, a in enumerate(items):
        for b in items[i + 1:]:
            if a['model'] == b['model'] and a['prop'] == b['prop'] and 'paper' not in names.get(a['model'], '') and abs(a['x'] - b['x']) < .05 and abs(a['y'] - b['y']) < .05 \
                    and abs(z_of(a) - z_of(b)) < .05:
                fault('DUPLICATE', a, 'a second %s in the same place' % names.get(a['model'], a['model']))
    for it in mp.fixtures:
        if it['solid'] or it['base'] > 1.9 or it['h'] < .15 or 'doorway' in names.get(it['model'], ''): continue
        for d in mp.doors:
            lo, hi = min(d['left'], d['right']) + .1, max(d['left'], d['right']) - .1
            if hi <= lo: continue
            q = edge_point(it, (lo + hi) * .5, d['y'])
            if lo < q[0] < hi and abs(q[1] - d['y']) < .35 and abs(it['y'] - d['y']) < 1.0:
                fault('IN DOORWAY', it, 'stands in the door opening x %.2f-%.2f at y %.2f' % (d['left'], d['right'], d['y']))
    return faults, unknown


def main():
    args = sys.argv[1:]
    out = None
    if '--json' in args:
        i = args.index('--json'); out = args[i + 1]; del args[i:i + 2]
    dumpdir, levels = args[0], [int(a) for a in args[1:]]
    names = load_names()
    fronts = json.load(open(os.path.join(ROOT, 'tools', 'dev', 'model_fronts.json'), encoding='utf-8'))
    fronts = {k: v for k, v in fronts.items() if not k.startswith('_')}
    # Deliberate oddities (a chair kicked over in a struggle), each with its reason.
    exempt = json.load(open(os.path.join(ROOT, 'tools', 'dev', 'placement_exempt.json'), encoding='utf-8'))
    allfaults, unknown = [], set()
    for lv in levels:
        path = os.path.join(dumpdir, 'map-dump-%d.txt' % lv)
        if not os.path.exists(path): print('== map %d: no dump' % lv); continue
        faults, unk = check(lv, Map(path), names, fronts)
        faults = [f for f in faults if not any(e['map'] == lv and e['model'] == f['model'] and abs(e['x'] - f['x']) < .05
                                               and abs(e['y'] - f['y']) < .05 for e in exempt)]
        allfaults += faults; unknown |= unk
        if faults: print('== map %d: %d faults' % (lv, len(faults)))
        for f in faults: print('   %-13s %s' % (f['code'], f['text']))
    if unknown: print('UNKNOWN FRONT (add to tools/dev/model_fronts.json): ' + ', '.join(sorted(unknown)))
    if out: json.dump(dict(faults=allfaults, unknown=sorted(unknown)), open(out, 'w'), indent=1)
    print('%s: %d maps, %d faults, %d models without a verified front' % ('FAIL' if allfaults or unknown else 'PASS', len(levels), len(allfaults), len(unknown)))
    return 1 if allfaults or unknown else 0


if __name__ == '__main__':
    sys.exit(main())
