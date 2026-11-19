# Claire's first-person MAC-11: her arms refitted onto the RetroWeaponPack's
# animated FP arm rig (the pistol set, FP_Arms_Pistol_01_Anims.blend), the
# pistol clips baked (arms, gun and constraints evaluated) into the pack
# camera's space, and the game's MAC-11 carried by reference points that ride
# the pack's pistol: its frame, its magazine and its slide (the MAC's cocking
# knob). The same refit as tools/player/build_claire_fp_shotgun.py.
#   blender -b --python tools/player/build_claire_fp_mac11.py -- PACK.blend Claire.fbx OUT.fbx
# Output (y-up, metres, camera at the origin looking down -z, x right):
#   ArmsMesh  her arms, skinned to the pack's deform bones (flat hierarchy)
#   gun_o/x/y/z, mag_o/x/y/z, knob_o/x/y/z  points that carry the MAC-11's
#     origin and unit axes (engine MAC-11 coordinates: metres, the grip's top
#     at the origin, +z the muzzle, +y up, +x the gun's left) for the frame, the
#     magazine and the cocking knob
#   one action per clip (see CLIPS). Long breathing loops are keyed every
#   STEP frames with linear keys and the export simplifies what is linear.
import bpy, sys, math
from mathutils import Matrix, Vector
pack_path, claire_path, out_path = sys.argv[sys.argv.index('--')+1:][:3]
CLIPS = [  # name, arms action, gun action, first, last, key step
 ('idle', 'Arms_BasePose', 'Pistol_Breathing', 1, 200, 8),
 ('aim', 'Arms_AimPose', 'Pistol_Breathing_Aiming', 1, 200, 8),
 ('walk', 'Arms_BasePose', 'Pistol_Walk', 1, 30, 2),
 ('walk_aim', 'Arms_AimPose', 'Pistol_Walk_Aiming', 1, 30, 2),
 ('run', 'Arms_BasePose', 'Pistol_Run', 1, 20, 2),
 ('reload', 'Arms_Reload', 'Pistol_Reload', 1, 37, 1),
 ('draw', 'Arms_Draw', 'Pistol_Draw', 1, 12, 1),
]
# The MAC-11 in engine coordinates (tools/player/import_mac11.py writes the
# same): its magazine's top, inside the grip, and the magazine's axis.
MAC_MAG_TOP = Vector((0.0, 0.0196, 0.0))       # metres: the magazine's top rides up into the receiver
bpy.ops.wm.open_mainfile(filepath=pack_path)
scene = bpy.context.scene
arms = bpy.data.objects['Arms_Armature']; gun = bpy.data.objects['Pistol_01_Armature']
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

# --- the MAC-11 fitted onto the pack's pistol ----------------------------------
# Pack armature space (cm): +x the bore, +z up, +y the gun's left. Engine MAC
# coordinates (m): +z the bore, +y up, +x the left. The magazine's top inside
# the grip lands on the pack magazine's top; the bore stays level with the
# pack's; real size (100 cm a metre).
pm = bpy.data.objects['Pistol_Mesh']; gw = gun.matrix_world
mag_group = pm.vertex_groups['Magazine'].index
mag_pts = [gw.inverted() @ (pm.matrix_world @ v.co) for v in pm.data.vertices if any(g.group == mag_group and g.weight > .5 for g in v.groups)]
top = max(p.z for p in mag_pts); band = [p for p in mag_pts if p.z > top - 1.5]
pack_mag_top = sum(band, Vector())/len(band)
R = Matrix(((0, 0, 1), (1, 0, 0), (0, 1, 0)))   # engine (x,y,z) -> pack (z, x, y): x<-z, y<-x, z<-y
SCALE = 100.0
fit3 = R*SCALE; offset = pack_mag_top - fit3 @ MAC_MAG_TOP
FIT = Matrix.Translation(offset) @ fit3.to_4x4()
print('FIT pack magazine top', tuple(round(x, 3) for x in pack_mag_top), 'offset', tuple(round(x, 3) for x in offset))
slide_rest_len = gun.data.bones['Slide'].length

