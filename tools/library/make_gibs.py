"""Authored gibs: the pieces a prop breaks into, cut in Blender from the prop's own mesh, automatically.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --python tools/library/make_gibs.py -- [--only NAME,...] [--inspect NAME]
  python tools/library/register.py <the gib names it prints>
  python tools/library/make_gibs.py --table        (plain Python: src/world/GibSets.h and prop_physics.json)
  python tools/campaign/model_ids.py

Each RECIPES entry is one prop. A recipe names the mesh the engine draws the prop with (the pack's own file in
src/assets, or a library OBJ), the engine material key its faces use, its inner material (the colour of a fresh
cut) and its pieces. A piece is one of:
  islands=(x0, x1, y0, y1, z0, z1)  the source's own separate parts (loose islands of the pack's mesh: a chair's
                                    legs, seat and back) whose middles lie in that box of the prop's bounds
                                    (0..1 along the prop's x, its y (OBJ z) and up);
  rest=True                         every island no other islands= piece took;
  box=(x0, x1, y0, y1, z0, z1)      the mesh cut to that box with planes (bmesh bisect), each cut capped with
                                    the inner material;
and may add fracture=N (N-1 extra random plane cuts through it, seeded by the recipe: brittle glass, a screen)
and shape='cylinder' (its collision shape; a box otherwise).

Out (src/assets/library, gitignored; copy to main by hand): gib_<prop>_<piece>.obj, its .json sidecar (the
importer's fields plus "gib": source model, piece index, collision shape, mass share by volume, its middle's
offset and its size as fractions of the source's bounds along the prop's x, y, up) and gib_<prop>_inner.png.
Rerunnable: add a recipe line, run it again, register the new names.
"""
import json, math, os, random, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
SRC = os.path.join(ROOT, 'src')
OUT = os.path.join(SRC, 'assets', 'library')
MAIN_LIB = os.path.join(ROOT.split(os.sep + '.claude' + os.sep)[0], 'src', 'assets', 'library')

def lib(name):
    """A library file: this checkout's, else the main checkout's (a worktree lacks the gitignored library)."""
    here = os.path.join(OUT, name)
    return here if os.path.exists(here) else os.path.join(MAIN_LIB, name)

