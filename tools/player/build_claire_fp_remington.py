# Claire's first-person Remington M700 (a bolt action): her arms refitted onto the RetroWeaponPack's
# animated FP arm rig (the pistol set, FP_Arms_Rifle_01_Anims.blend), the
# pistol clips baked (arms, gun and constraints evaluated) into the pack
# camera's space, and the game's MAC-11 carried by reference points that ride
# the pack's pistol: its frame, its magazine and its slide (the MAC's cocking
# knob). The same refit as tools/player/build_claire_fp_shotgun.py.
#   blender -b --python tools/player/build_claire_fp_remington.py -- PACK.blend Claire.fbx rifle.json OUT.fbx [PREVIEW_DIR M700.fbx]
# Output (y-up, metres, camera at the origin looking down -z, x right):
#   ArmsMesh  her arms, skinned to the pack's deform bones (flat hierarchy)
#   gun_o/x/y/z, mag_o/x/y/z, knob_o/x/y/z  points that carry the MAC-11's
#     origin and unit axes (engine MAC-11 coordinates: metres, the grip's top
#     at the origin, +z the muzzle, +y up, +x the gun's left) for the frame, the
#     magazine and the cocking knob
#   one action per clip (see CLIPS). Long breathing loops are keyed every
#   STEP frames with linear keys and the export simplifies what is linear.
import bpy, sys, math, os
from mathutils import Matrix, Vector
args = sys.argv[sys.argv.index('--')+1:]; pack_path, claire_path, rifle_json, out_path = args[:4]
PREVIEW = args[4] if len(args) > 4 else None; M700 = args[5] if len(args) > 5 else None
import json
REF = json.load(open(rifle_json))   # the rifle's sight points in engine coordinates (tools/player/import_sniper_fp.py)
CLIPS = [  # name, arms action, gun action, first, last, key step   (the pack's rifle clips; the hands are refitted to this rifle below)
 ('idle', 'Arms_BasePose', 'Rifle_Breathing', 1, 120, 8),
 ('aim', 'Arms_AimPose', 'Rifle_Breathing_Aiming', 1, 120, 8),
 ('aim_start', 'Arms_AimStart', 'Rifle_AimStart', 1, 11, 1),
 ('aim_end', 'Arms_AimEnd', 'Rifle_AimEnd', 1, 11, 1),
 ('walk', 'Arms_BasePose', 'Rifle_Walk', 1, 30, 2),
 ('walk_aim', 'Arms_AimPose', 'Rifle_Walk_Aiming', 1, 30, 2),
 ('run', 'Arms_BasePose', 'Rifle_Run', 1, 20, 2),
 ('fire', 'Arms_Fire', 'Rifle_Fire', 1, 12, 1),
 ('fire_aim', 'Arms_AimFireAiming', 'Rifle_AimFire', 1, 14, 1),
 ('draw', 'Arms_Draw', 'Rifle_Draw', 1, 20, 1),
]
# Authored here, not the pack's: the right hand works the bolt (the pack has no bolt action). Frames at 24 fps; the base pose carries the gun.
BOLT_FRAMES = 28
RELOAD_FRAMES = 86
CUSTOM = [('bolt', BOLT_FRAMES), ('reload', RELOAD_FRAMES)]
bpy.ops.wm.open_mainfile(filepath=pack_path)
scene = bpy.context.scene
arms = bpy.data.objects['Arms_Armature']; gun = bpy.data.objects['Rifle_01_Armature']
cam = bpy.data.objects['Camera']
for o in (arms, gun):
    for t in o.animation_data.nla_tracks: t.mute = True
def use(o, name):
    a = bpy.data.actions[name]; o.animation_data.action = a
    if a.slots: o.animation_data.action_slot = a.slots[0]
TO_EXPORT = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
UNIT = .01
def cam_space(world_m):
    m = cam.matrix_world.inverted() @ world_m
    m = Matrix.Diagonal((UNIT, UNIT, UNIT, 1)) @ m
    return TO_EXPORT @ m

