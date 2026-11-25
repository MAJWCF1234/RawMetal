"""Collision a walker can use for fixtures that are not a cube: arches, pipes, culverts, vents, stairs, frames.

  python tools/library/make_walk_shapes.py [--list] [--only NAME]

A fixture placed in a map collides as one solid box unless World.cpp knows its shape, so a player met a wall where
an arch's opening or a pipe's bore was, and could not climb a staircase model. This reads each model's mesh (the
same OBJ files and frame as make_hulls.py: mesh x, z, y to the fixture's across, depth, up, each over its range so
the box is -0.5..0.5) and finds the space a body can really use:

1. The mesh's surface is voxelised at about CELL metres (the model at its catalogue size).
2. A probe the size of a prone body (PROBE metres: across, depth, up) is placed wherever it fits in free space.
3. Placements joined to the open air round the box (not from below: that is the floor) are the reachable space;
   swept by the probe again, and carried down to the surface under it, they are the space a body can occupy. Everything else inside the box is solid, so
   cracks, the hollow inside of a closed shell and gaps narrower than a body all stay solid.
4. A staircase (a name with stair or step) gets a 45 degree ramp over its treads, as a clip ramp over steps.
5. A model whose usable space is at least MIN_SPACE of its box, and is at least 1 m in some direction, gets that
   solid as at most MAX_BOXES boxes (greedy merge; coarser across and along if need be), written to src/world/WalkShapes.h. World.cpp places them for every fixture
   of the model that has no hand-made shape (quarter turns, flat ground; others stay one box).
Do not edit the header by hand.
"""
import os, sys
import numpy as np
from scipy import ndimage
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from make_hulls import models, SRC   # noqa: E402

CELL = (0.08, 0.08, 0.04)     # metres (across, depth, up): finer up, so a floor or a step lands within 4 cm
PROBE = (0.42, 0.42, 0.62)    # a prone body: 2 x the 0.2 m hull radius (+2 cm), and ProneHeight 0.58 (+4 cm)
MIN_SPACE = 0.06              # share of the box a body can use, for the model to get its shape
MIN_SIZE = 1.0                # metres: smaller things stay a box
MAX_BOXES = 128
RISER = 0.6                   # metres: the highest step a staircase ramp spans
RAMPED = ('stair', 'step')    # models whose name says they are climbed get ramps over their treads
MAX_CELLS = 160               # per axis; a bigger model is voxelised coarser

def obj_triangles(path):
    verts, tris = [], []
    with open(path, encoding='utf-8', errors='replace') as f:
        for line in f:
            if line.startswith('v '):
                p = line.split(); verts.append((float(p[1]), float(p[2]), float(p[3])))
            elif line.startswith('f '):
                idx = []
                for tok in line.split()[1:]:
                    i = int(tok.split('/')[0]); idx.append(i - 1 if i > 0 else len(verts) + i)
                for k in range(1, len(idx) - 1): tris.append((idx[0], idx[k], idx[k + 1]))
    return np.array(verts, dtype=np.float64), np.array(tris, dtype=np.int64)

def voxelise(tri, dims, cells):
    """Surface cells of triangles given in metres within [0, dims] on each axis."""
    grid = np.zeros(cells, dtype=bool)
    step = dims / np.array(cells)
    a, b, c = tri[:, 0], tri[:, 1], tri[:, 2]
    edge = np.max(np.stack([np.linalg.norm(b - a, axis=1), np.linalg.norm(c - b, axis=1), np.linalg.norm(a - c, axis=1)], 1), 1)
    n = np.minimum(400, np.maximum(1, np.ceil(edge / (step.min() * 0.4)))).astype(int)
    for k in np.unique(n):
        sel = n == k
        i, j = np.meshgrid(np.arange(k + 1), np.arange(k + 1), indexing='ij'); keep = i + j <= k
        u = (i[keep] / k)[None, :, None]; v = (j[keep] / k)[None, :, None]
        p = a[sel][:, None, :] + (b[sel] - a[sel])[:, None, :] * u + (c[sel] - a[sel])[:, None, :] * v
        # Up, a point counts in the cell below once it is in a cell's lowest quarter: a floor lands within a
        # centimetre of the mesh, not up to a cell above it (a culvert's 0.2 m floor must stay under the 0.215 m step).
        q = np.floor(p.reshape(-1, 3) / step - np.array([0, 0, .25])).astype(int)
        q = np.clip(q, 0, np.array(cells) - 1)
        grid[q[:, 0], q[:, 1], q[:, 2]] = True
    return grid

