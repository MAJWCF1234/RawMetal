"""Resonance crystal: the ore the plant was built to draw on, grown out through Containment
where the fields failed. Modelled here, not boxed together in the map builders.

Each cluster is a knot of six-sided prisms with pyramid tips, leaning out of one root at
different heights, the root a lump of the pack's own rock sunk into the floor. The crystal
finish is a *_glow material: the engine lights it from its bright texels, so a prism is dark
and cold at its foot and burns toward its tip, and every facet reads separately because each
takes the full width of a texture with its own bright edge.

  res_crystal_l     the big knot (1.3 m across, the leader 2.3 m)
  res_crystal_m     a middle knot (0.8 m across, 1.2 m)
  res_crystal_s     a scatter of short shards (0.45 m across, 0.5 m)
  res_crystal_wall  a knot grown out of a wall: its root flat on the wall plane, prisms toward +y

OBJ + MTL + 128 px textures go to src/assets/library/_crystals/ (gitignored with the rest
of the library), ready for import_model.py.

  python tools/library/build_resonance_crystals.py          (textures; PIL + numpy)
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b \\
    --python tools/library/build_resonance_crystals.py      (geometry)
"""
import os, math, random
try:
    import bpy, bmesh
except ImportError:
    bpy = None

PACK = 'O:/retro official'
OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src', 'assets', 'library', '_crystals'))
os.makedirs(OUT, exist_ok=True)
S = 128
PSX2 = 'PSX Textures II v1.6/PSX Textures II/256/Color Maps/'
SHAFT = .78          # v at which a prism's shaft meets its tip