pack_mesh = bpy.data.objects['FPS_Arms_Mesh']
deform = [b.name for b in arms.data.bones if not b.name.startswith('ctrl_') and b.name not in ('root', 'camera')]
print('DEFORM', len(deform))

# --- Claire's arms, refitted onto the pack rig's rest pose (as the shotgun's) --
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=claire_path)
claire_objs = [o for o in bpy.data.objects if o not in before]
claire_names = {o.name for o in claire_objs}
skel = next(o for o in claire_objs if o.type == 'ARMATURE')
mapping = {}
for side, s in (('R', 'r'), ('L', 'l')):
    mapping['Arm_'+side] = 'upperArm_'+s; mapping['ForeArm_'+side] = 'lowerArm_'+s; mapping['Hand_'+side] = 'hand_'+s
    for f, t in (('Thumb', 'thumb'), ('Index', 'index'), ('Middle', 'middle'), ('Ring', 'ring'), ('Pinky', 'pinky')):
        for j in (1, 2, 3): mapping[f'{f}{j}_{side}'] = f'{t}_0{j}_{s}'
cw = skel.matrix_world; aw = arms.matrix_world
claire_mesh_for_girth = max((o for o in claire_objs if o.type == 'MESH' and 'ForeArm_R' in o.vertex_groups), key=lambda o: len(o.data.vertices))
def ch(n): b = skel.data.bones[n]; return cw @ b.head_local, cw @ b.tail_local
def rg(n): b = arms.data.bones[n]; return aw @ b.head_local, aw @ b.tail_local
def frame(direction, side):
    x = direction.normalized(); y = (side - x*side.dot(x)).normalized(); return Matrix((x, y, x.cross(y))).transposed()
def girth(obj, groups, a, b):
    gis = {obj.vertex_groups[g].index for g in groups if g in obj.vertex_groups}; ax = (b - a); L = ax.length; ax.normalize(); ds = []
    for v in obj.data.vertices:
        w = sum(g.weight for g in v.groups if g.group in gis)
        if w < .5: continue
        p = obj.matrix_world @ v.co; t = (p - a).dot(ax)
        if .2*L < t < .8*L: ds.append(((p - a) - ax*t).length)
    return sum(ds)/len(ds) if ds else None
transforms = {}
for side, s in (('R', 'r'), ('L', 'l')):
    arm = (rg(f'hand_{s}')[0]-rg(f'upperArm_{s}')[0]).length/(ch(f'Hand_{side}')[0]-ch(f'Arm_{side}')[0]).length
    seg_girth = {}
    for her, its, end_her, end_its in ((f'Arm_{side}', f'upperArm_{s}', f'ForeArm_{side}', f'lowerArm_{s}'), (f'ForeArm_{side}', f'lowerArm_{s}', f'Hand_{side}', f'hand_{s}')):
        pg = [f'upperArm_{s}', f'UpperArm_{s}'] if its.startswith('upper') else [f'twist_1_arm_{s}', f'twist_2_arm_{s}', f'twist_3_arm_{s}', f'LowerArm_{s}', f'ArmTwist_{s}']
        gp = girth(pack_mesh, pg, rg(its)[0], rg(end_its)[0]) or girth(pack_mesh, [g[:-1]+'l' for g in pg], rg(its[:-1]+'l')[0], rg(end_its[:-1]+'l')[0])
        gc = girth(claire_mesh_for_girth, [her], ch(her)[0], ch(end_her)[0])
        seg_girth[her] = (gp/gc) if gp and gc else arm
    palm = (rg(f'pinky_01_{s}')[0]-rg(f'index_01_{s}')[0]).length/(ch(f'Pinky1_{side}')[0]-ch(f'Index1_{side}')[0]).length
    cs = ch(f'Pinky1_{side}')[0]-ch(f'Index1_{side}')[0]; rs = rg(f'pinky_01_{s}')[0]-rg(f'index_01_{s}')[0]
    for her, its in mapping.items():
        if not her.endswith(side): continue
        (c0, c1), (r0, r1) = ch(her), rg(its)
        if her.startswith('Hand'): c1 = ch(f'Middle1_{side}')[0]; r1 = rg(f'middle_01_{s}')[0]
        across = seg_girth.get(her, arm) if her.startswith(('Arm', 'ForeArm')) else palm
        fc, fr = frame(c1-c0, cs), frame(r1-r0, rs); along = (r1-r0).length/max(1e-6, (c1-c0).length)
        transforms[her] = (c0, fr @ Matrix.Diagonal((along, across, across)) @ fc.transposed(), r0)
