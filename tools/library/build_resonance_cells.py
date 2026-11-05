"""The resonance cell: the thing the plant was built to fill, and the reason
everyone down here is dying. Kit-built from the purchased packs' own textures.

A heavy cell in machined steel: chamfered collars top and bottom, four bars
seated into both, a hazard band, violet seams lighting the window, and a
latched terminal lid on top with two folding grips and two binding posts,
+ in red and - in black, that a test lead's plug seats on (no taller than the
bolts on the boss, so the cell keeps its catalog size). Inside the bars a
pane of protective glass (a *_glass material: the engine blends it by its
alpha), and behind it the held resonance hangs on its own: a dark, twisted mass veined with light,
a few motes loose around it. You see it from every side and it has depth.

  anl_cell_charged  full: the mass in the window, the seams lit
  anl_cell_spent    drained: the window empty, a soot stain, the seams dead
  anl_cell_cap      the top collar pulled off: lid, latch, grips, bar stubs
  anl_cell_core     the bottom collar and bars, opened, the stain in it

OBJ + MTL + 128 px textures go to src/assets/library/_cells/ (gitignored with
the rest of the library), ready for import_model.py; GLB copies for preview.

  python tools/library/build_resonance_cells.py          (textures; PIL + numpy)
  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b \\
    --python tools/library/build_resonance_cells.py      (geometry)

Materials named *_glow light themselves in the engine from their bright texels,
so only the veins, motes and seams glow; the dark of the mass stays dark.
"""
import os, math
try:
    import bpy, bmesh
except ImportError:
    bpy = None

PACK = 'O:/retro official'
OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'src', 'assets', 'library', '_cells'))
os.makedirs(OUT, exist_ok=True)
S = 128
PSX2 = 'PSX Textures II v1.6/PSX Textures II/256/Color Maps/'


