"""Open the three doorway wall parts that came with a shut leaf modelled in (their collision always had the
opening, so the player walked through a drawn door). Drops the leaf's faces and raises the opening to the
engine's swinging leaf (Door::SwingHeight), the wall staying 3 m; the kits then hang a working DOOR in it.
Run once after (re)importing parts-1.5.6; running it again changes nothing."""
import os, shutil
ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
LIB = os.path.join(ROOT, 'src', 'assets', 'library')
HEAD = 2.36
# model: (leaf material name fragments, the modelled opening's head)
PARTS = {'bld_doorway_frame': (('_door_4', '_door_handle_2'), 2.1),
         'bld_doorway_wide': (('_door_hm_1',), 2.5),
         'bld_wall_doorway': (('_door_hr_1',), 2.1)}

for name, (leaf, head) in PARTS.items():
    path = os.path.join(LIB, name + '.obj')
    lines = open(path).read().splitlines()
    is_leaf = lambda m: any(k in m for k in leaf) and 'door_frame' not in m
    if not any(l.startswith('usemtl') and is_leaf(l.split()[1]) for l in lines):
        print(name, 'already open'); continue
    backup = os.path.join(ROOT, '.build', 'backup-doorways')
    os.makedirs(backup, exist_ok=True); shutil.copy2(path, backup)
    top = max(float(l.split()[2]) for l in lines if l.startswith('v '))

    def remap(y):  # 0..head stretches to 0..HEAD, the wall above keeps its top
        return y * HEAD / head if y <= head else HEAD + (y - head) * (top - HEAD) / (top - head)

    out, skip = [], False
    for l in lines:
        if l.startswith('usemtl'):
            skip = is_leaf(l.split()[1])
        if skip and (l.startswith('f ') or l.startswith('usemtl')):
            continue
        if l.startswith('v '):
            t = l.split(); t[2] = '%.6f' % remap(float(t[2])); l = ' '.join(t)
        out.append(l)
    open(path, 'w', newline='\n').write('\n'.join(out) + '\n')
    print(name, 'opened')