meshes = [o for o in claire_objs if o.type == 'MESH']
for o in meshes:
    for m in list(o.modifiers): o.modifiers.remove(m)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes: o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]; bpy.ops.object.join()
mesh = bpy.context.view_layer.objects.active; mesh.parent = None
mesh.data.transform(mesh.matrix_world); mesh.matrix_world = Matrix()
names = {g.index: g.name for g in mesh.vertex_groups}
def weights(v): return {names[g.group]: g.weight for g in v.groups if g.weight > 0}
keep = set()
for v in mesh.data.vertices:
    w = weights(v); total = sum(w.values()) or 1
    if sum(x for b, x in w.items() if b in mapping)/total > .5: keep.add(v.index)
import bmesh
bm = bmesh.new(); bm.from_mesh(mesh.data)
torso = {i for i, m in enumerate(mesh.data.materials) if m.name in ('VEST', 'JACKET')}
bmesh.ops.delete(bm, geom=[f for f in bm.faces if f.material_index in torso or not all(v.index in keep for v in f.verts)], context='FACES')
bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
bm.to_mesh(mesh.data); bm.free()
for v in mesh.data.vertices:
    w = {b: x for b, x in weights(v).items() if b in transforms}; total = sum(w.values())
    p = Vector((0, 0, 0))
    for b, x in w.items(): c0, m, r0 = transforms[b]; p += (m @ (v.co - c0) + r0)*(x/total)
    v.co = p
data = []
for v in mesh.data.vertices:
    w = {}
    for b, x in weights(v).items():
        if b not in mapping: continue
        target = mapping[b]
        if target.startswith('lowerArm_'):
            s = target[-1]; e, h = rg(f'lowerArm_{s}')[0], rg(f'hand_{s}')[0]
            t = max(0., min(1., (v.co - e).dot((h - e).normalized())/(h - e).length))
            for name, at in ((f'lowerArm_{s}', 0.), (f'twist_2_arm_{s}', .5), (f'twist_3_arm_{s}', .78)):
                share = max(0., 1 - abs(t - at)/.4)
                if share > 0: w[name] = w.get(name, 0) + x*share
        else:
            w[target] = w.get(target, 0) + x
    data.append(w)
def smooth(t, a, b): x = min(1., max(0., (t - a)/(b - a))); return x*x*(3 - 2*x)
for side in ('l', 'r'):
    S, E, H = rg(f'upperArm_{side}')[0], rg(f'lowerArm_{side}')[0], rg(f'hand_{side}')[0]
    along = ((E - S).normalized() + (H - E).normalized()).normalized(); band = .18*(H - E).length
    up, fore = f'upperArm_{side}', [f'lowerArm_{side}', f'twist_2_arm_{side}', f'twist_3_arm_{side}']
    for v, w in zip(mesh.data.vertices, data):
        total = w.get(up, 0) + sum(w.get(f, 0) for f in fore)
        if total <= 0: continue
        d = (v.co - E).dot(along)
        if abs(d) > band: continue
        h = smooth(d, -band, band)
        for f in fore: w.pop(f, None)
        w[up] = total*(1 - h); w[f'lowerArm_{side}'] = total*h
for g in list(mesh.vertex_groups): mesh.vertex_groups.remove(g)
for v, w in zip(mesh.data.vertices, data):
    total = sum(w.values())
    for b, x in w.items():
        g = mesh.vertex_groups.get(b) or mesh.vertex_groups.new(name=b); g.add([v.index], x/total, 'REPLACE')