def textures():
    import numpy as np
    from PIL import Image, ImageOps

    def load(src, size=S):
        return Image.open(os.path.join(PACK, src)).convert('RGB').resize((size, size), Image.LANCZOS)

    def save(a, name):
        Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).save(os.path.join(OUT, name + '.png'))

    def grade(src, lo, hi):
        return np.asarray(ImageOps.colorize(ImageOps.autocontrast(ImageOps.grayscale(load(src))), lo, hi), float)

    # Machined steel for the collars (the pack's dark panel steel, blued), polished bar for the ribs.
    save(grade(PSX2 + 'metal_panels_pt_4.png', (16, 18, 22), (128, 136, 146)), 'cell_metal')
    save(grade('Props/Metal/Materials/metal_bar.png', (40, 44, 50), (200, 206, 212)), 'cell_rib')
    save(grade('Props/Metal/Materials/metal_army_tank_lid.png', (24, 26, 30), (190, 194, 200)), 'cell_lid')
    # Hazard band: diagonal stripes in the decal's own yellow, with grime over them.
    decal = np.asarray(load('Props/Misc/Materials/decal_yellow_stripes_01.png'), float)
    yellow = np.median(decal[(decal[..., 0] > 150) & (decal[..., 2] < 90)], axis=0)
    yy, xx = np.mgrid[0:S, 0:S].astype(float)
    # the band wraps .63 m of circumference into u and only its .026 m height into v,
    # so a 45-degree stripe in metres is near-vertical in the texture
    stripe = ((((xx / S) * .63 + (yy / S) * .026) // .022) % 2)[..., None]
    grime = np.asarray(ImageOps.grayscale(load('Props/Misc/Materials/grunge.png')), float)[..., None] / 255
    save((stripe * yellow + (1 - stripe) * np.array([22, 22, 20])) * (.7 + .3 * grime), 'cell_hazard')

    # The mass: the pack's reservoir liquid folded on itself, nearly black, with thin veins of
    # violet-white light where the folds cross one level.
    liquid = np.asarray(ImageOps.autocontrast(ImageOps.grayscale(load(
        'Environments/Industrial_exterior_v1/Industrial_exterior_v2/Textures/Liquid_reservoir_1.png'))), float) / 255
    u, v = xx / S, yy / S
    twist = ((u * S + np.sin(v * math.pi * 2) * 20 + v * 64).astype(int)) % S
    rise = ((v * S + np.sin(u * math.pi * 4) * 12).astype(int)) % S
    field = (liquid[rise, twist] + liquid[(rise * 2) % S, (twist + 40) % S] * .7) / 1.7
    ridge = np.clip(1 - np.abs(field - .52) * 9, 0, 1) ** 2
    dark = np.stack([6 + field * 16, 3 + field * 6, 12 + field * 26], -1)
    vein = np.array([196, 150, 255])
    save(dark * (1 - ridge[..., None]) + vein * ridge[..., None], 'cell_mass_glow')
    # soot where a drained mass sat
    save(np.stack([10 + liquid * 20] * 3, -1) * np.array([1, .95, 1.05]), 'cell_soot')
    # Glass: a cold tint you see the mass through (alpha is opacity), two highlight streaks
    # down it, frosted rims where it seats in the collars, a few scuffs.
    def glass(broken):
        a = np.full((S, S), 22.)
        for c, w, k in ((.18, .04, 120), (.25, .01, 190), (.68, .025, 70)):
            a = np.maximum(a, np.clip(1 - np.abs(u - c) / w, 0, 1) * k)
        rim = np.clip(np.maximum(.1 - v, v - .9) / .1, 0, 1)
        a = np.maximum(a, rim * 170)
        scuff = np.asarray(ImageOps.grayscale(load('Props/Misc/Materials/grunge.png')), float) / 255
        a = np.maximum(a, (scuff < .12) * 80)
        rgb = np.stack([200 + 0 * u, 225 + 0 * u, 235 + 0 * u], -1) * (.8 + .2 * scuff[..., None])
        if broken:                                    # a jagged break: nothing above the edge
            edge = .45 + .25 * np.abs(np.sin(u * math.pi * 7)) + .12 * np.sin(u * math.pi * 23)
            a = np.where(v < edge, 0, np.maximum(a, 70))
            a = np.where(np.abs(v - edge) < .025, 230, a)
        Image.fromarray(np.dstack([rgb, a]).clip(0, 255).astype(np.uint8), 'RGBA').save(os.path.join(OUT, ('cell_shard_glass' if broken else 'cell_glass') + '.png'))
    glass(False)
    glass(True)
    # seams and motes: violet light; the same seam dead
    band = np.clip(1 - np.abs(v - .5) * 2.2, 0, 1)[..., None]
    save(np.concatenate([140 + band * 110, 60 + band * 160, 225 + band * 30], -1), 'cell_seam_glow')
    save(np.concatenate([26 + band * 10, 24 + band * 8, 32 + band * 12], -1), 'cell_seam_dead')
    # The terminal posts' insulators: moulded red (+) and black (-), the bar's polish as a sheen.
    save(grade('Props/Metal/Materials/metal_bar.png', (92, 10, 8), (226, 52, 38)), 'cell_pos')
    save(grade('Props/Metal/Materials/metal_bar.png', (12, 12, 14), (78, 78, 84)), 'cell_neg')


R = .1                            # collar radius
POST = .048                       # the terminal posts stand this far either side of the lid's centre,
                                  # + toward -x: the engine's lead ports (World.cpp leadPorts) match
BOT, WIN, TOP = .075, .23, .065   # bottom collar, window, top collar heights


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def material(name, image):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nodes = m.node_tree.nodes
    t = nodes.new('ShaderNodeTexImage')
    t.image = bpy.data.images.load(os.path.join(OUT, image + '.png'))
    m.node_tree.links.new(t.outputs['Color'], nodes['Principled BSDF'].inputs['Base Color'])
    if name.endswith('glass'):                                               # previewers honour the alpha
        m.node_tree.links.new(t.outputs['Alpha'], nodes['Principled BSDF'].inputs['Alpha'])
        try:
            m.surface_render_method = 'BLENDED'
        except Exception:
            pass
    return m


def cyl(r, h, z, mat, verts=20, x=0., y=0., r2=None):
    bpy.ops.mesh.primitive_cone_add(vertices=verts, radius1=r, radius2=r if r2 is None else r2, depth=h, location=(x, y, z + h / 2))
    o = bpy.context.object
    o.data.materials.append(mat)
    return o


def box(w, d, h, x, y, z, rot, mat):
    bpy.ops.mesh.primitive_cube_add(size=1, location=(x, y, z + h / 2))
    o = bpy.context.object
    o.scale = (w, d, h)
    o.rotation_euler = (0, 0, rot)
    o.data.materials.append(mat)
    return o


def disc(r, z, mat, verts=24):
    """A flat disc facing up with its texture projected straight down onto it."""
    mesh = bpy.data.meshes.new('disc')
    bm = bmesh.new()
    vs = [bm.verts.new((math.cos(a * 2 * math.pi / verts) * r, math.sin(a * 2 * math.pi / verts) * r, z)) for a in range(verts)]
    face = bm.faces.new(vs)
    uv = bm.loops.layers.uv.new()
    for loop in face.loops:
        loop[uv].uv = (loop.vert.co.x / (2 * r) + .5, loop.vert.co.y / (2 * r) + .5)
    bm.to_mesh(mesh)
    o = bpy.data.objects.new('disc', mesh)
    bpy.context.collection.objects.link(o)
    o.data.materials.append(mat)
    return o


def save(name):
    # one mesh, so each material is written (and imported) once
    bpy.ops.object.select_all(action='SELECT')
    bpy.context.view_layer.objects.active = [o for o in bpy.context.scene.objects if o.type == 'MESH'][0]
    bpy.ops.object.join()
    path = os.path.join(OUT, name + '.obj')
    bpy.ops.wm.obj_export(filepath=path, export_selected_objects=False, export_materials=True,
                          path_mode='STRIP', forward_axis='NEGATIVE_Z', up_axis='Y')
    print('wrote', path)


def bottom(m, seam):
    cyl(R * .9, .014, 0, m['metal'], r2=R)                                   # chamfered foot
    cyl(R, BOT - .014, .014, m['metal'])
    cyl(R * 1.006, .026, .03, m['hazard'])                                   # hazard band
    cyl(R * .94, .01, BOT, seam)                                             # the lower seam
    cyl(R * .86, .004, BOT + .006, m['metal'])                               # the window floor


def bars(m, z0, h):
    for a in range(4):                                                       # seated into both collars
        t = a * math.pi / 2 + math.pi / 4
        box(.03, .026, h, math.cos(t) * R * .9, math.sin(t) * R * .9, z0, t, m['rib'])
        for zz in (z0 + .012, z0 + h - .024):                                # the bolts that hold them
            cyl(.006, .012, zz, m['metal'], 6, x=math.cos(t) * R * 1.005, y=math.sin(t) * R * 1.005)


def top(m, z, seam):
    cyl(R * .94, .01, z, seam)                                               # the upper seam
    cyl(R, TOP - .012, z + .01, m['metal'])
    cyl(R, .012, z + TOP - .002, m['metal'], r2=R * .84)                     # chamfer
    lid = z + TOP + .01
    disc(R * .84, lid + .0005, m['lid'])                                     # the latched lid
    cyl(.026, .014, lid, m['rib'])                                           # the terminal boss
    for a in range(3):
        t = a * 2 * math.pi / 3
        cyl(.005, .012, lid + .014, m['metal'], 6, x=math.cos(t) * .016, y=math.sin(t) * .016)
    for s, mat in ((-1, m['pos']), (1, m['neg'])):                           # the terminal posts, + and -
        x = s * POST
        cyl(.012, .006, lid, mat, 12, x=x, r2=.0105)                         # the insulating collar
        cyl(.0078, .004, lid + .006, m['rib'], 6, x=x)                       # the hex nut that holds it
        cyl(.0085, .011, lid + .01, mat, 12, x=x, r2=.0075)                  # the cap a plug seats on
        cyl(.004, .004, lid + .021, m['rib'], 8, x=x)                        # its socket's rim, flush at .405
    for s in (-1, 1):                                                        # two folding grips
        box(.012, .05, .012, s * R * 1.02, 0, z + TOP * .3, 0, m['rib'])
        for e in (-1, 1):
            box(.016, .01, .024, s * R * .99, e * .022, z + TOP * .3 - .006, 0, m['metal'])


def mass(m):
    """The held resonance: an elongated, lumpy, twisted body hanging free in the window."""
    bpy.ops.mesh.primitive_uv_sphere_add(segments=14, ring_count=12, radius=.044, location=(0, 0, BOT + WIN / 2 + .006))
    o = bpy.context.object
    o.scale = (1, 1, 1.85)
    bpy.ops.object.transform_apply(scale=True)
    tex = bpy.data.textures.new('lumps', 'CLOUDS')
    tex.noise_scale = .028
    d = o.modifiers.new('lumps', 'DISPLACE')
    d.texture = tex
    d.strength = .03
    d.mid_level = .5
    t = o.modifiers.new('twist', 'SIMPLE_DEFORM')
    t.deform_method = 'TWIST'
    t.angle = math.radians(230)
    t.deform_axis = 'Z'
    bpy.ops.object.convert(target='MESH')
    o.data.materials.append(m['mass_glow'])
    for x, y, z in [(.062, .03, .2), (-.05, -.055, .12), (.02, -.068, .26), (-.066, .03, .17), (.045, .05, .1)]:
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=.006, location=(x, y, z))
        bpy.context.object.data.materials.append(m['seam_glow'])            # loose light around it


def soot(m):
    bpy.ops.mesh.primitive_uv_sphere_add(segments=12, ring_count=6, radius=.06, location=(0, 0, BOT + .01))
    o = bpy.context.object
    o.scale = (1, 1, .12)
    o.data.materials.append(m['soot'])


def kit():
    return {k: material(k, 'cell_' + k) for k in ('metal', 'rib', 'lid', 'hazard', 'mass_glow', 'soot', 'seam_glow', 'seam_dead', 'glass', 'shard_glass', 'pos', 'neg')}


def pane(m, z, h, key='glass'):
    cyl(R * .86, h, z, m[key], 24)                                           # the protective glass, inside the bars


def geometry():
    reset(); m = kit()
    bottom(m, m['seam_glow']); bars(m, BOT - .02, WIN + .04); top(m, BOT + WIN, m['seam_glow']); mass(m); pane(m, BOT + .01, WIN - .01)
    save('anl_cell_charged')
    reset(); m = kit()
    bottom(m, m['seam_dead']); bars(m, BOT - .02, WIN + .04); top(m, BOT + WIN, m['seam_dead']); soot(m); pane(m, BOT + .01, WIN - .01)
    save('anl_cell_spent')
    reset(); m = kit()                                                       # the top collar, pulled
    top(m, 0, m['seam_dead']); bars(m, -.035, .045); pane(m, -.04, .05, 'shard_glass')
    save('anl_cell_cap')
    reset(); m = kit()                                                       # the opened body, no top
    bottom(m, m['seam_dead']); bars(m, BOT - .02, WIN + .02); soot(m); pane(m, BOT + .01, WIN * .55, 'shard_glass')
    save('anl_cell_core')


if bpy:
    geometry()
else:
    textures()
