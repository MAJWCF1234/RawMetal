"""Convex hulls for props whose shape is clearly not a box: rocks, cones, irregular things and the Blender gibs.

  python tools/library/make_hulls.py [--fill F] [--list]

Reads each model's mesh (the library's OBJ files by catalog.json slot, and the engine's own facility models that are a
plain OBJ), maps it into the prop's frame as the renderer draws it (Scene3D drawClutter: mesh x, z, y to the prop's
x, y, up, each over its range, so the box is -0.5..0.5 on every axis), and takes its convex hull. A model whose hull
fills less than --fill (0.7) of its box (a gib piece: 0.8) gets the hull, simplified to at most 32 corners (the
corners along the axes and diagonals first, then each the farthest outside the hull of those so far), so its box is
kept. Writes src/world/PropHulls.h; PropBodies.cpp scales each by the prop's dims into a Vectra hull part.
Models the shape catalogue already gives a shape (cylinders, chairs, tables, containers, padded props) keep theirs.
"""
import json, os, re, sys
import numpy as np
from scipy.spatial import ConvexHull

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src')
MAIN_SRC = os.path.join(ROOT.split(os.sep + '.claude' + os.sep)[0], 'src')
MAX_VERTS = 32
MAX_SIZE = 3.0   # metres: bigger things are scenery, never loose props

def src_file(rel):
    """An asset: this checkout's, else the main checkout's (a worktree lacks the gitignored library)."""
    here = os.path.join(SRC, rel)
    return here if os.path.exists(here) else os.path.join(MAIN_SRC, rel)

def obj_points(path):
    pts = []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('v '):
                p = line.split()
                pts.append((float(p[1]), float(p[2]), float(p[3])))
    return np.array(pts, dtype=np.float64)

def models():
    """(slot, name, obj path, size or None) for every model that is a plain OBJ."""
    out = []
    rc = {}
    for line in open(os.path.join(SRC, 'assets.rc'), encoding='utf-8', errors='replace'):
        m = re.match(r'\s*(\d+)\s+RCDATA\s+"([^"]+)"', line)
        if m: rc[int(m.group(1))] = m.group(2)
    header = open(os.path.join(SRC, 'renderer', 'SoftwareRenderer.h'), encoding='utf-8').read()
    i = header.index('m_facilityMeshes{Mesh::Deferred{},')
    body = header[i:header.index('};', i)]
    meshes = re.findall(r'Mesh\{(\d+)((?:,[^}]*)?)\}', body)
    catalog = json.load(open(src_file(os.path.join('assets', 'library', 'catalog.json'))))
    names = {e['slot']: (n, e) for n, e in catalog.items()}
    for slot, (res, rest) in enumerate(meshes):
        if rest.strip(','):   # a node of a bigger file, or an animated mesh: not here
            continue
        path = rc.get(int(res), '')
        if not path.endswith('.obj'): continue
        name, e = names.get(slot, (None, None))
        if name is None and slot >= 63: continue
        size = (e['width'], e['depth'], e['height']) if e else None
        out.append((slot, name or 'facility_%d' % slot, src_file(path), size))
    return out

def simplify(points, cap):
    """At most `cap` corners: the farthest along 26 ways, then each the farthest outside the hull so far."""
    hull = ConvexHull(points)
    cand = points[hull.vertices]
    if len(cand) <= cap: return cand
    sel = []
    mid = (cand.min(0) + cand.max(0)) / 2
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                if not (dx or dy or dz): continue
                k = int(np.argmax((cand - mid) @ np.array([dx, dy, dz], dtype=np.float64)))
                if k not in sel and len(sel) < cap: sel.append(k)
    while len(sel) < cap:
        try: h = ConvexHull(cand[sel])
        except Exception: break
        out = (cand @ h.equations[:, :3].T + h.equations[:, 3]).max(1)
        k = int(np.argmax(out))
        if out[k] < 1e-3: break
        sel.append(k)
    return cand[sel]

def main():
    args = sys.argv[1:]
    fill_cut = float(args[args.index('--fill') + 1]) if '--fill' in args else 0.7
    rows = []
    for slot, name, path, size in models():
        if not os.path.exists(path): continue
        if size and max(size) > MAX_SIZE: continue
        p = obj_points(path)
        if len(p) < 4: continue
        lo, hi = p.min(0), p.max(0)
        rng = np.maximum(hi - lo, 1e-6)
        c = (lo + hi) / 2
        q = np.stack([(p[:, 0] - c[0]) / rng[0], (p[:, 2] - c[2]) / rng[2], (p[:, 1] - c[1]) / rng[1]], 1)
        try: hull = ConvexHull(q)
        except Exception: continue
        fill = hull.volume
        gib = name.startswith('gib_')
        if fill >= (0.8 if gib else fill_cut): continue
        pts = simplify(q, MAX_VERTS)
        kept = ConvexHull(pts).volume
        rows.append((slot, name, fill, kept, pts))
        if '--list' in args: print('%4d %-34s fill %.2f  kept %.2f of it  %d corners' % (slot, name, fill, kept / fill, len(pts)))
    rows.sort()
    out = ['#pragma once',
           '// Convex hulls of props whose shape is clearly not a box (rocks, cones, irregular things, gib pieces), in the prop\'s',
           '// frame with its box -0.5..0.5 on each axis (x, y, up): PropBodies.cpp scales one by the prop\'s dims into a Vectra',
           '// hull part. Generated by `python tools/library/make_hulls.py` from each model\'s mesh; do not edit by hand.',
           '#include <cstddef>',
           'namespace retro::hulls {',
           'struct Entry {int model;int count;float fill;float p[%d][3];};' % MAX_VERTS,
           'inline constexpr Entry entries[]={']
    for slot, name, fill, kept, pts in rows:
        coords = ','.join('{%.4ff,%.4ff,%.4ff}' % tuple(v) for v in pts)
        out.append(' {%d,%d,%.3ff,{%s}},   // %s' % (slot, len(pts), fill, coords, name))
    out += ['};',
            '// The hull for a model, or null.',
            'inline const Entry* find(int model){for(const auto& e:entries)if(e.model==model)return &e;return nullptr;}',
            '}', '']
    dest = os.path.join(SRC, 'world', 'PropHulls.h')
    open(dest, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    print('%d hulls -> %s' % (len(rows), dest))

if __name__ == '__main__':
    main()