for o in list(bpy.data.objects):
    if o.name != mesh.name and o.name in claire_names: bpy.data.objects.remove(o, do_unlink=True)
print('CLAIRE ARMS', len(mesh.data.vertices), 'verts')


# --- the Remington on the pack's rig -------------------------------------------------------------------------------------------------
# Armature space (cm): +x the bore, +y the gun's left, +z up. Engine rifle frame (m): +z the bore, +y up, +x the left (the MAC-11's).
# The eyepiece (engine origin) lands on the pack's sight line, 0.8 cm in front of the camera's x; real size (100 cm a metre).
from math import radians
REF = json.load(open(rifle_json))
EYE_X, SCALE = 0.8, 100.0
rear_t = gun.data.bones['RearSight'].tail_local.copy(); front_t = gun.data.bones['FrontSight'].tail_local.copy()
sight = (front_t - rear_t).normalized()
pack_eye = rear_t + sight*((EYE_X - rear_t.x)/sight.x)
R = Matrix(((0, 0, 1), (1, 0, 0), (0, 1, 0)))
FIT = Matrix.Translation(pack_eye) @ Vector((1, 0, 0)).rotation_difference(sight).to_matrix().to_4x4() @ (R*SCALE).to_4x4()
FIT_INV = FIT.inverted()
def P(name): return FIT @ Vector(REF[name])
main_rest = gun.data.bones['Main'].matrix_local
def motion(ge): return ge.matrix_world @ ge.pose.bones['Main'].matrix @ main_rest.inverted()
def ev():
    dg = bpy.context.evaluated_depsgraph_get(); return arms.evaluated_get(dg), gun.evaluated_get(dg)
def smooth(x): x = min(1., max(0., x)); return x*x*(3 - 2*x)
def lerp_m(a, b, u):
    la, ra, _ = a.decompose(); lb, rb, _ = b.decompose()
    return Matrix.Translation(la.lerp(lb, u)) @ ra.slerp(rb, u).to_matrix().to_4x4()

# Hands: the pack's two hand controls follow the pack rifle (Child Of Main / Magazine). Here an empty carries each (Copy Transforms),
# placed from the gun's own motion, so the hands sit on this rifle's wrist and forend whatever the clip.
def empty(n):
    o = bpy.data.objects.new(n, None); scene.collection.objects.link(o); return o
tgt = {s: empty('tgt_'+s) for s in 'rl'}; cons = {}
for s in 'rl':
    pb = arms.pose.bones['ctrl_HandIK_'+s]
    co = next(c for c in pb.constraints if c.type == 'CHILD_OF')
    ct = pb.constraints.new('COPY_TRANSFORMS'); ct.target = tgt[s]; ct.influence = 0
    cons[s] = (co, ct)
def override(on):
    for s in 'rl': cons[s][0].influence = 0 if on else 1; cons[s][1].influence = 1 if on else 0
use(arms, 'Arms_AimPose'); use(gun, 'Rifle_Breathing_Aiming'); scene.frame_set(1); override(False); bpy.context.view_layer.update()   # the bolt is worked shouldered
ae, ge = ev(); M0 = motion(ge); M0i = M0.inverted()
for label in ('eye', 'front', 'muzzle'):   # where the scope's axis sits in the aimed pose (camera space, metres; the bake's export axes: x right, y forward, z up)
    print('AIM', label, tuple(round(x, 4) for x in cam_space(M0 @ FIT @ Matrix.Translation(Vector(REF[label]))).to_translation()))
