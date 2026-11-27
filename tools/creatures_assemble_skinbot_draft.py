import bpy, bmesh, sys, os, math
from mathutils import Vector, Matrix

src, out = sys.argv[sys.argv.index('--') + 1:]
bpy.ops.wm.read_factory_settings(use_empty=True)
image = bpy.data.images.load(os.path.join(out, 'base_glow.png'))
mat = bpy.data.materials.new('skinbot')
mat.use_nodes = True
tex = mat.node_tree.nodes.new('ShaderNodeTexImage')
tex.image = image
mat.node_tree.links.new(tex.outputs['Color'], mat.node_tree.nodes['Principled BSDF'].inputs['Base Color'])
mat.node_tree.nodes.active = tex


def load(n, name):
    bpy.ops.wm.obj_import(filepath=os.path.join(src, 'model_%d.obj' % n))
    o = bpy.context.selected_objects[0]
    o.name = name
    o.data.materials.clear()
    o.data.materials.append(mat)
    o.data.transform(o.matrix_world)
    o.matrix_world = Matrix.Identity(4)
    return o


body, eyes, teeth, limbs = load(0, 'body'), load(1, 'eyes'), load(2, 'teeth'), load(3, 'limbs')
# The head, eyes and teeth share one space; turn them upright (head high, tail trailing back along +y).
mid_body = sum((v.co for v in body.data.vertices), Vector()) / len(body.data.vertices)
pitch = Matrix.Translation(mid_body) @ Matrix.Rotation(math.radians(-70), 4, 'X') @ Matrix.Translation(-mid_body)
raw = [v.co.copy() for v in body.data.vertices]   # sockets are found before the pitch, along the body's own length
for o in (body, eyes, teeth):
    o.data.transform(pitch)

# The limb mesh holds two arms and two rear legs: part it into the four by loose pieces.
bm = bmesh.new()
bm.from_mesh(limbs.data)
bm.verts.ensure_lookup_table()
seen, pieces = set(), []
for v in bm.verts:
    if v.index in seen:
        continue
    stack, piece = [v], []
    seen.add(v.index)
    while stack:
        a = stack.pop()
        piece.append(a.index)
        for e in a.link_edges:
            b = e.other_vert(a)
            if b.index not in seen:
                seen.add(b.index)
                stack.append(b)
    pieces.append(piece)
centres = [sum((bm.verts[i].co for i in p), Vector()) / len(p) for p in pieces]
mid_x = (min(c.x for c in centres) + max(c.x for c in centres)) / 2
zs = sorted(c.z for c in centres)
gap = max(range(len(zs) - 1), key=lambda i: zs[i + 1] - zs[i])
mid_z = (zs[gap] + zs[gap + 1]) / 2
groups = {}
for p, c in zip(pieces, centres):
    groups.setdefault(('rear' if c.z > mid_z else 'arm', 'a' if c.x < mid_x else 'b'), set()).update(p)
bm.free()
print('PIECES', len(pieces), {k: len(v) for k, v in groups.items()})


def part(key, name):
    o = limbs.copy()
    o.data = limbs.data.copy()
    o.name = name
    bpy.context.scene.collection.objects.link(o)
    m = bmesh.new()
    m.from_mesh(o.data)
    m.verts.ensure_lookup_table()
    bmesh.ops.delete(m, geom=[v for v in m.verts if v.index not in groups[key]], context='VERTS')
    m.to_mesh(o.data)
    m.free()
    return o


# The sockets: where the body is widest in a band along it (the bolt heads).
bv = raw
y0, y1 = min(v.y for v in bv), max(v.y for v in bv)


def socket(lo, hi, sign):
    band = [v for v in bv if lo <= (v.y - y0) / (y1 - y0) <= hi and v.x * sign > 0]
    far = max(abs(v.x) for v in band)
    ring = [v for v in band if abs(v.x) > far - 0.02]
    return pitch @ (sum(ring, Vector()) / len(ring))


# Each leg is bent at its own joints. Joint places were read off a flat side render of the raw leg (pixels of an
# 800 px orthographic view: its centre cy, cz and width s), root first. The wanted directions are the user's side
# reference, as (forward, up) of each section; forward is -y here. Each leg then turns out about the vertical.
LEG = {
    'arm': dict(view=(0.61140, -0.51990, 1.36911), joints=[(45, 102), (388, 260), (392, 486), (768, 642)],
                want=[(32, -174), (90, 36), (48, -179)], splay=34),
    'rear': dict(view=(0.94573, 0.84175, 0.91260), joints=[(178, 750), (278, 622), (179, 421), (665, 30)],
                 want=[(-45, -39), (-62, 29), (-73, -157)], splay=30),
}


def flat(kind, px, py):
    cy, cz, s = LEG[kind]['view']
    return Vector((cy + (px - 400) * s / 800, cz - (py - 400) * s / 800))


def off_segment(p, a, b):
    t = max(0.0, min(1.0, (p - a).dot(b - a) / (b - a).length_squared))
    return (p - (a + (b - a) * t)).length