# --- export armature: flat deform bones and the gun points --------------------
bpy.ops.object.select_all(action='DESELECT')
rig_data = bpy.data.armatures.new('FPRig'); rig = bpy.data.objects.new('FPRig', rig_data)
scene.collection.objects.link(rig); bpy.context.view_layer.objects.active = rig
scene.frame_set(1); use(arms, 'Arms_BasePose'); use(gun, 'Pistol_IdlePose'); scene.frame_set(1)
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
for name, a, g, first, last, step in CLIPS:
    use(arms, a); use(gun, g)
    act = bpy.data.actions.new(name); rig.animation_data.action = act
    frames = list(range(first, last + 1, step))
    if frames[-1] != last: frames.append(last)
    for f in frames:
        scene.frame_set(f)
        dg = bpy.context.evaluated_depsgraph_get(); ae = arms.evaluated_get(dg); ge = gun.evaluated_get(dg)
        frame_no = f - first + 1
        for n in deform:
            world = ae.matrix_world @ ae.pose.bones[n].matrix
            pb = rig.pose.bones[n]; pb.matrix = cam_space(world)
            pb.keyframe_insert('location', frame=frame_no); pb.keyframe_insert('rotation_quaternion', frame=frame_no); pb.keyframe_insert('scale', frame=frame_no)
        for prefix, bone in (('gun', 'Main'), ('mag', 'Magazine'), ('knob', 'Slide')):
            mat = ge.matrix_world @ ge.pose.bones[bone].matrix @ gun.data.bones[bone].matrix_local.inverted() @ FIT
            for axis, p in (('o', (0, 0, 0)), ('x', (1, 0, 0)), ('y', (0, 1, 0)), ('z', (0, 0, 1))):
                q = cam_space(mat @ Matrix.Translation(Vector(p))).to_translation()
                pb = rig.pose.bones[f'{prefix}_{axis}']; pb.matrix = Matrix.Translation(q)
                pb.keyframe_insert('location', frame=frame_no)
        if name in ('reload', 'idle') and f in (1, 12, 20, 27, 29, 31, 37):
            sl = ge.pose.bones['Slide'].matrix.translation - gun.data.bones['Slide'].head_local
            print('  ', name, f, 'slide offset (cm)', tuple(round(x, 2) for x in sl))
    # Linear between keys: the export then drops what a line already says.
    for fc in (act.layers[0].strips[0].channelbags[0].fcurves if hasattr(act, 'layers') and act.layers else act.fcurves):
        for k in fc.keyframe_points: k.interpolation = 'LINEAR'
    print('CLIP', name, last - first + 1, 'frames', round((last - first + 1)/fps, 3), 's,', len(frames), 'keys')
# Where the front sight sits in the aimed pose (camera space, metres): the engine lowers the aim by it.
use(arms, 'Arms_AimPose'); use(gun, 'Pistol_AimPose'); scene.frame_set(1)
dg = bpy.context.evaluated_depsgraph_get(); ge = gun.evaluated_get(dg)
mat = ge.matrix_world @ ge.pose.bones['Main'].matrix @ gun.data.bones['Main'].matrix_local.inverted() @ FIT
for label, p in (('front sight', (0.0, 0.066, 0.111)), ('knob', (0.0, 0.063, 0.072)), ('muzzle', (0.0, 0.031, 0.163))):
    print('AIM', label, tuple(round(x, 4) for x in cam_space(mat @ Matrix.Translation(Vector(p))).to_translation()))
for o in list(bpy.data.objects):
    if o not in (rig, mesh): bpy.data.objects.remove(o, do_unlink=True)
clip_names = {c[0] for c in CLIPS}
for act in list(bpy.data.actions):
    if act.name not in clip_names: bpy.data.actions.remove(act)
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = 1.0
rig.animation_data.action = bpy.data.actions['idle']
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); mesh.select_set(True)
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=True, add_leaf_bones=False, bake_anim=True,
    bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
    bake_anim_simplify_factor=1.0, path_mode='STRIP', axis_forward='-Z', axis_up='Y', global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE')
print('EXPORTED', out_path)