L = {s: M0i @ (ae.matrix_world @ ae.pose.bones['ctrl_HandIK_'+s].matrix) for s in 'rl'}   # each hand control in armature space at rest
trig_c = (gun.data.bones['Trigger'].head_local + gun.data.bones['Trigger'].tail_local)/2
print('HANDS rest r', tuple(round(x, 2) for x in L['r'].translation), 'l', tuple(round(x, 2) for x in L['l'].translation), 'trigger', tuple(round(x, 2) for x in trig_c))
# the right hand keeps the pack's place on the grip relative to the trigger; the left sits under the forend
HAND_R_TWEAK = Vector((0, 0, 0)); HAND_L_TWEAK = Vector((0, 0, -2.0))
delta_r = P('trigger') - trig_c + HAND_R_TWEAK
delta_l = Vector((P('forend').x - L['l'].translation.x, 0, P('forend').z - L['l'].translation.z)) + HAND_L_TWEAK
HG = Matrix.Translation(delta_r) @ L['r']                     # the right hand on the grip
HL = Matrix.Translation(delta_l) @ L['l']                     # the left hand under the forend
# The right hand on the bolt knob: the knob, a hand's width out from it, the palm over the knob from the rifle's right
HK_OFF = Vector((-1.5, -8.0, -2.0)); HK_ROT = Matrix.Rotation(radians(0), 4, 'X'); HK_TURN = 0.5
HK = Matrix.Translation(P('knob') + HK_OFF) @ L['r'].to_3x3().to_4x4() @ HK_ROT
POUCH = Matrix.Translation(P('wrist') + Vector((-4.0, -12.0, -16.0))) @ L['r'].to_3x3().to_4x4()
PORT = Matrix.Translation(P('knob') + Vector((6.0, -0.5, 4.5))) @ L['r'].to_3x3().to_4x4()
# the bolt: back along the bore and the handle lifted about the bolt's own axis (engine frame, metres)
A = Vector(REF['bolt_axis']); BACK = REF['bolt_back']
def bolt_eng(theta, d): return Matrix.Translation((0, 0, -d)) @ Matrix.Translation(A) @ Matrix.Rotation(theta, 4, 'Z') @ Matrix.Translation(-A)
def bolt_arm(theta, d): return FIT @ bolt_eng(theta, d) @ FIT_INV
UP = radians(-80)
def bolt_state(k, t):   # t: (rot up from, to, pull from, to, push from, to, rot down from, to)
    ru0, ru1, pu0, pu1, ps0, ps1, rd0, rd1 = t
    theta = UP*smooth((k - ru0)/(ru1 - ru0)) if k < rd0 else UP*(1 - smooth((k - rd0)/(rd1 - rd0)))
    d = BACK*smooth((k - pu0)/(pu1 - pu0)) if k < ps0 else BACK*(1 - smooth((k - ps0)/(ps1 - ps0)))
    return theta, d
BOLT_T = (3, 8, 8, 14, 16, 21, 21, 25)
RELOAD_T = (4, 8, 8, 14, 65, 70, 70, 75)
def knob_hand(th, d):   # the hand follows the knob's place but keeps its own side of it; it turns a little with the handle
    pos = bolt_arm(th, d) @ P('knob') + HK_OFF
    return Matrix.Translation(pos) @ L['r'].to_3x3().to_4x4() @ HK_ROT @ Matrix.Rotation(th*HK_TURN, 4, 'X')
def hand_bolt(k, t, w_in0, w_in1, w_out0, w_out1):
    th, d = bolt_state(k, t); hk = knob_hand(th, d)
    w = smooth((k - w_in0)/(w_in1 - w_in0)) if k < w_out0 else 1 - smooth((k - w_out0)/(w_out1 - w_out0))
    return lerp_m(HG, hk, w), th, d
def right_bolt_clip(k): return hand_bolt(k, BOLT_T, 0, 3, 25, 28)
def right_reload_clip(k):
    if k < 14 or (k >= 65 and k < 75): return hand_bolt(k, RELOAD_T, 0, 4, 75, 80)
    th, d = bolt_state(k, RELOAD_T); open_hk = knob_hand(th, d)
    if k < 17: return lerp_m(open_hk, POUCH, smooth((k - 14)/3)), th, d
    if k < 61:
        j = (k - 17) % 11; u = j/11
        return (lerp_m(POUCH, PORT, smooth(u/.5)) if u < .5 else lerp_m(PORT, POUCH, smooth((u - .5)/.5))), th, d
    if k < 65: return lerp_m(POUCH, open_hk, smooth((k - 61)/4)), th, d
    return HG, th, d