Q = 0.5
RECIPES = {
    # A slat-backed wooden chair: the pack's mesh has its legs, seat, rails and back as separate parts.
    'wooden_chair': dict(src='assets/facility/cargo/chair-wooden.fbx', material='wood_1', inner=(.80, .64, .44), grain=True, pieces=[
        dict(name='leg_a', islands=(0, .4, 0, .4, 0, .4)), dict(name='leg_b', islands=(.6, 1, 0, .4, 0, .4)),
        dict(name='leg_c', islands=(0, .3, Q, 1, 0, .75)), dict(name='leg_d', islands=(.7, 1, Q, 1, 0, .75)),
        dict(name='seat', islands=(0, 1, 0, 1, .38, .62)), dict(name='back', rest=True)]),
    'scrap_chair': dict(src='library:scrap_chair.obj', material='scrap_chair_chair', inner=(.55, .52, .48), grain=False, pieces=[
        dict(name='back', islands=(0, 1, 0, .2, .6, 1)), dict(name='seat', islands=(.3, .7, .4, .6, .4, .5)), dict(name='frame', rest=True)]),
    # Crates are one closed box each: cut into the planks of a hollow box (the lid, the floor, four sides, two
    # of them split along their boards), so what is left is its walls, not a solid block.
    'wooden_crate': dict(src='assets/facility/cargo/wooden-crate.obj', material='wooden_crate_8', inner=(.84, .70, .50), grain=True, pieces=[
        dict(name='lid', box=(0, 1, 0, 1, .9, 1)), dict(name='floor', box=(0, 1, 0, 1, 0, .1)),
        dict(name='side_a', box=(0, 1, 0, .1, .1, .5)), dict(name='side_b', box=(0, 1, 0, .1, .5, .9)),
        dict(name='side_c', box=(0, 1, .9, 1, .1, .5)), dict(name='side_d', box=(0, 1, .9, 1, .5, .9)),
        dict(name='end_a', box=(0, .1, .1, .9, .1, .9)), dict(name='end_b', box=(.9, 1, .1, .9, .1, .9))]),
    'packing_crate': dict(src='assets/facility/cargo/packing-crate.fbx', material='wooden_crate_hx_2_1', inner=(.86, .74, .55), grain=True, pieces=[
        dict(name='lid', box=(0, 1, 0, 1, .88, 1)), dict(name='floor', box=(0, 1, 0, 1, 0, .12)),
        dict(name='side_a', box=(0, 1, 0, .12, .12, .88)), dict(name='side_b', box=(0, 1, .88, 1, .12, .88)),
        dict(name='end_a', box=(0, .12, .12, .88, .12, .88)), dict(name='end_b', box=(.88, 1, .12, .88, .12, .88))]),
    # A glass bottle: the neck and shoulder whole, the body in random shards.
    'glass_bottle': dict(src='library:glass_bottle.obj', material='glass_bottle_glass_bottle_mx_1', inner='source', cap=False, seed=7, pieces=[
        dict(name='neck', box=(0, 1, 0, 1, .62, 1), shape='cylinder'), dict(name='shard', box=(0, 1, 0, 1, 0, .62), fracture=4)]),
    # A CRT monitor: the pack's mesh is its body and its stand; the body's front (its +z, model_fronts.json) is
    # cut off as the screen, cracked in three; the casing stays one piece.
    'crt_monitor': dict(src='assets/office/crt-monitor.obj', material='computer', inner=(.16, .16, .17), grain=False, seed=3, pieces=[
        dict(name='stand', islands=(0, 1, 0, 1, 0, .2)), dict(name='screen', box=(0, 1, .8, 1, .18, 1), fracture=3),
        dict(name='casing', box=(0, 1, 0, .8, .18, 1))]),
}