def bend(o, kind, sign, socket_at, extra=0.0):
    spec = LEG[kind]
    joints = [flat(kind, *j) for j in spec['joints']]
    m = bmesh.new()
    m.from_mesh(o.data)
    m.verts.ensure_lookup_table()
    done, section = set(), {}
    for v in m.verts:   # a loose piece (a plate, a rod, a bolt) stays whole and goes with the section it lies along
        if v.index in done:
            continue
        stack, piece = [v], []
        done.add(v.index)
        while stack:
            a = stack.pop()
            piece.append(a.index)
            for e in a.link_edges:
                b = e.other_vert(a)
                if b.index not in done:
                    done.add(b.index)
                    stack.append(b)
        c = sum((m.verts[i].co for i in piece), Vector()) / len(piece)
        here = Vector((c.y, c.z))
        k = min(range(3), key=lambda n: off_segment(here, joints[n], joints[n + 1]))
        for i in piece:
            section[i] = k
    m.free()
    across = sum(v.co.x for v in o.data.vertices) / len(o.data.vertices)
    turn, at = [], [Vector((0.0, 0.0))]
    for k in range(3):
        was = joints[k + 1] - joints[k]
        f, u = spec['want'][k]
        wanted = Vector((-f, u))
        angle = math.atan2(wanted.y, wanted.x) - math.atan2(was.y, was.x) + (extra if k == 1 else 0.0)
        turn.append(angle)
        at.append(at[k] + Vector((was.x * math.cos(angle) - was.y * math.sin(angle), was.x * math.sin(angle) + was.y * math.cos(angle))))
    for v in o.data.vertices:
        k = section[v.index]
        r = Vector((v.co.y, v.co.z)) - joints[k]
        c, n = math.cos(turn[k]), math.sin(turn[k])
        q = at[k] + Vector((r.x * c - r.y * n, r.x * n + r.y * c))
        v.co = Vector((v.co.x - across, q.x, q.y))
    yaw = math.radians(spec['splay']) * (sign if kind == 'arm' else -sign)
    place = Matrix.Translation(socket_at + Vector((0.02 * sign, 0, 0))) @ Matrix.Rotation(yaw, 4, 'Z')
    o.data.transform(place)
    # For the rig: each section its own material (same texture), and where the joints ended up, root to tip.
    o.data.materials.clear()
    for k in range(3):
        name = '%s_%d' % (o.name, k)
        m = bpy.data.materials.get(name) or mat.copy()
        m.name = name
        o.data.materials.append(m)
    for face in o.data.polygons:
        face.material_index = section[face.vertices[0]]
    o['joints'] = [c for q in at for c in (place @ Vector((0.0, q.x, q.y)))]


made = []
for sign, side in ((-1, 'a'), (1, 'b')):
    shoulder = socket(0.18, 0.55, sign)
    hip = socket(0.80, 1.0, sign)
    print('SOCKETS', sign, 'shoulder', [round(x, 2) for x in shoulder], 'hip', [round(x, 2) for x in hip])
    arm = part(('arm', side), 'arm_' + ('L' if sign < 0 else 'R'))
    bend(arm, 'arm', sign, shoulder)
    floor_z = min(v.co.z for v in arm.data.vertices)
    # The back blades keep the reference's backward angle; the knee link lifts or drops until the foot stands
    # on the same ground as the front claws.
    best = None
    for step in range(-80, 81):
        trial = part(('rear', side), 'trial')
        bend(trial, 'rear', sign, hip, math.radians(step))
        miss = abs(min(v.co.z for v in trial.data.vertices) - floor_z) + abs(step) * 0.00002
        if best is None or miss < best[0]:
            best = (miss, step)
        bpy.data.meshes.remove(trial.data)
    rear = part(('rear', side), 'rear_' + ('L' if sign < 0 else 'R'))
    bend(rear, 'rear', sign, hip, math.radians(best[1]))
    print('FEET', sign, 'front', round(floor_z, 3), 'back', round(min(v.co.z for v in rear.data.vertices), 3), 'knee link turned', best[1])
    made += [arm, rear]
bpy.data.objects.remove(limbs)
body_mat = mat.copy()
body_mat.name = 'body'
for o in (body, eyes, teeth):
    o.data.materials.clear()
    o.data.materials.append(body_mat)

everything = [body, eyes, teeth] + made
floor = min(v.co.z for o in made for v in o.data.vertices)
for o in everything:
    o.data.transform(Matrix.Translation((0, 0, -floor)))
for o in made:
    o['joints'] = [c - (floor if i % 3 == 2 else 0.0) for i, c in enumerate(o['joints'])]
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
shade = scene.display.shading
shade.light, shade.color_type, shade.show_cavity = 'STUDIO', 'TEXTURE', True
scene.render.resolution_x, scene.render.resolution_y = 700, 560
pts = [v.co for o in everything for v in o.data.vertices]
lo = Vector([min(p[i] for p in pts) for i in range(3)])
hi = Vector([max(p[i] for p in pts) for i in range(3)])
print('BOUNDS', [round(x, 2) for x in hi - lo])
mid, size = (lo + hi) / 2, max(hi - lo)
cam = bpy.data.cameras.new('c')
cam.type, cam.ortho_scale = 'ORTHO', size * 1.5
co = bpy.data.objects.new('c', cam)
scene.collection.objects.link(co)
scene.camera = co
for name, deg, lift in (('front34', 28, 0.18), ('front', 0, 0.05), ('side', -90, 0.08), ('top', 0, 6.0)):
    a = math.radians(deg)
    d = Vector((math.sin(a), -math.cos(a), lift)).normalized()
    co.location = mid + d * 8
    co.rotation_euler = (-d).to_track_quat('-Z', 'Y').to_euler()
    scene.render.filepath = os.path.join(out, 'asm_%s.png' % name)
    bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out, 'skinbot-assembly.blend'))