# --- export armature: flat deform bones and the gun points ----------------------------------------------------------------------------
bpy.ops.object.select_all(action='DESELECT')
rig_data = bpy.data.armatures.new('FPRig'); rig = bpy.data.objects.new('FPRig', rig_data)
scene.collection.objects.link(rig); bpy.context.view_layer.objects.active = rig
points = [p+'_'+a for p in ('gun', 'mag', 'knob') for a in 'oxyz']
bpy.ops.object.mode_set(mode='EDIT')
for n in deform:
    b = rig_data.edit_bones.new(n); rb = arms.data.bones[n]
    b.head = (arms.matrix_world @ rb.head_local); b.tail = (arms.matrix_world @ rb.tail_local); b.roll = 0
for n in points:
    b = rig_data.edit_bones.new(n); b.head = (0, 0, 0); b.tail = (0, 0, 1)
bpy.ops.object.mode_set(mode='OBJECT')
mesh.parent = rig; mod = mesh.modifiers.new('Armature', 'ARMATURE'); mod.object = rig
mesh.name = 'ArmsMesh'; mesh.data.name = 'ArmsMesh'
rig.animation_data_create()
fps = scene.render.fps

# Preview: the pack's own arms stand in for hers; the rifle is the real model on the fitted matrices.
preview_parts = {}
if PREVIEW:
    scene.render.engine = 'BLENDER_WORKBENCH'; scene.render.resolution_x = 640; scene.render.resolution_y = 360; scene.render.resolution_percentage = 100
    scene.display.shading.light = 'STUDIO'; scene.display.shading.color_type = 'OBJECT'; scene.render.film_transparent = False
    for o in bpy.data.objects:
        if o.name.startswith('Ctrl_Mesh') or o.name in ('Cube', 'ChargeHandle_Mesh'): o.hide_render = True
    before = set(bpy.data.objects); bpy.ops.import_scene.fbx(filepath=M700)
    for o in set(bpy.data.objects) - before:
        if o.type != 'MESH': continue
        o.data = o.data.copy(); o.data.transform(o.matrix_world); o.matrix_world = Matrix(); o.parent = None
        o.color = {'Base': (.45, .28, .12, 1), 'Bolt': (.8, .15, .1, 1), 'Barrel': (.2, .22, .26, 1), 'Scope': (.1, .12, .15, 1)}.get(o.name, (.5, .5, .5, 1)); preview_parts[o.name] = o
    for o in arms.children: o.color = (.12, .12, .12, 1)
    ys = [v.co.y for o in preview_parts.values() for v in o.data.vertices]; k = 9.74/(max(ys) - min(ys)); print('PREVIEW rifle scale', k, 'y range', min(ys), max(ys))
    for o in preview_parts.values(): o.data.transform(Matrix.Scale(k, 4))
    # rifle units -> engine metres -> armature cm, as tools/player/import_remington700.py writes it:  (x, y, z) -> (-x, z - 1.82, y + 2.124) * .1
    ENG = Matrix(((-.1, 0, 0, 0), (0, 0, .1, -.182), (0, .1, 0, .2124), (0, 0, 0, 1)))
    os.makedirs(PREVIEW, exist_ok=True)
def preview(tag, M, Bm):
    if not PREVIEW: return
    for n, o in preview_parts.items(): o.matrix_world = M @ (Bm if n == 'Bolt' else Matrix()) @ FIT @ ENG
    scene.render.filepath = os.path.join(PREVIEW, tag + '.png'); bpy.ops.render.render(write_still=True)
PREVIEW_AT = {'idle': (1,), 'aim': (1,), 'fire': (1, 4, 8), 'bolt': (1, 4, 6, 9, 12, 16, 19, 23, 27), 'reload': (1, 6, 12, 18, 24, 30, 40, 55, 63, 68, 73, 79, 85)}