# ---------------------------------------------------------------------------------------------- Blender side
def blender_main(argv):
    import bpy, bmesh, mathutils
    only = None; inspect = None
    if '--only' in argv: only = set(argv[argv.index('--only') + 1].split(','))
    if '--inspect' in argv: inspect = argv[argv.index('--inspect') + 1]
    os.makedirs(OUT, exist_ok=True)
    made = []; capping = [True]

    def load(recipe):
        bpy.ops.wm.read_factory_settings(use_empty=True)
        src = recipe['src']
        path = lib(src[8:]) if src.startswith('library:') else os.path.join(SRC, src)
        low = path.lower()
        if low.endswith('.fbx'): bpy.ops.import_scene.fbx(filepath=path)
        elif low.endswith('.obj'): bpy.ops.wm.obj_import(filepath=path, forward_axis='NEGATIVE_Z', up_axis='Y')
        else: bpy.ops.import_scene.gltf(filepath=path)
        meshes = [o for o in bpy.data.objects if o.type == 'MESH']
        bm = bmesh.new()
        for o in meshes:
            me = o.data.copy(); me.transform(o.matrix_world); bm.from_mesh(me)
        # One material for the source (the engine keys the prop's sheet by it) and the inner one.
        for f in bm.faces: f.material_index = 0
        return bm, path

    def bounds(bm):
        xs = [v.co for v in bm.verts]
        lo = mathutils.Vector((min(v.x for v in xs), min(v.y for v in xs), min(v.z for v in xs)))
        hi = mathutils.Vector((max(v.x for v in xs), max(v.y for v in xs), max(v.z for v in xs)))
        return lo, hi

    def norm(co, lo, hi):
        # The prop's own axes, 0..1 over the source's bounds: x = Blender x, y = OBJ z = -Blender y, up = Blender z.
        s = hi - lo
        return ((co.x - lo.x) / max(s.x, 1e-6), (hi.y - co.y) / max(s.y, 1e-6), (co.z - lo.z) / max(s.z, 1e-6))

    def islands(bm):
        seen, out = set(), []
        for f in bm.faces:
            if f in seen: continue
            group, stack = [], [f]
            while stack:
                x = stack.pop()
                if x in seen: continue
                seen.add(x); group.append(x)
                for e in x.edges:
                    for g in e.link_faces:
                        if g not in seen: stack.append(g)
            out.append(group)
        return out

    def keep_faces(bm, keep):
        out = bm.copy()
        idx = {f.index for f in keep}
        out.faces.ensure_lookup_table()
        kill = [f for f in out.faces if f.index not in idx]
        bmesh.ops.delete(out, geom=kill, context='FACES')
        loose = [v for v in out.verts if not v.link_faces]
        bmesh.ops.delete(out, geom=loose, context='VERTS')
        return out

    def cap_uv(bm, faces, normal):
        uv = bm.loops.layers.uv.active or bm.loops.layers.uv.new()
        t = normal.orthogonal().normalized(); u = normal.cross(t).normalized()
        for f in faces:
            f.material_index = 1
            for l in f.loops:
                l[uv].uv = (l.vert.co.dot(t) * 3.0, l.vert.co.dot(u) * 3.0)

    def cut(bm, co, no):
        """Keep the side of the plane its normal points to; cap the cut with the inner material."""
        geom = bm.verts[:] + bm.edges[:] + bm.faces[:]
        r = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=co, plane_no=no, clear_inner=True, dist=1e-5)
        edges = [e for e in r['geom_cut'] if isinstance(e, bmesh.types.BMEdge)]
        if edges and capping[0]:
            try:
                made = bmesh.ops.holes_fill(bm, edges=edges, sides=0)['faces']
            except Exception:
                made = []
            cap_uv(bm, made, -no)
        return bm

    def clip(bm, box, lo, hi):
        s = hi - lo
        x0, x1, y0, y1, z0, z1 = box
        planes = []
        if x0 > 0: planes.append(((lo.x + s.x * x0, 0, 0), (1, 0, 0)))
        if x1 < 1: planes.append(((lo.x + s.x * x1, 0, 0), (-1, 0, 0)))
        # prop y runs along -Blender y: y0 is at hi.y - s.y * y0.
        if y0 > 0: planes.append(((0, hi.y - s.y * y0, 0), (0, -1, 0)))
        if y1 < 1: planes.append(((0, hi.y - s.y * y1, 0), (0, 1, 0)))
        if z0 > 0: planes.append(((0, 0, lo.z + s.z * z0), (0, 0, 1)))
        if z1 < 1: planes.append(((0, 0, lo.z + s.z * z1), (0, 0, -1)))
        out = bm.copy()
        for co, no in planes:
            out = cut(out, mathutils.Vector(co), mathutils.Vector(no))
        return out

    def middle(bm):
        n = max(1, len(bm.verts)); return sum((v.co for v in bm.verts), mathutils.Vector()) / n

    def fracture(bm, n, rng):
        parts = [bm]
        for k in range(n - 1):
            parts.sort(key=lambda p: -len(p.faces)); big = parts.pop(0)
            plo, phi = bounds(big); c = (plo + phi) / 2 + mathutils.Vector([rng.uniform(-.15, .15) * (phi[i] - plo[i]) for i in range(3)])
            no = mathutils.Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-.6, .6))).normalized()
            a = cut(big.copy(), c, no); b = cut(big.copy(), c, -no)
            parts += [p for p in (a, b) if len(p.faces)]
        return parts

    def inner_png(name, recipe):
        if recipe['inner'] == 'source': return None
        r, g, b = recipe['inner']; n = 32; img = bpy.data.images.new(name, n, n, alpha=False); px = []
        rng = random.Random(11)
        for y in range(n):
            for x in range(n):
                k = 1.0
                if recipe.get('grain'): k = .86 + .14 * math.sin(x * .9 + 2.2 * math.sin(y * .31)) + rng.uniform(-.04, .04)
                else: k = .92 + rng.uniform(-.08, .08)
                px += [min(1, r * k), min(1, g * k), min(1, b * k), 1.0]
        img.pixels = px; img.filepath_raw = os.path.join(OUT, name + '.png'); img.file_format = 'PNG'; img.save()
        return name + '.png'

    for prop, recipe in RECIPES.items():
        if only and prop not in only: continue
        if inspect and prop != inspect: continue
        bm, path = load(recipe)
        lo, hi = bounds(bm)
        groups = islands(bm)
        if inspect:
            print('INSPECT', prop, 'bounds', tuple(round(v, 3) for v in lo), tuple(round(v, 3) for v in hi))
            for i, gr in enumerate(groups):
                c = sum((f.calc_center_median() for f in gr), mathutils.Vector()) / len(gr)
                print('  island', i, 'faces', len(gr), 'middle', tuple(round(v, 2) for v in norm(c, lo, hi)))
            continue
        rng = random.Random(recipe.get('seed', 1)); capping[0] = recipe.get('cap', True)
        inner = 'gib_%s_inner' % prop if recipe['inner'] != 'source' else recipe['material']
        inner_file = inner_png(inner, recipe)
        taken = set(); pieces = []
        centres = [norm(sum((f.calc_center_median() for f in gr), mathutils.Vector()) / len(gr), lo, hi) for gr in groups]
        for spec in recipe['pieces']:
            if 'islands' in spec:
                x0, x1, y0, y1, z0, z1 = spec['islands']
                pick = [i for i, c in enumerate(centres) if i not in taken and x0 <= c[0] <= x1 and y0 <= c[1] <= y1 and z0 <= c[2] <= z1]
                if spec.get('smallest') and pick: pick = [min(pick, key=lambda i: len(groups[i]))]
                taken |= set(pick)
                parts = [keep_faces(bm, [f for i in pick for f in groups[i]])] if pick else []
            elif spec.get('rest'):
                pick = [i for i in range(len(groups)) if i not in taken]; taken |= set(pick)
                parts = [keep_faces(bm, [f for i in pick for f in groups[i]])] if pick else []
            else:
                parts = [clip(bm, spec['box'], lo, hi)]
            parts = [p for p in parts if len(p.faces)]
            if spec.get('fracture') and parts: parts = fracture(parts[0], spec['fracture'], rng)
            for k, p in enumerate(parts):
                pieces.append((spec['name'] + ('_%d' % (k + 1) if len(parts) > 1 else ''), p, spec.get('shape', 'box')))
        # Mass shares by each piece's bounding box (pack meshes are seldom closed, so their own volume is no guide).
        volumes = []
        for _, p, _ in pieces:
            plo, phi = bounds(p); volumes.append(max(1e-9, (phi.x - plo.x) * (phi.y - plo.y) * (phi.z - plo.z)))
        total = sum(volumes) or 1
        size = hi - lo
        for k, (name, p, shape) in enumerate(pieces):
            gib = 'gib_%s_%s' % (prop, name)
            plo, phi = bounds(p); c = (plo + phi) / 2; ps = phi - plo
            bmesh.ops.triangulate(p, faces=p.faces[:])
            bmesh.ops.translate(p, verts=p.verts[:], vec=-mathutils.Vector((c.x, c.y, plo.z)))
            me = bpy.data.meshes.new(gib); p.to_mesh(me)
            for mname in (recipe['material'], inner):
                mat = bpy.data.materials.get(mname) or bpy.data.materials.new(mname)
                if mat.name != mname: mat.name = mname
                me.materials.append(mat)
            ob = bpy.data.objects.new(gib, me); bpy.context.collection.objects.link(ob)
            bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
            obj = os.path.join(OUT, gib + '.obj')
            bpy.ops.wm.obj_export(filepath=obj, export_selected_objects=True, forward_axis='NEGATIVE_Z', up_axis='Y',
                                  export_materials=True, export_normals=True, export_uv=True, export_triangulated_mesh=True)
            mtl = os.path.join(OUT, gib + '.mtl')
            if os.path.exists(mtl): os.remove(mtl)
            mats = {inner: inner_file} if (inner_file and k == 0) else {}
            manifest = dict(name=gib, source=path.replace(os.sep, '/'), width=round(ps.x, 4), depth=round(ps.y, 4), height=round(ps.z, 4),
                            triangles=len(p.faces), materials=mats,
                            gib=dict(of=prop, piece=k, shape=shape, mass=round(volumes[k] / total, 4),
                                     offset=[round((c.x - (lo.x + hi.x) / 2) / size.x, 4), round(-(c.y - (lo.y + hi.y) / 2) / size.y, 4), round((c.z - (lo.z + hi.z) / 2) / size.z, 4)],
                                     size=[round(ps.x / size.x, 4), round(ps.y / size.y, 4), round(ps.z / size.z, 4)]))
            with open(os.path.join(OUT, gib + '.json'), 'w') as f: json.dump(manifest, f, indent=1)
            made.append(gib)
            bpy.data.objects.remove(ob, do_unlink=True)
        print('GIBS', prop, len(pieces), [n for n, _, _ in pieces])
    if made: print('MADE', ' '.join(made))