def textures():
    import numpy as np
    from PIL import Image, ImageOps, ImageFilter

    def load(src, size=S):
        return Image.open(os.path.join(PACK, src)).convert('RGB').resize((size, size), Image.LANCZOS)

    def save(a, name, w=S, h=S):
        Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).resize((w, h), Image.LANCZOS).save(os.path.join(OUT, name + '.png'))

    yy, xx = np.mgrid[0:S, 0:S].astype(float)
    u, v = xx / S, 1 - yy / S                                  # v = 0 at a prism's foot, 1 at its point
    # Inclusions: the pack's rock, blurred and stretched along the prism, as clouds inside the stone.
    rock = np.asarray(ImageOps.autocontrast(ImageOps.grayscale(load(PSX2 + 'rock_pt_2.png').filter(ImageFilter.GaussianBlur(2)))), float) / 255
    cloud = rock[(yy.astype(int) // 3 + xx.astype(int)) % S, xx.astype(int)]
    # Light rises up the prism: deep teal at the foot, cyan through the body, near-white at the point.
    # The engine lights a *_glow texel itself once its luma passes .28 (fully at .68) and adds that on
    # top of the room's light, so the shaft is kept under the threshold (deep teal, lit by the room),
    # the upper shaft just over it and the point well over it, in a saturated cyan that does not
    # blow out to white.
    rise = np.clip(v / SHAFT, 0, 1) ** 1.8
    body = (np.array([3, 14, 20])[None, None] * (1 - rise[..., None]) + np.array([16, 92, 112])[None, None] * rise[..., None])
    tip = np.clip((v - SHAFT) / (1 - SHAFT), 0, 1)[..., None]
    col = body * (1 - tip) + np.array([52, 196, 226])[None, None] * tip
    # each facet: a bright leading edge, a cool trailing edge, a thin internal fracture line
    edge = np.clip(1 - u / .07, 0, 1) ** 2 * .9 + np.clip((u - .93) / .07, 0, 1) ** 2 * .35
    fracture = np.clip(1 - np.abs(u - (.42 + .18 * np.sin(v * 7))) / .012, 0, 1) * .5 * rise
    col = col * (.8 + .3 * cloud[..., None]) + np.array([36, 120, 140])[None, None] * (edge + fracture)[..., None] * (.3 + .7 * rise[..., None])
    save(col, 'crys_body_glow')
    # The same stone gone dead: grey-blue, unlit, for broken shards on the floor
    dead = np.stack([30 + 60 * cloud, 38 + 66 * cloud, 46 + 72 * cloud], -1) * (1 + .6 * edge[..., None])
    save(dead, 'crys_dead')
    # The root: the pack's rock, lifted a little so it does not render as a black lump.
    root = np.asarray(ImageOps.colorize(ImageOps.autocontrast(ImageOps.grayscale(load(PSX2 + 'rock_pt_2.png'))), (34, 32, 30), (168, 160, 150)), float)
    # where the crystal comes out of it the rock is stained and wet, a faint cyan bleed
    save(root, 'crys_root')
    stain = np.clip(1 - np.hypot(u - .5, v - .5) * 2.2, 0, 1)[..., None]
    save(root * (1 - .5 * stain) + np.array([20, 80, 96])[None, None] * stain, 'crys_seam_glow')
    # The bedrock the deep levels are cut into, for the outcrops: the pack's rock at 256 px, lifted out
    # of black so a face away from the lamps still reads as stone.
    face = ImageOps.colorize(ImageOps.autocontrast(ImageOps.grayscale(load(PSX2 + 'rock_pt_4.png', 256)), cutoff=1), (66, 62, 56), (226, 218, 204))
    Image.fromarray(np.asarray(face)).save(os.path.join(OUT, 'rock_face.png'))
    lift_cave_rocks()


def lift_cave_rocks(target=84.):
    """The cave pack's stones (cave_*_rp_rocks) come in near black (mean luma ~46) and render as black
    blobs anywhere a lamp is not close. Lift their imported textures to the outcrops' level by a
    gamma that lands the mean on `target`; a texture already there is left alone, so this can rerun."""
    import glob
    import numpy as np
    from PIL import Image
    for root in (os.path.dirname(OUT), os.path.join(os.path.dirname(OUT), '..', '..', '..', 'tools', 'Level Editor', 'src', 'assets', 'library')):
        for path in glob.glob(os.path.join(root, 'cave_*rp_rocks*.jpg')):
            a = np.asarray(Image.open(path).convert('RGB'), float) / 255
            mean = a.mean() * 255
            if abs(mean - target) < 4:
                continue
            lo, hi = 0., 1.
            for _ in range(30):                                    # gamma that lands the mean on target
                g = (lo + hi) / 2
                if (a ** g).mean() * 255 > target: lo = g
                else: hi = g
            Image.fromarray(np.clip((a ** g) * 255, 0, 255).astype(np.uint8)).save(path, quality=95)
            print('lifted', os.path.basename(path), round(mean, 1), '->', target)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, image):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nodes = m.node_tree.nodes
    t = nodes.new('ShaderNodeTexImage')
    t.image = bpy.data.images.load(os.path.join(OUT, image + '.png'))
    m.node_tree.links.new(t.outputs['Color'], nodes['Principled BSDF'].inputs['Base Color'])
    return m


def kit():
    return {k: material(k, 'crys_' + k) for k in ('body_glow', 'dead', 'root', 'seam_glow')}


def prism(base, axis, r, length, mat, sides=6, spin=0.):
    """A six-sided crystal: a shaft of `length` along `axis` from `base`, a pyramid point on it.
    Every facet takes the texture's full width, the shaft v 0..SHAFT, the point SHAFT..1."""
    ax = axis
    n = math.sqrt(sum(c * c for c in ax))
    ax = [c / n for c in ax]
    ref = (0, 0, 1) if abs(ax[2]) < .9 else (1, 0, 0)
    a = [ax[1] * ref[2] - ax[2] * ref[1], ax[2] * ref[0] - ax[0] * ref[2], ax[0] * ref[1] - ax[1] * ref[0]]
    an = math.sqrt(sum(c * c for c in a)); a = [c / an for c in a]
    b = [ax[1] * a[2] - ax[2] * a[1], ax[2] * a[0] - ax[0] * a[2], ax[0] * a[1] - ax[1] * a[0]]
    point_len = r * 2.6
    shaft = max(length - point_len, r)
    taper = .86                                              # a prism narrows a little toward its point

    def ring(t, rr):
        out = []
        for i in range(sides):
            ang = spin + i * 2 * math.pi / sides
            out.append(tuple(base[k] + ax[k] * t + (a[k] * math.cos(ang) + b[k] * math.sin(ang)) * rr for k in range(3)))
        return out
    lo, hi = ring(-.05, r), ring(shaft, r * taper)
    apex = tuple(base[k] + ax[k] * (shaft + point_len) for k in range(3))
    mesh = bpy.data.meshes.new('prism')
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new()
    for i in range(sides):
        j = (i + 1) % sides
        f = bm.faces.new([bm.verts.new(lo[i]), bm.verts.new(lo[j]), bm.verts.new(hi[j]), bm.verts.new(hi[i])])
        for loop, (uu, vv) in zip(f.loops, ((0, 0), (1, 0), (1, SHAFT), (0, SHAFT))):
            loop[uv].uv = (uu, vv)
        t = bm.faces.new([bm.verts.new(hi[i]), bm.verts.new(hi[j]), bm.verts.new(apex)])
        for loop, (uu, vv) in zip(t.loops, ((0, SHAFT), (1, SHAFT), (.5, 1))):
            loop[uv].uv = (uu, vv)
    bm.normal_update()
    bm.to_mesh(mesh)
    o = bpy.data.objects.new('prism', mesh)
    bpy.context.collection.objects.link(o)
    o.data.materials.append(mat)
    return o


def lump(x, y, z, sx, sy, sz, mat, seed):
    """A lump of the pack's rock, displaced so it reads as broken stone, not a ball."""
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=1, location=(x, y, z))
    o = bpy.context.object
    o.scale = (sx, sy, sz)
    bpy.ops.object.transform_apply(scale=True)
    tex = bpy.data.textures.new('rock%d' % seed, 'CLOUDS')
    tex.noise_scale = .35
    d = o.modifiers.new('rough', 'DISPLACE')
    d.texture = tex
    d.strength = min(sx, sy, sz) * .45
    d.mid_level = .5
    bpy.ops.object.convert(target='MESH')
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.cube_project(cube_size=1.2)
    bpy.ops.object.mode_set(mode='OBJECT')
    o.data.materials.append(mat)
    return o


