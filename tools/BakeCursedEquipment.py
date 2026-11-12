"""Build the cursed equipment's runtime FBX and texture.

  blender -b --python tools/BakeCursedEquipment.py -- testenemy.glb cursed.fbx cursed.png
  blender -b --python tools/BakeCursedEquipment.py -- --sheets cursed.fbx OUTDIR

then

  RawMetalAuthoredCreatureBake cursed.fbx cursed-animation.bin Cursed_Idle Cursed_Walk Cursed_Attack Cursed_Pain Cursed_Death

The pack (O:/retro official/cursed equipment.zip, source/testenemy.glb) is a
rusted pressure vessel on two mismatched legs: a heavy piston leg in a boot and
a thin hooked strut it leans on like a crutch, pipe arms, and a pole over its
top from which a lantern hangs on a chain in front of it. It is a 14-bone rig
whose parts are rigid (each parented to a bone) but for one skinned hose mesh,
with a stray Icosphere at the origin that is dropped here.

The pack faces +Y (boot toe, knee, lantern and its attack all point +Y), so the
whole thing is turned half round to face -Y like every engine creature, scaled
by .30 (2.2 m to the top of the pole at rest) and set down so the boot's sole
is the floor (z 0). Its parts are re-bound to the armature as rigid vertex
groups, the lantern's lens gets a bone of its own ('Lamp', so it can flicker
out), and five clips are keyed at 30 fps, every frame, from poses composed in
world space and written back as local bone keys:

  Cursed_Idle    2.5 s loop: the pack's settled idlePhys stand (the plain idle is a
                 half-second stab of the strut that does not loop), loaded 7 cm
                 lower on its legs, with a pressure
                 heave twice a loop, a servo twitch (the vessel jerks round and
                 drops, the pipes flick, the lamp gutters), the strut tapping
                 the floor, the lantern swinging on its chain
  Cursed_Walk    1.4 s loop, keyed here: the pack's walk is a pole-vault hop
                 airborne for most of its cycle (walkPhys the same with the
                 strut dangling). This one is a limp: the boot stamps down and
                 carries it, the vessel heaves up over it, the strut is swung
                 through and planted, and the whole weight falls onto it with a
                 lurch and sag to the strut side while the piston leg is
                 dragged forward toe-down. Planted feet slide back the 1.0 m the
                 game moves it each cycle (gaitPerMetre 6.28)
  Cursed_Attack  1.0 s: the pack's attackPhys (better than the plain take: the
                 lantern goes over the top on its chain as a flail and the chain
                 follows through). It rears to .25, the lantern is furthest out
                 (1.6 m ahead) at .27 and smashes down at .33, the contact; it
                 is back in its crouch by .6. The strut's hook, which digs into
                 the floor in the take, is lifted out of it
  Cursed_Pain    .6 s, keyed here: a jolt back, pipes flung, the lamp flickers
                 out twice, the strut slams down to catch a stagger, recover
  Cursed_Death   1.8 s, keyed here (the pack's death and deathPhys only slump
                 where it stands, twisting half round): a convulsion with the
                 lamp guttering, the piston knee gives, it pitches forward onto
                 its face over the lantern at .66, a bounce, the strut kicked up
                 behind it, and the lamp goes out. Every frame is set down on
                 the floor, as the bake does with a death clip

The texture is the GLB's own sheet (the loose textures/body_0.png is flipped),
its gutters padded from the islands outward (texels no UV triangle covers, so
the dark rust inside islands is kept), the lamp's lens brightened so its every
texel is above the *_glow threshold (luma .28), and halved to 512 px.
"""
import bpy, sys, math, os
import numpy as np
from mathutils import Matrix, Vector, Quaternion

args = sys.argv[sys.argv.index('--') + 1:]
SCALE = .30
FPS = 30


def ease(w):
    w = min(1., max(0., w)); return w * w * (3 - 2 * w)


def ramp(t, t0, t1):
    return ease((t - t0) / max(1e-6, t1 - t0))


def bump(t, t0, t1, t2, t3):
    """0 before t0, up to 1 by t1, held, back to 0 from t2 to t3."""
    return ramp(t, t0, t1) * (1 - ramp(t, t2, t3))


def T(v):
    return Matrix.Translation(Vector(v))


def about(pivot, rot):
    """A world rotation (3x3 or Quaternion) about pivot, as a 4x4."""
    m = rot.to_matrix().to_4x4() if isinstance(rot, Quaternion) else rot.to_4x4()
    return T(pivot) @ m @ T(-Vector(pivot))


def euler(x=0., y=0., z=0.):
    """Degrees about the world X (pitch: + tips the top toward the front, -Y),
    Y (roll: + tips the top toward +X) and Z (yaw)."""
    return (Matrix.Rotation(math.radians(z), 3, 'Z') @ Matrix.Rotation(math.radians(y), 3, 'Y')
            @ Matrix.Rotation(math.radians(x), 3, 'X'))