def write_frame(frame_no, M, Bm, hand_r, hand_l):
    tgt['r'].matrix_world = M @ hand_r; tgt['l'].matrix_world = M @ hand_l; override(True); bpy.context.view_layer.update()
    ae, ge = ev()
    for n in deform:
        world = ae.matrix_world @ ae.pose.bones[n].matrix
        pb = rig.pose.bones[n]; pb.matrix = cam_space(world)
        pb.keyframe_insert('location', frame=frame_no); pb.keyframe_insert('rotation_quaternion', frame=frame_no); pb.keyframe_insert('scale', frame=frame_no)
    for prefix, mat in (('gun', M @ FIT), ('mag', M @ FIT), ('knob', M @ Bm @ FIT)):
        for axis, p in (('o', (0, 0, 0)), ('x', (1, 0, 0)), ('y', (0, 1, 0)), ('z', (0, 0, 1))):
            q = cam_space(mat @ Matrix.Translation(Vector(p))).to_translation()
            pb = rig.pose.bones[f'{prefix}_{axis}']; pb.matrix = Matrix.Translation(q); pb.keyframe_insert('location', frame=frame_no)
    return ae

def linear(act):
    for fc in (act.layers[0].strips[0].channelbags[0].fcurves if hasattr(act, 'layers') and act.layers else act.fcurves):
        for k in fc.keyframe_points: k.interpolation = 'LINEAR'
for name, a, g, first, last, step in CLIPS:
    use(arms, a); use(gun, g)
    act = bpy.data.actions.new(name); rig.animation_data.action = act
    frames = list(range(first, last + 1, step))
    if frames[-1] != last: frames.append(last)
    for f in frames:
        scene.frame_set(f); override(False); bpy.context.view_layer.update()
        ae, ge = ev(); M = motion(ge)
        Wr = ae.matrix_world @ ae.pose.bones['ctrl_HandIK_r'].matrix; Wl = ae.matrix_world @ ae.pose.bones['ctrl_HandIK_l'].matrix
        Mi = M.inverted()
        hr = Matrix.Translation(delta_r) @ (Mi @ Wr); hl = Matrix.Translation(delta_l) @ (Mi @ Wl)   # the pack's hands this frame, moved onto this rifle
        write_frame(f - first + 1, M, Matrix(), hr, hl)
        if PREVIEW and f - first + 1 in PREVIEW_AT.get(name, ()): preview(f'{name}-{f - first + 1:03d}', M, Matrix())
    linear(act); print('CLIP', name, last - first + 1, 'frames', round((last - first + 1)/fps, 3), 's,', len(frames), 'keys')
use(arms, 'Arms_AimPose')
for name, count in CUSTOM:
    use(gun, 'Rifle_Breathing_Aiming'); act = bpy.data.actions.new(name); rig.animation_data.action = act
    for k in range(count):
        scene.frame_set(1 + k); override(False); bpy.context.view_layer.update()
        ae, ge = ev(); M = motion(ge)
        hr, th, d = right_bolt_clip(k) if name == 'bolt' else right_reload_clip(k)
        write_frame(k + 1, M, bolt_arm(th, d), hr, HL)
        if PREVIEW and k + 1 in PREVIEW_AT.get(name, ()): preview(f'{name}-{k + 1:03d}', M, bolt_arm(th, d))
    linear(act); print('CLIP', name, count, 'frames', round(count/fps, 3), 's')
for o in list(bpy.data.objects):
    if o not in (rig, mesh): bpy.data.objects.remove(o, do_unlink=True)
clip_names = {c[0] for c in CLIPS} | {c[0] for c in CUSTOM}
for act in list(bpy.data.actions):
    if act.name not in clip_names: bpy.data.actions.remove(act)
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = 1.0
rig.animation_data.action = bpy.data.actions['idle']
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); mesh.select_set(True)
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=True, add_leaf_bones=False, bake_anim=True,
    bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
    bake_anim_simplify_factor=1.0, path_mode='STRIP', axis_forward='-Z', axis_up='Y', global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE')
print('EXPORTED', out_path)