def ramp(solid, dims, cells):
    """A staircase's treads under a 45 degree slope from each nosing, as a level designer lays a clip ramp over steps:
    a pack's risers (0.28-0.53 m) are higher than the 0.215 m a walker steps up. Only where the step is at most RISER."""
    dz = dims[2] / cells[2]; cx, cy = dims[0] / cells[0], dims[1] / cells[1]
    nz = cells[2]
    has = solid.any(2)
    top = np.where(has, nz - np.argmax(solid[:, :, ::-1], axis=2), 0)   # cells: the top of each column's highest solid
    best = top.copy()
    reach = int(np.ceil(RISER / min(cx, cy)))
    for ox in range(-reach, reach + 1):
        for oy in range(-reach, reach + 1):
            d = np.hypot(ox * cx, oy * cy)
            if d == 0 or d > RISER: continue
            src = np.zeros_like(top)
            xs = slice(max(0, ox), cells[0] + min(0, ox)); xd = slice(max(0, -ox), cells[0] + min(0, -ox))
            ys = slice(max(0, oy), cells[1] + min(0, oy)); yd = slice(max(0, -oy), cells[1] + min(0, -oy))
            src[xd, yd] = top[xs, ys]
            lift = src - np.ceil(d / dz).astype(int)
            ok = (src - top) * dz <= RISER
            best = np.where(ok, np.maximum(best, lift), best)
    out = solid.copy()
    z = np.arange(nz)[None, None, :]
    out |= (z < best[:, :, None]) & (z >= top[:, :, None]) & (best > top)[:, :, None]
    return out

def boxes_of(solid):
    """Greedy merge of a boolean grid into boxes (x0, y0, z0, x1, y1, z1), end exclusive."""
    left = solid.copy(); out = []
    nx, ny, nz = solid.shape
    for z in range(nz):
        for y in range(ny):
            for x in range(nx):
                if not left[x, y, z]: continue
                x1 = x
                while x1 + 1 < nx and left[x1 + 1, y, z]: x1 += 1
                y1 = y
                while y1 + 1 < ny and left[x:x1 + 1, y1 + 1, z].all(): y1 += 1
                z1 = z
                while z1 + 1 < nz and left[x:x1 + 1, y:y1 + 1, z1 + 1].all(): z1 += 1
                left[x:x1 + 1, y:y1 + 1, z:z1 + 1] = False
                out.append((x, y, z, x1 + 1, y1 + 1, z1 + 1))
    return out