# ============================================================================== sheets
if args[0] == '--sheets':
    fbx, outdir = os.path.abspath(args[1]), os.path.abspath(args[2])
    os.makedirs(outdir, exist_ok=True)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    sc.render.fps = FPS
    bpy.ops.import_scene.fbx(filepath=fbx)
    arm = next(o for o in sc.objects if o.type == 'ARMATURE')
    meshes = [o for o in sc.objects if o.type == 'MESH']
    clips = ['Cursed_Idle', 'Cursed_Walk', 'Cursed_Attack', 'Cursed_Pain', 'Cursed_Death']
    acts = {c: next(a for a in bpy.data.actions if a.name.endswith(c) or ('|' + c) in a.name) for c in clips}
    ad = arm.animation_data or arm.animation_data_create()
    for tr in list(ad.nla_tracks):
        ad.nla_tracks.remove(tr)
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'TEXTURE'
    sc.view_settings.view_transform = 'Standard'
    sc.view_settings.exposure = 1.3
    sc.world = bpy.data.worlds.new('w')
    sc.render.resolution_x, sc.render.resolution_y = 400, 380
    sc.render.film_transparent = False

    def solid(name, verts, faces, colour):
        me = bpy.data.meshes.new(name); me.from_pydata(verts, [], faces)
        ob = bpy.data.objects.new(name, me); sc.collection.objects.link(ob)
        mat = bpy.data.materials.new(name); mat.diffuse_color = colour; me.materials.append(mat)
        return ob

    def box(name, lo, hi, colour):
        (x0, y0, z0), (x1, y1, z1) = lo, hi
        v = [(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0), (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]
        f = [(0, 1, 2, 3), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        return solid(name, v, f, colour)

    # Floor, the 2.1 m door head as a red bar, and a 1.8 m figure beside it.
    solid('floor', [(-6, -6, 0), (6, -6, 0), (6, 6, 0), (-6, 6, 0)], [(0, 1, 2, 3)], (.22, .22, .24, 1))
    box('doorhead', (-2.4, -2.4, 2.095), (2.4, 2.4, 2.105), (.9, .1, .1, 1))
    fig = (.25, .45, .7, 1)
    FX, FY = -1.3, 1.05
    box('fig_legs', (FX - .17, FY - .1, 0), (FX + .17, FY + .1, .86), fig)
    box('fig_body', (FX - .22, FY - .12, .86), (FX + .22, FY + .12, 1.5), fig)
    box('fig_neck', (FX - .06, FY - .06, 1.5), (FX + .06, FY + .06, 1.58), fig)
    box('fig_head', (FX - .1, FY - .11, 1.58), (FX + .1, FY + .11, 1.8), fig)
    for o in sc.objects:
        if o.type == 'MESH' and o not in meshes:
            o.color = o.data.materials[0].diffuse_color
    sc.display.shading.color_type = 'TEXTURE'   # creature textured; the helpers fall back to their material colour

    cams = {}
    for name, loc, rz in (('side', (12, -.4, 1.2), 90), ('front', (-.35, -12, 1.2), 0)):
        c = bpy.data.objects.new(name, bpy.data.cameras.new(name)); sc.collection.objects.link(c)
        c.data.type = 'ORTHO'; c.data.ortho_scale = 3.6
        c.location = loc; c.rotation_euler = (math.radians(90), 0, math.radians(rz)); cams[name] = c
    label = bpy.data.objects.new('label', bpy.data.curves.new('label', 'FONT')); sc.collection.objects.link(label)
    label.data.size = .2
    lmat = bpy.data.materials.new('label'); lmat.diffuse_color = (1, .9, .2, 1); label.data.materials.append(lmat)

    COLS = 8
    stats = {}
    dg = None
    for view, cam in cams.items():
        sc.camera = cam
        if view == 'side':
            label.location = (0, -2.1, 2.62); label.rotation_euler = (math.radians(90), 0, math.radians(90))
        else:
            label.location = (-2.05, 0, 2.62); label.rotation_euler = (math.radians(90), 0, 0)
        rows = []
        for clip in clips:
            act = acts[clip]; ad.action = act
            if act.slots:
                ad.action_slot = act.slots[0]
            f0, f1 = act.frame_range
            row = []
            for i in range(COLS):
                fr = f0 + (f1 - f0) * i / (COLS - 1)
                sc.frame_set(int(math.floor(fr)), subframe=fr - math.floor(fr))
                label.data.body = '%s  %.2f' % (clip.split('_')[1], i / (COLS - 1))
                if view == 'side':
                    dg = bpy.context.evaluated_depsgraph_get()
                    lo = Vector((1e9,) * 3); hi = -lo
                    for o in meshes:
                        e = o.evaluated_get(dg); m = e.matrix_world
                        for v in e.data.vertices:
                            w = m @ v.co; lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
                    s = stats.setdefault(clip, [Vector((1e9,) * 3), Vector((-1e9,) * 3), []])
                    s[0] = Vector(map(min, s[0], lo)); s[1] = Vector(map(max, s[1], hi)); s[2].append((round(lo.z, 3), round(hi.z, 3)))
                path = os.path.join(outdir, '_%s_%s_%d.png' % (view, clip, i))
                sc.render.filepath = path
                bpy.ops.render.render(write_still=True)
                img = bpy.data.images.load(path)
                px = np.array(img.pixels[:], dtype=np.float32).reshape(img.size[1], img.size[0], 4)
                bpy.data.images.remove(img); os.remove(path)
                row.append(px)
            rows.append(np.concatenate(row, axis=1))
        sheet = np.concatenate(rows[::-1], axis=0)        # pixel rows run bottom-up
        h, w = sheet.shape[:2]
        im = bpy.data.images.new('sheet_' + view, w, h, alpha=True)
        im.pixels = sheet.ravel()
        im.filepath_raw = os.path.join(outdir, 'cursed_%s.png' % view); im.file_format = 'PNG'; im.save()
        print('SHEET', im.filepath_raw)
    for clip, (lo, hi, per) in stats.items():
        print('STATS %-14s x %.2f..%.2f  y %.2f..%.2f  z %.3f..%.3f  frames(min,max z) %s' % (clip, lo.x, hi.x, lo.y, hi.y, lo.z, hi.z, per))
    sys.exit(0)

# ============================================================================== build
glb, out_fbx, out_png = (os.path.abspath(a) for a in args[:3])   # Blender resolves relative paths oddly
bpy.ops.wm.read_factory_settings(use_empty=True)
sc = bpy.context.scene
sc.render.fps = FPS                    # the takes come in at 30 frames a second of their own time
bpy.ops.import_scene.gltf(filepath=glb)
for o in list(sc.objects):
    if o.type == 'MESH' and o.parent is None:          # the stray Icosphere
        bpy.data.objects.remove(o, do_unlink=True)
arm = next(o for o in sc.objects if o.type == 'ARMATURE')
arm.name = 'CursedEquipment'; arm.data.name = 'CursedRig'
parts = [o for o in sc.objects if o.type == 'MESH']

# ---- the floor: the boot's sole in the settled stand -------------------------------------------
STAND = ('idlePhys', 120)
arm.animation_data.action = bpy.data.actions[STAND[0]]
arm.animation_data.action_slot = bpy.data.actions[STAND[0]].slots[0]
sc.frame_set(STAND[1])
dg = bpy.context.evaluated_depsgraph_get()
boot = next(o for o in parts if o.parent_bone == 'Bone.003')
e = boot.evaluated_get(dg)
sole = min((e.matrix_world @ v.co).z for v in e.data.vertices)
arm.animation_data.action = None

# ---- rest-space parts, re-bound to the armature as rigid vertex groups -------------------------
arm.data.pose_position = 'REST'
bpy.context.view_layer.update()
for o in parts:
    mw = o.matrix_world.copy()
    bone = o.parent_bone if o.parent_type == 'BONE' else None
    o.parent = None
    o.data.transform(mw)
    o.matrix_world = Matrix.Identity(4)
    if bone:
        o.vertex_groups.new(name=bone).add(range(len(o.data.vertices)), 1.0, 'REPLACE')
        o.modifiers.new('Rig', 'ARMATURE').object = arm
FIX = Matrix.Rotation(math.pi, 4, 'Z') @ Matrix.Scale(SCALE, 4) @ T((0, 0, -sole))
for o in parts:
    o.data.transform(FIX)
arm.data.transform(FIX)
arm.matrix_world = Matrix.Identity(4)
for o in parts:
    o.parent = arm; o.matrix_parent_inverse = Matrix.Identity(4)
    for m in o.modifiers:
        if m.type == 'ARMATURE':
            m.object = arm
# Location keys are in bone space: they shrink with the rig; rotations are untouched.
for a in bpy.data.actions:
    for layer in a.layers:
        for strip in layer.strips:
            for bag in strip.channelbags:
                for fc in bag.fcurves:
                    if fc.data_path.endswith('location'):
                        for k in fc.keyframe_points:
                            k.co[1] *= SCALE; k.handle_left[1] *= SCALE; k.handle_right[1] *= SCALE

# ---- the lamp's own bone ------------------------------------------------------------------------
lamp = next(o for o in parts if o.name == 'light')
lamp_centre = sum((v.co for v in lamp.data.vertices), Vector()) / len(lamp.data.vertices)
lamp_normal = (lamp.data.polygons[0].normal).normalized()
bpy.context.view_layer.objects.active = arm
bpy.ops.object.mode_set(mode='EDIT')
eb = arm.data.edit_bones.new('Lamp')
eb.head = lamp_centre; eb.tail = lamp_centre + lamp_normal * .08
eb.parent = arm.data.edit_bones['Bone.011']
bpy.ops.object.mode_set(mode='OBJECT')
lamp.vertex_groups.clear()
lamp.vertex_groups.new(name='Lamp').add(range(len(lamp.data.vertices)), 1.0, 'REPLACE')
arm.data.pose_position = 'POSE'
for pb in arm.pose.bones:
    pb.rotation_mode = 'QUATERNION'

# ---- materials -------------------------------------------------------------------------------
for o in parts:
    for i, m in enumerate(o.data.materials):
        if m and m.name == 'base':
            m.name = 'cursed_body'
        elif m and m.name == 'light':
            m.name = 'cursed_lamp_glow'

BONES = [b.name for b in arm.data.bones]
REST = {b.name: b.matrix_local.copy() for b in arm.data.bones}
PARENT = {b.name: (b.parent.name if b.parent else None) for b in arm.data.bones}


def below(name):
    """name and every bone under it."""
    out = [name]
    for b in BONES:
        p = PARENT[b]
        while p:
            if p == name:
                out.append(b); break
            p = PARENT[p]
    return out


BODY = below('Bone')                         # everything but the stray IK handle Bone.013
PISTON = ('Bone.001', 'Bone.002')            # thigh, piston shin; Bone.003 the boot
STRUT = ('Bone.004', 'Bone.005')
CHAIN = below('Bone.010')                    # chain, lantern, lamp
LEN = {b.name: b.length for b in arm.data.bones}

# Rigid parts' vertices in their bone's rest space, for floor contact checks.
RIGID = []
for o in parts:
    if len(o.vertex_groups) == 1:
        b = o.vertex_groups[0].name
        inv = REST[b].inverted()
        RIGID.append((b, o.name, np.array([(inv @ v.co)[:] + (1.,) for v in o.data.vertices])))


def points(pose, only=None):
    out = []
    for b, name, loc in RIGID:
        if only and name not in only:
            continue
        m = np.array(pose[b])
        out.append(loc @ m.T)
    return np.concatenate(out)[:, :3]


def lowest(pose, only=None):
    p = points(pose, only)
    return p[np.argmin(p[:, 2])]


STRUT_MESH = next(o.name for o in parts if o.vertex_groups and o.vertex_groups[0].name == 'Bone.005' and len(o.vertex_groups) == 1)
BOOT_MESH = boot.name


def pack(action, frame):
    a = bpy.data.actions[action]
    for pb in arm.pose.bones:                    # the Lamp has no curves in the takes
        pb.location, pb.rotation_quaternion, pb.scale = (0, 0, 0), (1, 0, 0, 0), (1, 1, 1)
    arm.animation_data.action = a; arm.animation_data.action_slot = a.slots[0]
    sc.frame_set(int(math.floor(frame)), subframe=frame - math.floor(frame))
    pose = {pb.name: pb.matrix.copy() for pb in arm.pose.bones}
    arm.animation_data.action = None
    return pose


def move(pose, bones, X):
    for b in bones:
        pose[b] = X @ pose[b]


def tail(pose, b):
    return pose[b] @ Vector((0, LEN[b], 0))


def solve(pose, chain, end_local, target, hint):
    """Two-bone solve: turn the chain's upper and lower bones (and all under them) so that the
    point end_local (in the lower bone's space) lands on target, the knee pushed toward hint."""
    up, lo = chain
    H = pose[up].translation.copy(); K0 = pose[lo].translation.copy(); E0 = pose[lo] @ end_local
    a = (K0 - H).length; b = (E0 - K0).length
    dv = target - H
    d = min(max(dv.length, abs(a - b) + 1e-4), a + b - 1e-4)
    u = dv.normalized()
    v = hint - u * hint.dot(u)
    v = v.normalized() if v.length > 1e-6 else Vector((0, -1, 0))
    ca = (a * a + d * d - b * b) / (2 * a * d); sa = math.sqrt(max(0., 1 - ca * ca))
    K = H + u * (a * ca) + v * (a * sa)
    move(pose, below(up), about(H, (K0 - H).rotation_difference(K - H)))
    K1 = pose[lo].translation.copy(); E1 = pose[lo] @ end_local
    move(pose, below(lo), about(K1, (E1 - K1).rotation_difference(target - K1)))


def ankle_local():
    return Vector((0, LEN['Bone.002'], 0))


def body(pose, D, feet=None, hints=None):
    """Move the vessel (and all on it) by the world transform D, then put the boot's ankle and
    the strut's contact point back where they were (or on feet[...]) and the boot flat as it was
    (or as feet['boot_rot'] turns it about the ankle)."""
    feet = feet or {}
    ankle0 = tail(pose, 'Bone.002')
    boot0 = pose['Bone.003'].copy()
    strut_end = feet.get('strut_local') or pose['Bone.005'].inverted() @ Vector(lowest(pose, {STRUT_MESH}))
    strut0 = pose['Bone.005'] @ strut_end
    knee_p = pose['Bone.002'].translation - ankle0
    knee_s = pose['Bone.005'].translation - strut0
    move(pose, BODY, D)
    R = D.to_3x3()
    A = feet.get('ankle', ankle0)
    solve(pose, PISTON, ankle_local(), A, (hints or {}).get('piston', R @ knee_p))
    solve(pose, STRUT, strut_end, feet.get('strut', strut0), (hints or {}).get('strut', R @ knee_s))
    rot = feet.get('boot_rot', Matrix.Identity(3))
    pose['Bone.003'] = T(A) @ rot.to_4x4() @ T(-ankle0) @ boot0
    return pose


def swing(pose, fore=0., side=0.):
    """The chain and lantern swung about the pole's tip: fore + swings the lantern toward the
    front (-Y), side + toward +X."""
    pivot = pose['Bone.010'].translation.copy()
    move(pose, CHAIN, about(pivot, euler(x=-fore, y=side)))


def lamp_scale(pose, k):
    c = pose['Lamp'].translation.copy()
    pose['Lamp'] = T(c) @ Matrix.Scale(max(k, .02), 4) @ T(-c) @ pose['Lamp']


def flick(pose, bone, deg, axis='X'):
    h = pose[bone].translation.copy()
    move(pose, below(bone), about(h, Matrix.Rotation(math.radians(deg), 3, axis)))


def keep_strut_up(pose):
    """Lift the strut's hook out of the floor where a take drives it in."""
    for _ in range(3):
        p = lowest(pose, {STRUT_MESH})
        if p[2] >= -1e-4:
            return
        local = pose['Bone.005'].inverted() @ Vector(p)
        target = Vector((p[0], p[1], 0.))
        hint = pose['Bone.005'].translation - Vector(p)
        solve(pose, STRUT, local, target, hint)


def report(name, t, p):
    low = min(((float((m @ np.array(p[b]).T)[:, 2].min()), n) for b, n, m in RIGID), key=lambda q: q[0])
    if '--verbose' in sys.argv or low[0] < -.005:
        print('CURSED %s %.2f lowest %s %.3f top %.2f' % (name, t, low[1], low[0], float(points(p)[:, 2].max())))


def keep_boot_up(pose):
    """Lift the boot (the piston leg re-solved to its ankle) out of the floor where a take sinks it."""
    low = float(lowest(pose, {BOOT_MESH})[2])
    if low >= -1e-3:
        return
    ankle = tail(pose, 'Bone.002'); boot = pose['Bone.003'].copy()
    knee = pose['Bone.002'].translation - ankle
    solve(pose, PISTON, ankle_local(), ankle + Vector((0, 0, -low)), knee)
    pose['Bone.003'] = T((0, 0, -low)) @ boot


def copy(pose):
    return {k: v.copy() for k, v in pose.items()}


# ---- keys ---------------------------------------------------------------------------------------
clips = {}


def key(action, frame, pose, prev):
    arm.animation_data.action = action
    for pb in arm.pose.bones:
        n = pb.name
        rest = REST[n]
        if PARENT[n]:
            basis = rest.inverted() @ REST[PARENT[n]] @ pose[PARENT[n]].inverted() @ pose[n]
        else:
            basis = rest.inverted() @ pose[n]
        loc, rot, sca = basis.decompose()
        if n in prev and prev[n].dot(rot) < 0:
            rot = -rot
        prev[n] = rot
        pb.location = loc; pb.rotation_quaternion = rot; pb.scale = sca
        for path in ('location', 'rotation_quaternion', 'scale'):
            pb.keyframe_insert(path, frame=frame)


def clip(name, frames, pose_at):
    action = bpy.data.actions.new(name); action.use_fake_user = True
    prev = {}
    for f in range(1, frames + 1):
        key(action, f, pose_at((f - 1) / (frames - 1)), prev)
    clips[name] = (action, 1, frames)
    arm.animation_data.action = None


S = math.sin; C = math.cos; TAU = 2 * math.pi
base = pack(*STAND)
# Loaded: the take's stand settled 7 cm lower on its legs and leaning in, so it reads as weight
# ready to move and its idle top stays under a 2.1 m door head.
body(base, T((0, 0, -.07)) @ about(base['Bone'].translation, euler(x=3)))
BASE_STRUT = lowest(base, {STRUT_MESH})
BASE_STRUT_LOCAL = base['Bone.005'].inverted() @ Vector(BASE_STRUT)
PIVOT = base['Bone'].translation.copy()            # the vessel's centre
ANKLE = tail(base, 'Bone.002')
print('CURSED stand: ankle', ANKLE, 'strut contact', BASE_STRUT, 'vessel', PIVOT)


# ---- Idle ---------------------------------------------------------------------------------------
def idle(t):
    p = copy(base)
    heave = .5 - .5 * C(2 * TAU * t)                                   # two pump strokes a loop
    twitch = ramp(t, .40, .42) * (1 - ramp(t, .52, .66))
    jolt = ramp(t, .40, .415) * (1 - ramp(t, .415, .47))
    tap = bump(t, .70, .75, .76, .84)                                 # the strut down to the floor and back
    D = (T((0, .004 * twitch, .012 * heave - .022 * twitch - .01 * jolt))
         @ about(PIVOT, euler(x=-1.4 * heave + 3 * jolt, y=1.2 * S(TAU * t) - 2 * twitch, z=7 * twitch)))
    strut = Vector(BASE_STRUT) + Vector((0, -.03 * tap, -BASE_STRUT[2] * tap))
    body(p, D, {'strut': strut, 'strut_local': BASE_STRUT_LOCAL})
    flick(p, 'Bone.006', 18 * jolt - 4 * twitch, 'X'); flick(p, 'Bone.007', -12 * jolt, 'X')
    flick(p, 'Bone.008', 10 * twitch, 'Z')
    # The lantern hangs still but for a slow sway, knocked swinging by the twitch.
    after = max(0., t - .41)
    knock = 16 * S(TAU * after / .3) * math.exp(-after * 7) if t > .41 else 0.
    swing(p, fore=3 * S(TAU * t) + knock, side=2.5 * C(TAU * t) + .4 * knock)
    lamp_scale(p, 1 - .85 * bump(t, .415, .425, .44, .45) - .6 * bump(t, .48, .49, .5, .51))
    keep_strut_up(p)
    report('idle', t, p)
    return p


clip('Cursed_Idle', 76, idle)

# ---- Walk: a limp -------------------------------------------------------------------------------
CYCLE = 1.0                         # metres the game moves it each loop: gaitPerMetre = 2 pi / CYCLE
BOOT_DOWN = .62                     # the boot is on the floor 62% of the loop (it carries it)
STRUT_ON, STRUT_OFF = .40, .92      # the strut is planted from .40 to .92
CROUCH = .07                        # below the loaded stand
BOOT_X, STRUT_X = ANKLE.x, BASE_STRUT[0] + .03
BOOT_Y0, STRUT_Y0 = ANKLE.y - .04, BASE_STRUT[1] + .10


def boot_on(t):
    """The ankle's (y, lift) and the boot's pitch at walk phase t: it lands (stamps) at 0 ahead of
    the hip, slides back with the floor, then is dragged forward toe-down and slammed down."""
    span = CYCLE * BOOT_DOWN
    if t < BOOT_DOWN:
        return BOOT_Y0 - span / 2 + span * (t / BOOT_DOWN), 0., 0.
    s = (t - BOOT_DOWN) / (1 - BOOT_DOWN)
    y = BOOT_Y0 + span / 2 - span * ease(s * 1.1 - .05)
    lift = .11 * S(math.pi * min(1., s * 1.15)) ** .8
    pitch = 22 * S(math.pi * min(1., s * 1.3)) ** 1.5                   # the toe hangs, dragged
    return y, lift, pitch


def strut_on(t):
    span = CYCLE * (STRUT_OFF - STRUT_ON)
    if STRUT_ON <= t < STRUT_OFF:
        return STRUT_Y0 - span / 2 + span * ((t - STRUT_ON) / (STRUT_OFF - STRUT_ON)), 0.
    s = ((t - STRUT_OFF) % 1) / (1 - (STRUT_OFF - STRUT_ON))
    return STRUT_Y0 + span / 2 - span * ease(s), .16 * S(math.pi * s)


def walk(t):
    p = copy(base)
    by, blift, bpitch = boot_on(t)
    sy, slift = strut_on(t)
    # Weight: on the boot from its stamp it heaves up over it; the strut's plant at .40 takes
    # the weight with a lurch and the vessel sags onto it until the boot comes through.
    stamp = bump(t, -.03, 0., .05, .2) + bump(t, .97, 1., 1.05, 1.2)      # the boot lands at 0
    over = bump(t, .05, .25, .3, .42)
    catch = ramp(t, STRUT_ON - .01, STRUT_ON + .04) * (1 - ramp(t, .84, .99))
    jolt = bump(t, STRUT_ON, STRUT_ON + .025, STRUT_ON + .04, STRUT_ON + .15)
    sag = ramp(t, STRUT_ON + .04, .78) * (1 - ramp(t, .84, .99))
    drop = -CROUCH - .06 * stamp + .045 * over - .05 * jolt - .07 * sag
    sway = -.05 * over + .08 * catch                                   # toward the leg it stands on
    pitch = 6 + 8 * stamp - 4 * over + 7 * jolt - 2 * sag              # always leaning into it
    roll = -8 * over + 11 * catch + 6 * jolt + 3 * sag
    yaw = 7 * S(TAU * t + .6)
    D = T((sway, -.02 * stamp, drop)) @ about(PIVOT, euler(x=pitch, y=roll, z=yaw))
    feet = {'ankle': Vector((BOOT_X, by, ANKLE.z + blift)),
            'strut': Vector((STRUT_X, sy, slift)), 'strut_local': BASE_STRUT_LOCAL,
            'boot_rot': Matrix.Rotation(math.radians(bpitch), 3, 'X')}
    body(p, D, feet, {'piston': Vector((0, -1, .1)), 'strut': Vector((0, 1, 0))})
    keep_strut_up(p)
    flick(p, 'Bone.006', 10 * stamp - 6 * jolt + 3 * S(TAU * t), 'X')
    flick(p, 'Bone.007', -8 * stamp + 5 * jolt, 'X')
    flick(p, 'Bone.008', 6 * S(TAU * t + 1), 'Z')
    swing(p, fore=-12 * S(TAU * t - 1.0) - 10 * jolt + 8 * stamp, side=-12 * S(TAU * t - .9))
    report('walk', t, p)
    return p


clip('Cursed_Walk', 43, walk)

# ---- Attack: the pack's attackPhys, the hook kept out of the floor ------------------------------
ATTACK = bpy.data.actions['attackPhys']
A0, A1 = ATTACK.frame_range


def attack(t):
    p = pack('attackPhys', A0 + (A1 - A0) * t)
    keep_strut_up(p)
    keep_boot_up(p)
    report('attack', t, p)
    return p


clip('Cursed_Attack', int(A1 - A0) + 1, attack)

# ---- Pain: jolt, flicker, stagger ---------------------------------------------------------------
def pain(t):
    p = copy(base)
    hit = ramp(t, 0, .08) * (1 - ramp(t, .12, .45))
    stag = bump(t, .1, .28, .4, .9)
    catch = bump(t, .12, .2, .55, .92)
    shake = S(TAU * 7 * t) * bump(t, .04, .1, .3, .45)
    D = (T((.05 * stag, .1 * hit + .04 * stag, .03 * hit - .09 * stag))
         @ about(PIVOT, euler(x=-16 * hit + 7 * stag + 2 * shake, y=9 * hit + 8 * stag - 2 * shake, z=-12 * hit + 5 * stag)))
    strut = Vector(BASE_STRUT) + Vector((.03 * catch, .12 * catch, -BASE_STRUT[2] * catch))
    body(p, D, {'strut': strut, 'strut_local': BASE_STRUT_LOCAL})
    flick(p, 'Bone.006', -35 * hit + 12 * stag, 'X'); flick(p, 'Bone.007', 28 * hit - 10 * stag, 'X')
    flick(p, 'Bone.008', -30 * hit + 6 * shake, 'Z')
    swing(p, fore=38 * hit - 26 * stag + 8 * ramp(t, .5, 1) * S(TAU * 2 * t), side=-14 * hit + 10 * stag)
    out = bump(t, .05, .06, .14, .16) + bump(t, .3, .31, .35, .37)
    lamp_scale(p, 1 - .98 * min(1., out))
    keep_strut_up(p)
    report('pain', t, p)
    return p


clip('Cursed_Pain', 19, pain)


# ---- Death: a convulsion, the piston knee gives, it goes over onto its face ---------------------
def grounded(p):
    z = float(points(p)[:, 2].min())
    move(p, BONES, T((0, 0, -z)))
    return p


FRONT = Vector((ANKLE.x + .05, ANKLE.y - .3, 0))     # the floor it pitches over onto


def death(t):
    p = copy(base)
    jerk = bump(t, 0, .04, .08, .2)
    shudder = S(TAU * 9 * t) * bump(t, .04, .08, .26, .34)
    give = ramp(t, .14, .42)
    u = min(1., max(0., (t - .36) / .3)); fall = u * u           # it drops like a dead weight
    bounce = bump(t, .66, .71, .71, .8)
    D = (T((.02 * shudder, .06 * jerk, .03 * jerk - .4 * give))
         @ about(PIVOT, euler(x=-13 * jerk + 16 * give + 3 * shudder, y=8 * jerk - 6 * give, z=-8 * jerk + 10 * give)))
    strut = Vector(BASE_STRUT) + Vector((.06 * give, .2 * give, -BASE_STRUT[2] * give))
    body(p, D, {'strut': strut, 'strut_local': BASE_STRUT_LOCAL})
    move(p, BONES, about(FRONT, euler(x=82 * fall - 7 * bounce, y=20 * fall - 2 * bounce, z=-14 * fall)))
    flick(p, 'Bone.004', 34 * fall - 6 * bounce, 'X')                  # the strut kicks up behind
    flick(p, 'Bone.001', -10 * fall, 'X')                              # the boot leg trails, sagging
    flick(p, 'Bone.002', 12 * fall, 'X')
    flick(p, 'Bone.006', -30 * jerk - 25 * bounce, 'X'); flick(p, 'Bone.007', 25 * jerk + 15 * bounce, 'X')
    swing(p, fore=30 * jerk - 40 * fall + 28 * bounce + 8 * ramp(t, .78, 1) * S(TAU * 3 * t), side=-10 * jerk + 12 * fall)
    out = bump(t, .05, .06, .1, .12) + bump(t, .2, .21, .27, .29) + bump(t, .4, .41, .45, .46) + ramp(t, .6, .66)
    lamp_scale(p, 1 - .98 * min(1., out))
    grounded(p)
    report('death', t, p)
    return p


clip('Cursed_Death', 54, death)

# ---- NLA strips, rest pose, materials' texture, export ------------------------------------------
ad = arm.animation_data
ad.action = None
for name, (action, first, last) in clips.items():
    track = ad.nla_tracks.new(); track.name = name
    strip = track.strips.new(name, first, action)
    if hasattr(strip, 'action_slot') and action.slots:
        strip.action_slot = action.slots[0]
    strip.action_frame_start, strip.action_frame_end = first, last
for pb in arm.pose.bones:
    pb.location, pb.rotation_quaternion, pb.scale = (0, 0, 0), (1, 0, 0, 0), (1, 1, 1)
for a in [a for a in bpy.data.actions if a.name not in clips]:
    bpy.data.actions.remove(a)

# ---- texture ------------------------------------------------------------------------------------
src = next(i for i in bpy.data.images if i.size[0] > 0)
W, H = src.size
pix = np.array(src.pixels[:], dtype=np.float32).reshape(H, W, 4)[..., :3].copy()   # row 0 is the bottom


def raster(objs):
    mask = np.zeros((H, W), bool)
    ys, xs = np.mgrid[0:H, 0:W]
    for o in objs:
        me = o.data; me.calc_loop_triangles(); uv = me.uv_layers.active.data
        for tri in me.loop_triangles:
            q = np.array([uv[l].uv[:] for l in tri.loops]) * (W, H) - .5
            x0, y0 = np.floor(q.min(0)).astype(int) - 1; x1, y1 = np.ceil(q.max(0)).astype(int) + 1
            x0, y0 = max(x0, 0), max(y0, 0); x1, y1 = min(x1, W - 1), min(y1, H - 1)
            if x1 < x0 or y1 < y0:
                continue
            px_, py_ = xs[y0:y1 + 1, x0:x1 + 1], ys[y0:y1 + 1, x0:x1 + 1]
            (ax, ay), (bx, by_), (cx, cy) = q
            den = (by_ - cy) * (ax - cx) + (cx - bx) * (ay - cy)
            if abs(den) < 1e-9:
                continue
            l1 = ((by_ - cy) * (px_ - cx) + (cx - bx) * (py_ - cy)) / den
            l2 = ((cy - ay) * (px_ - cx) + (ax - cx) * (py_ - cy)) / den
            l3 = 1 - l1 - l2
            e = -.75 / max(1., math.sqrt(abs(den)))                     # a little over the edges
            mask[y0:y1 + 1, x0:x1 + 1] |= (l1 >= e) & (l2 >= e) & (l3 >= e)
    return mask


def grow(mask, n):
    m = mask.copy()
    for _ in range(n):
        g = m.copy()
        for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1)):
            g |= np.roll(np.roll(m, dy, 0), dx, 1)
        m = g
    return m


