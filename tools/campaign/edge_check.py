"""Check every planned edge of the descent chunks against the built maps.

  python tools/campaign/edge_check.py AUDITDIR [FIRST LAST]

Reads map-audit-N.txt (written by RawMetal.exe --map-audit=N) and, for each
edge in the plan (tools/campaign/plan.py), looks at the border tiles across the
edge's span in the audit's reach map: at least one must be reachable standing
('.' or '^'). A seam the player can only crawl through ('c') or not reach at
all is reported, since it strands a playthrough. So is a border tile the
player can stand in more than a metre below the edge (map-views-N.txt): the
row opened under a raised edge with nothing built up to it is a pit.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import plan

def reach_map(path):
    rows = [l.rstrip('\n') for l in open(path, encoding='utf-8', errors='replace')]
    grid = [r for r in rows if len(r) == 24 and set(r) <= set('#.^oc ')]
    return grid[-24:] if len(grid) >= 24 else None

def tiles(edge):
    a, b = int(edge['a']), int(-(-edge['b'] // 1))
    side = edge['side']
    for t in range(a, b):
        yield (0, t) if side == 'N' else (23, t) if side == 'S' else (t, 0) if side == 'W' else (t, 23)

# Seams designed as crawls: Operations Corridor's yard shutter is jammed at
# knee height and the player crouches under it.
CRAWLS = {(57, 'S')}

def main():
    folder = sys.argv[1]
    first, last = (int(sys.argv[2]), int(sys.argv[3])) if len(sys.argv) > 3 else (34, 67)
    bad = 0
    for c in plan.CHUNKS:
        if not first <= c['idx'] <= last: continue
        path = os.path.join(folder, 'map-audit-%d.txt' % c['idx'])
        grid = reach_map(path) if os.path.exists(path) else None
        if grid is None: print('%d: no reach map' % c['idx']); bad += 1; continue
        with open(path, encoding='utf-8') as audit:
            border = [(int(f[1]), int(f[2]), float(f[3]), f[4]) for line in audit if (f := line.split()) and f[0] == 'BORDER']
        if not border:
            print('%d: no height-aware BORDER records; regenerate audit' % c['idx']); bad += 1; continue
        for e in c['edges']:
            cells = set(tiles(e))
            marks = ''.join(next((m for x, y, z, m in border if (y, x) == (r, col) and abs(z - e['z']) <= .45 and m == '.'), next((m for x, y, z, m in border if (y, x) == (r, col) and abs(z - e['z']) <= .45), 'o')) for r, col in tiles(e))
            ok = '.^c' if (c['idx'], e['side']) in CRAWLS else '.^'
            if not any(m in ok for m in marks):
                print('%d %s: %s %g-%g z %g, border tiles "%s"' % (c['idx'], c['name'], e['side'], e['a'], e['b'], e['z'], marks)); bad += 1
            low = sorted({round(z, 2) for x, y, z, _ in border if (y, x) in cells and z < e['z'] - 1})
            if low:
                print('%d %s: %s %g-%g z %g, pit under the edge at z %s' % (c['idx'], c['name'], e['side'], e['a'], e['b'], e['z'], low)); bad += 1
    print('edges ok' if not bad else '%d edge problems' % bad)
    sys.exit(1 if bad else 0)

main()
