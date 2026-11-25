"""Open blast_door_frame: it came with a shut leaf (bars, wheels, keypad, hinges) modelled in, which nothing
could ever open. Keeps only the ring (the one connected part in the _x material that spans the whole width),
centred in depth and stretched to the model's catalogue depth so its size is unchanged. World.cpp gives it
two piers and a lintel round the opening. Run once after (re)importing the part; running it again changes
nothing.  python tools/library/open_blast_frame.py [library-dir]"""
import collections, os, shutil, sys
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LIB = sys.argv[1] if len(sys.argv) > 1 else os.path.join(ROOT, 'src', 'assets', 'library')
DEPTH = .56
path = os.path.join(LIB, 'blast_door_frame.obj')
lines = open(path).read().splitlines()
if len({l.split()[1] for l in lines if l.startswith('usemtl')}) == 1:
    print('blast_door_frame already open'); sys.exit()
backup = os.path.join(ROOT, '.build', 'backup-doorways')
os.makedirs(backup, exist_ok=True); shutil.copy2(path, backup)
V = [list(map(float, l.split()[1:4])) for l in lines if l.startswith('v ')]
index = lambda face: [int(p.split('/')[0]) - 1 for p in face.split()[1:]]
key = lambda i: tuple(round(c, 3) for c in V[i])
parent = {}
def find(a):
    parent.setdefault(a, a)
    while parent[a] != a:
        parent[a] = parent[parent[a]]; a = parent[a]
    return a
for l in lines:
    if l.startswith('f '):
        ids = index(l)
        for i in ids[1:]:
            parent[find(key(i))] = find(key(ids[0]))
parts, material = collections.defaultdict(list), None
for n, l in enumerate(lines):
    if l.startswith('usemtl'): material = l.split()[1]
    elif l.startswith('f '): parts[find(key(index(l)[0]))].append((n, material, index(l)))
def width(fs): xs = [V[i][0] for _, _, ids in fs for i in ids]; return max(xs) - min(xs)
ring = max((fs for fs in parts.values() if all(m.endswith('_x') for _, m, _ in fs)), key=width)
keep = {n for n, _, _ in ring}
used = {i for _, _, ids in ring for i in ids}
lo, hi = min(V[i][2] for i in used), max(V[i][2] for i in used)
rest = V[min(used)]
out, vi = [], 0
for n, l in enumerate(lines):
    if l.startswith('v '):
        v = V[vi] if vi in used else rest      # dropped vertices must not widen the model's bounds
        l = 'v %.6f %.6f %.6f' % (v[0], v[1], ((v[2] - lo) / (hi - lo) - .5) * DEPTH); vi += 1
    elif l.startswith('f ') and n not in keep: continue
    elif l.startswith('usemtl') and l.split()[1] != ring[0][1]: continue
    out.append(l)
open(path, 'w', newline='\n').write('\n'.join(out) + '\n')
print('blast_door_frame opened:', len(ring), 'faces kept')