covered = grow(raster([o for o in parts if o is not lamp]), 1)
lens = raster([lamp])
print('CURSED texture: lamp lens texels', int(lens.sum()), 'shared with other parts', int((lens & covered).sum()))
lens_g = grow(lens, 2) & ~covered
# The lens: every texel lifted over the glow threshold, keeping its tint.
y = pix[..., 0] * .3 + pix[..., 1] * .59 + pix[..., 2] * .11
target = .5 + .5 * y
tint = np.array([.92, 1.0, .72], np.float32)
scaled = pix * (target / np.maximum(y, 1e-3))[..., None]
w = np.clip(y / .25, 0, 1)[..., None]
lit = np.clip(scaled * w + tint * target[..., None] * (1 - w), 0, 1)
pix[lens_g] = lit[lens_g]
# Pad the gutters: grow the islands' colours outward into every texel no triangle covers.
filled = (covered | lens_g).astype(np.float32)
colour = pix * filled[..., None]
for _ in range(400):
    if filled.all():
        break
    total = np.zeros_like(colour); count = np.zeros_like(filled)
    for dy, dx in ((-1, 0), (1, 0), (0, -1), (0, 1), (-1, -1), (1, 1), (-1, 1), (1, -1)):
        total += np.roll(np.roll(colour, dy, 0), dx, 1)
        count += np.roll(np.roll(filled, dy, 0), dx, 1)
    g = (filled == 0) & (count > 0)
    colour[g] = total[g] / count[g][:, None]
    filled[g] = 1