def cluster(m, seed, spread, leader, count, lean, wall=False):
    rnd = random.Random(seed)
    # the root: one broad lump and two smaller, sunk so only their crowns show
    if wall:
        lump(0, .02, spread * .45, spread * .55, spread * .18, spread * .5, m['root'], seed)
    else:
        lump(0, 0, -spread * .18, spread * .62, spread * .55, spread * .3, m['root'], seed)
        for k in range(2):
            ang = rnd.uniform(0, 2 * math.pi)
            lump(math.cos(ang) * spread * .45, math.sin(ang) * spread * .45, -spread * .1, spread * .25, spread * .22, spread * .16, m['root'], seed + k + 1)
    prisms = [(0., leader, .0)] + [(rnd.uniform(.25, 1.), leader * rnd.uniform(.22, .75), rnd.uniform(.25, 1.) * lean) for _ in range(count - 1)]
    for i, (out, length, tilt) in enumerate(prisms):
        ang = rnd.uniform(0, 2 * math.pi) if i else 0
        r = max(.025, length * rnd.uniform(.07, .1)) if i else leader * .085
        if wall:
            # out of the wall plane (y = 0) toward +y, fanning in x and z
            base = (math.cos(ang) * spread * .4 * out, 0, spread * .45 + math.sin(ang) * spread * .35 * out)
            axis = (math.cos(ang) * tilt, 1, math.sin(ang) * tilt + .25)
        else:
            base = (math.cos(ang) * spread * .38 * out, math.sin(ang) * spread * .38 * out, 0)
            axis = (math.cos(ang) * tilt, math.sin(ang) * tilt, 1)
        prism(base, axis, r, length, m['body_glow'], spin=rnd.uniform(0, 1))
    # a couple of broken shards lying at the foot, dead
    if not wall:
        for _ in range(2):
            ang = rnd.uniform(0, 2 * math.pi); d = spread * rnd.uniform(.45, .7)
            prism((math.cos(ang) * d, math.sin(ang) * d, .02), (math.cos(ang + 1.2), math.sin(ang + 1.2), .12), .03, leader * .18, m['dead'])