def shape(path, size, cell, ramped=False):
    verts, tris = obj_triangles(path)
    if len(tris) == 0: return None
    lo, hi = verts.min(0), verts.max(0); rng = np.maximum(hi - lo, 1e-6)
    # Prop frame (across, depth, up) = OBJ (x, z, y), scaled to the model's size in metres.
    q = (verts - lo) / rng
    q = q[:, [0, 2, 1]]
    dims = np.array(size if size else (rng[0], rng[2], rng[1]), dtype=np.float64)
    k = max(1.0, max(dims[i] / cell[i] for i in range(3)) / MAX_CELLS)
    cells = tuple(int(max(1, round(dims[i] / (cell[i] * k)))) for i in range(3))
    surface = voxelise(q[tris] * dims, dims, cells)
    probe = tuple(max(1, int(round(p / (dims[i] / cells[i])))) for i, p in enumerate(PROBE))
    pad = tuple(p + 1 for p in probe)
    # Open air round the box on every side but the bottom (the floor is solid).
    free = np.ones(tuple(c + 2 * p for c, p in zip(cells, pad)), dtype=bool)
    box = (slice(pad[0], pad[0] + cells[0]), slice(pad[1], pad[1] + cells[1]), slice(pad[2], pad[2] + cells[2]))
    free[box] = ~surface
    free[:, :, :pad[2]] = False
    # A placement at a cell: the probe's corner there (window spans +probe on each axis).
    fits = ndimage.minimum_filter(free.astype(np.uint8), size=probe, origin=tuple(-(p // 2) for p in probe), mode='constant', cval=0).astype(bool)
    labels, count = ndimage.label(fits)
    outside = np.zeros_like(fits); outside[box] = True; outside = ~outside & fits
    keep = np.unique(labels[outside]); keep = keep[keep > 0]
    reach = np.isin(labels, keep)
    space = ndimage.maximum_filter(reach.astype(np.uint8), size=probe, origin=tuple((p - 1) // 2 for p in probe), mode='constant', cval=0).astype(bool)
    inside_space = space[box].copy()
    # A body's space reaches down to the surface it stands on: the probe, wider than a curved or dented floor's
    # low middle, left the floor higher than the mesh, a lip a walker could not step over.
    for z in range(cells[2] - 2, -1, -1):
        inside_space[:, :, z] |= inside_space[:, :, z + 1] & ~surface[:, :, z]
    share = inside_space.mean()
    solid = ~inside_space
    if ramped: solid = ramp(solid, dims, cells)
    return dict(cells=cells, share=share, solid=solid, dims=dims, probe=probe)

def main():
    args = sys.argv[1:]
    only = args[args.index('--only') + 1] if '--only' in args else None
    rows = []
    for slot, name, path, size in models():
        if only and name != only: continue
        if not os.path.exists(path): continue
        dims = size
        if dims and (max(dims) < MIN_SIZE or min(dims) <= 0): continue
        try: s = shape(path, size, CELL, any(w in name for w in RAMPED))
        except Exception as e:
            print('skip', name, e); continue
        if s is None or max(s['dims']) < MIN_SIZE or s['share'] < MIN_SPACE or not s['solid'].any(): continue
        b = boxes_of(s['solid'])
        c = s['cells']
        if len(b) > MAX_BOXES:
            # Coarser across and along (never up, so floors and steps stay true): a cell is solid when any it joins is.
            for coarse in (2, 3, 4):
                pad = [(-c[0]) % coarse, (-c[1]) % coarse]
                g = np.pad(s['solid'], [(0, pad[0]), (0, pad[1]), (0, 0)], constant_values=False)
                g = g.reshape(g.shape[0] // coarse, coarse, g.shape[1] // coarse, coarse, g.shape[2]).any(axis=(1, 3))
                b2 = boxes_of(g)
                if len(b2) <= MAX_BOXES:
                    b = [(min(x0 * coarse, c[0]), min(y0 * coarse, c[1]), z0, min(x1 * coarse, c[0]), min(y1 * coarse, c[1]), z1) for x0, y0, z0, x1, y1, z1 in b2]; break
            else:
                if '--list' in args: print('%4d %-34s too many boxes (%d)' % (slot, name, len(b)))
                continue
        norm = [tuple((v / c[i % 3]) - 0.5 for i, v in enumerate(bb)) for bb in b]
        rows.append((slot, name, s['share'], norm))
        if '--list' in args: print('%4d %-34s space %.2f  %2d boxes  %s m' % (slot, name, s['share'], len(norm), 'x'.join('%.2f' % d for d in s['dims'])))
    rows.sort()
    if only: return
    q = lambda v: max(0, min(65535, int(round((v + 0.5) * 65535))))
    out = ['#pragma once',
           '// Collision a walker can use for fixtures that are not a cube (arches, pipes, culverts, vents, stairs, frames): the solid',
           '// part of each model as boxes in its frame, 0..65535 over the box on each axis (across, depth, up), from the space a',
           '// prone body can reach in the model mesh. Generated by `python tools/library/make_walk_shapes.py`; do not edit by hand.',
           '#include <cstdint>',
           'namespace retro::walkshapes {',
           'struct Box {std::uint16_t x0,y0,z0,x1,y1,z1;};',
           'struct Entry {int model;float space;int first,count;};',
           'inline constexpr Box boxes[]={']
    entries = []; n = 0
    for slot, name, share, norm in rows:
        out.append(' ' + ','.join('{%d,%d,%d,%d,%d,%d}' % tuple(q(v) for v in bb) for bb in norm) + ',   // %s' % name)
        entries.append((slot, share, n, len(norm), name)); n += len(norm)
    out += ['};', 'inline constexpr Entry entries[]={']
    for slot, share, first, count, name in entries:
        out.append(' {%d,%.3ff,%d,%d},   // %s' % (slot, share, first, count, name))
    out += ['};',
            '// The shape for a model, or null.',
            'inline const Entry* find(int model){for(const auto& e:entries)if(e.model==model)return &e;return nullptr;}',
            '// A box coordinate in the model frame, -0.5..0.5.',
            'inline float at(std::uint16_t v){return float(v)*(1.f/65535.f)-.5f;}',
            '}', '']
    dest = os.path.join(SRC, 'world', 'WalkShapes.h')
    open(dest, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    print('%d shapes, %d boxes -> %s' % (len(rows), n, dest))

if __name__ == '__main__':
    main()