half = colour.reshape(H // 2, 2, W // 2, 2, 3).mean(axis=(1, 3))
yl = half[..., 0] * .3 + half[..., 1] * .59 + half[..., 2] * .11
lens_half = lens.reshape(H // 2, 2, W // 2, 2).all(axis=(1, 3))
print('CURSED texture: lens luma at 512 min %.3f mean %.3f; body luma >.28 %.1f%%' % (
    yl[lens_half].min(), yl[lens_half].mean(), 100 * (yl[~lens_half] > .28).mean()))
out = bpy.data.images.new('cursed.png', W // 2, H // 2, alpha=False)
out.pixels = np.concatenate([half, np.ones(half.shape[:2] + (1,), np.float32)], axis=2).ravel()
out.filepath_raw = out_png; out.file_format = 'PNG'; out.save()
out.filepath = out_png
for m in bpy.data.materials:
    if m.node_tree:
        for n in m.node_tree.nodes:
            if n.type == 'TEX_IMAGE':
                n.image = out
for i in [i for i in bpy.data.images if i is not out]:
    bpy.data.images.remove(i)

# The FBX's static pose (what the engine loads as the bind, and sizes the creature by) is the pose at the
# scene's frame with the strips playing: frame 1, where every clip starts on the loaded stand.
sc.frame_set(1)
os.makedirs(os.path.dirname(os.path.abspath(out_fbx)), exist_ok=True)
bpy.ops.export_scene.fbx(filepath=out_fbx, use_selection=False, object_types={'ARMATURE', 'MESH'},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
                         bake_anim_use_nla_strips=True, bake_anim_force_startend_keying=True,
                         path_mode='STRIP', embed_textures=False)
print('CURSED exported', out_fbx, 'clips', list(clips), 'bones', len(arm.data.bones))