def save(name):
    bpy.ops.object.select_all(action='SELECT')
    bpy.context.view_layer.objects.active = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
    bpy.ops.object.join()
    path = os.path.join(OUT, name + '.obj')
    bpy.ops.wm.obj_export(filepath=path, export_selected_objects=False, export_materials=True,
                          path_mode='STRIP', forward_axis='NEGATIVE_Z', up_axis='Y')
    print('wrote', path)


def outcrop(name, seed, sx, sy, sz, taper):
    """A broken outcrop of bedrock standing on the floor: a sphere pushed about by two scales of
    noise and then collapsed to a few hundred planes, so it breaks into flat faces the way cut
    rock does instead of reading as a ball or as folded card. `taper` narrows it toward its crown."""
    reset()
    mat = material('rock_face', 'rock_face')
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, radius=1, location=(0, 0, 0))
    o = bpy.context.object
    o.scale = (sx, sy, sz)
    bpy.ops.object.transform_apply(scale=True)
    me = o.data
    for v in me.vertices:
        t = min(max((v.co.z + sz) / (2 * sz), 0), 1)
        k = 1 - taper * t
        v.co.x = v.co.x * k + taper * .45 * sx * t * t      # a tall one leans off its foot
        v.co.y *= k
    for i, (scale, amount) in enumerate(((.7, .55), (.22, .14))):
        tex = bpy.data.textures.new('%s%d' % (name, i), 'CLOUDS')
        tex.noise_scale = scale
        tex.noise_depth = 2
        d = o.modifiers.new('n%d' % i, 'DISPLACE')
        d.texture = tex
        d.strength = min(sx, sy) * amount
        d.mid_level = .5
        d.texture_coords = 'GLOBAL'
        tex.intensity = 1
        o.location = (seed * 3.7, seed * 1.3, 0)          # each seed takes a different patch of noise
    bpy.ops.object.modifier_apply(modifier='n0')
    bpy.ops.object.modifier_apply(modifier='n1')
    o.location = (0, 0, 0)
    dec = o.modifiers.new('facets', 'DECIMATE')
    dec.ratio = .09
    bpy.ops.object.modifier_apply(modifier='facets')
    # cut off flat where it meets the floor, a little of it below so it sits in rather than on
    for v in o.data.vertices:
        if v.co.z < -sz * .35:
            v.co.z = -sz * .35
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.uv.cube_project(cube_size=1.6)
    bpy.ops.object.mode_set(mode='OBJECT')
    o.data.materials.append(mat)
    save(name)


def geometry():
    # outcrops: a broad crag, a tall spire, a low shelf of rock
    outcrop('res_rock_crag', 3, 1.0, .8, .9, .35)
    outcrop('res_rock_spire', 5, .55, .5, 1.6, .6)
    outcrop('res_rock_low', 9, 1.1, .9, .5, .2)
    for name, args in (('res_crystal_l', (7, 1.3, 2.3, 11, .55)), ('res_crystal_m', (19, .8, 1.2, 8, .6)),
                       ('res_crystal_s', (31, .45, .5, 6, .8)), ('res_crystal_wall', (43, 1., 1.1, 9, .5))):
        reset(); m = kit()
        cluster(m, *args, wall=name.endswith('wall'))
        save(name)


if bpy:
    geometry()
else:
    textures()