# ---------------------------------------------------------------------------------------------- the table
def table():
    catalog = json.load(open(os.path.join(OUT, 'catalog.json')))
    physics_path = os.path.join(ROOT, 'tools', 'campaign', 'prop_physics.json')
    physics = json.load(open(physics_path))
    sets = {}
    for name in sorted(catalog):
        if not name.startswith('gib_'): continue
        side = os.path.join(OUT, name + '.json')
        if not os.path.exists(side): side = lib(name + '.json')
        m = json.load(open(side)); g = m.get('gib')
        if not g: continue
        sets.setdefault(g['of'], []).append((g['piece'], name, g))
    rows, piece_rows, first = [], [], 0
    for prop in sorted(sets):
        pieces = sorted(sets[prop])
        src = physics.get(prop, {})
        # The prop's own mass, or the engine's guess from its size when the catalogue gives none (Clutter::mass).
        size = src.get('size') or [catalog[prop][k] for k in ('width', 'depth', 'height')] if prop in catalog else src.get('size', [.3, .3, .3])
        mass = src.get('mass', min(30., max(.05, 140 * size[0] * size[1] * size[2])))
        for _, name, g in pieces:
            piece_rows.append(' {model::%s,{%.4ff,%.4ff,%.4ff},{%.4ff,%.4ff,%.4ff},%s},' % (name, *g['offset'], *g['size'], 'true' if g['shape'] == 'cylinder' else 'false'))
            entry = physics.setdefault(name, {})
            if 'material' in src: entry['material'] = src['material']
            entry['mass'] = round(max(.02, mass * g['mass']), 3)
        rows.append(' {model::%s,%d,%d},' % (prop, first, len(pieces))); first += len(pieces)
    with open(physics_path, 'w', newline='\n') as f: json.dump(physics, f, indent=2); f.write('\n')
    out = ['#pragma once', '// Authored gibs: the pieces a prop breaks into (Game::breakClutter), each a library model cut from the',
           '// prop\'s own mesh by tools/library/make_gibs.py. Generated by `python tools/library/make_gibs.py --table`.',
           '// offset: the piece\'s middle from the prop\'s, size: its extent, both as fractions of the prop\'s dims (x, y, up).',
           '#include "ModelIds.h"', 'namespace retro::gibs {',
           'struct Piece {int model;float offset[3],size[3];bool cylinder;};',
           'struct Set {int model,first,count;};',
           'inline constexpr Piece pieces[]={'] + piece_rows + ['};', 'inline constexpr Set sets[]={'] + rows + ['};',
           '// The set a prop breaks into, or null (it is sliced as before).',
           'inline const Set* setOf(int model){for(const auto& s:sets)if(s.model==model)return &s;return nullptr;}',
           '// A piece model\'s entry (its collision shape), or null.',
           'inline const Piece* pieceOf(int model){for(const auto& p:pieces)if(p.model==model)return &p;return nullptr;}', '}']
    open(os.path.join(SRC, 'world', 'GibSets.h'), 'w', newline='\n').write('\n'.join(out) + '\n')
    print('GibSets.h: %d sets, %d pieces' % (len(rows), len(piece_rows)))


if __name__ == '__main__':
    if '--table' in sys.argv: table()
    else: blender_main(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])
