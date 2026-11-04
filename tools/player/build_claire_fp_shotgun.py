# Claire's first-person shotgun: her arms refitted onto the RetroWeaponPack's
# animated FP arm rig, every shotgun clip baked from the pack (arms, movement
# and gun layers, constraints evaluated) into the pack camera's space, and the
# game's Remington placed by reference points that ride the pack's gun.
#   blender -b --python tools/player/build_claire_fp_shotgun.py -- PACK.blend Claire.fbx Remington.fbx OUT.fbx
# Output (y-up, metres, camera at the origin looking down -z, x right):
#   ArmsMesh  her arms, skinned to the pack's deform bones (flat hierarchy)
#   gun_o/x/y/z, pump_o/x/y/z  points that carry the Remington's origin and unit
#     axes (in the engine's Remington coordinates) for the gun and its pump
#   one action per clip (see CLIPS), sampled every frame.
import bpy, sys, math
from mathutils import Matrix, Vector
pack_path, claire_path, rem_path, out_path = sys.argv[sys.argv.index('--')+1:][:4]
CLIPS = [  # name, arms action, movement action, gun action, first, last
 ('idle', 'arms_basePose', 'movement_breathing', 'shotgun01_BasePose', 1, 160),
 ('aim', 'arms_basePose', 'movement_breathing', 'shotgun01_AimPose', 1, 160),
 ('walk', 'arms_basePose', 'movement_walking', 'shotgun01_BasePose', 1, 40),
 ('walk_aim', 'arms_basePose', 'movement_walking_Aiming', 'shotgun01_AimPose', 1, 40),
 ('run', 'arms_basePose', 'movement_running', 'shotgun01_BasePose', 1, 20),
 ('crouch', 'arms_basePose', 'movement_breathing', 'shotgun01_CrouchPose', 1, 160),
 ('fire', 'arms_Fire', 'movement_Fire', 'shotgun01_fire', 1, 24),
 ('fire_aim', 'arms_Fire', 'movement_Fire_Aiming', 'shotgun01_fire_Aiming', 1, 25),
 ('reload_start', 'arms_ReloadStart', 'movement_ReloadStart', 'shotgun01_ReloadStart', 1, 11),
 ('reload_step', 'arms_ReloadStep', 'movement_ReloadStep', 'shotgun01_ReloadStep', 1, 17),
 ('reload_end', 'arms_ReloadEnd', 'movement_ReloadEnd', 'shotgun01_ReloadEnd', 1, 33),
 ('draw', 'arms_DrawWeapon', 'movement_DrawWeapon', 'shotgun01_BasePose', 1, 25),
 ('hide', 'arms_HideWeapon', 'movement_hideWeapon', 'shotgun01_BasePose', 1, 12),
]
ENGINE_FOV = 2*math.atan(1/1.3)        # Projection::horizontal: x/z = 1/1.3 at the edge

bpy.ops.wm.open_mainfile(filepath=pack_path)
scene = bpy.context.scene
arms = bpy.data.objects['Arms_Armature']; gun = bpy.data.objects['Shotgun_01_Armature']
mover = bpy.data.objects['Empty']; cam = bpy.data.objects['Camera']
for o in (arms, gun, mover):
    for t in o.animation_data.nla_tracks: t.mute = True
def use(o, name):
    a = bpy.data.actions[name]; o.animation_data.action = a
    if a.slots: o.animation_data.action_slot = a.slots[0]
fov = cam.data.angle_x if cam.data.sensor_fit != 'VERTICAL' else cam.data.angle
k = math.tan(ENGINE_FOV/2)/math.tan(fov/2)   # widen the pack's framing to the engine's lens
print('PACK FOV', math.degrees(fov), 'fps', scene.render.fps, 'k', k)
PUSH = 0.0   # metres back along the view; 0 keeps the pack's own distances
# Camera space (x right, y up, z back) -> export space (Blender, so FBX y-up comes out as camera space)
TO_EXPORT = Matrix(((1, 0, 0, 0), (0, 0, -1, 0), (0, 1, 0, 0), (0, 0, 0, 1)))
UNIT = .01                                   # the pack works in centimetres
def cam_space(world_m):
    m = cam.matrix_world.inverted() @ world_m
    # Uniform only: a non-uniform squeeze (the pack's 90-degree lens to the
    # engine's 75) thinned her arms into twigs. Units, then the engine's
    # narrower lens framed by pushing everything back, not by squashing it.
    m = Matrix.Diagonal((UNIT, UNIT, UNIT, 1)) @ m
    return TO_EXPORT @ Matrix.Translation((0, 0, -PUSH)) @ m

# --- deform bones and their rest pose -----------------------------------------
pack_mesh = bpy.data.objects['FPS_Arms_Mesh']
deform = [b.name for b in arms.data.bones if not b.name.startswith('ctrl_') and b.name not in ('root', 'camera')]   # every bone her weights may use
print('DEFORM', len(deform))

# --- Claire's arms, refitted onto the pack rig's rest pose ---------------------
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
# Girth of a segment: mean distance from its bone line of the vertices it
# mostly owns. Her arms take the pack arms' girth (a slim forearm stretched to
# the pack's length read as a twig); the hands keep their palm-width ratio.
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
        gp = girth(pack_mesh, pg, rg(its)[0], rg(end_its)[0]) or girth(pack_mesh, [g[:-1]+'l' for g in pg], rg(its[:-1]+'l')[0], rg(end_its[:-1]+'l')[0]);
        gc = girth(claire_mesh_for_girth, [her], ch(her)[0], ch(end_her)[0])
        seg_girth[her] = (gp/gc) if gp and gc else arm
        print('GIRTH', her, 'pack', gp, 'claire', gc, 'ratio', seg_girth[her], 'length ratio', arm)
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
# Weights onto the pack's bones; the forearm is shared out along its twist
# bones (they carry the hand's turn down the arm, as the radius does).
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
# Her elbow was hard-weighted (all upper arm or all forearm): the pack's deep
# bends folded it into a crease. Blend the two across a band at the joint.
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
used = {g.name for g in mesh.vertex_groups}
keep_names = {mesh.name}
for o in list(bpy.data.objects):
    if o.name not in keep_names and o.name in {n for n in claire_names}: bpy.data.objects.remove(o, do_unlink=True)
print('CLAIRE ARMS', len(mesh.data.vertices), 'verts, bones', sorted(used))

# --- Remington fitted onto the pack's gun -------------------------------------
before = set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=rem_path)
for o in [o for o in bpy.data.objects if o not in before]: bpy.data.objects.remove(o, do_unlink=True)
gw = gun.matrix_world
def mesh_centre(name):
    o = bpy.data.objects[name]; pts = [o.matrix_world @ v.co for v in o.data.vertices]
    lo = Vector([min(p[i] for p in pts) for i in range(3)]); hi = Vector([max(p[i] for p in pts) for i in range(3)])
    return gw.inverted() @ ((lo + hi)/2)      # in the gun armature's space
pack_trigger, pack_pump = mesh_centre('Trigger_Mesh'), mesh_centre('Forearm_Mesh')
def eng_to_blender(p): return Vector((p[0], -p[2], p[1]))   # engine Remington (x,y,z) = Blender (x,z,-y)
rem_trigger = eng_to_blender((-.064, 1.0, .45)); rem_pump = eng_to_blender((-.064, 1.22, -2.25))
R = Matrix(((0, 1, 0), (-1, 0, 0), (0, 0, 1)))   # Remington +y (bore) -> gun +x, +z -> +z, +x -> -y
scale = (pack_pump - pack_trigger).length/(rem_pump - rem_trigger).length
fit3 = R*scale; offset = pack_trigger - fit3 @ rem_trigger
FIT = Matrix.Translation(offset) @ fit3.to_4x4()
print('FIT scale', scale, 'pump miss', (FIT @ rem_pump - pack_pump).length)

# --- export armature: flat deform bones and the gun points --------------------
bpy.ops.object.select_all(action='DESELECT')
rig_data = bpy.data.armatures.new('FPRig'); rig = bpy.data.objects.new('FPRig', rig_data)
scene.collection.objects.link(rig); bpy.context.view_layer.objects.active = rig
scene.frame_set(1); use(arms, 'arms_basePose'); use(mover, 'movement_basePose'); use(gun, 'shotgun01_BasePose'); scene.frame_set(1)
points = ['gun_o', 'gun_x', 'gun_y', 'gun_z', 'pump_o', 'pump_x', 'pump_y', 'pump_z']
bpy.ops.object.mode_set(mode='EDIT')
for n in deform:
    b = rig_data.edit_bones.new(n); rb = arms.data.bones[n]
    b.head = (arms.matrix_world @ rb.head_local); b.tail = (arms.matrix_world @ rb.tail_local)
    b.roll = 0
for n in points:
    b = rig_data.edit_bones.new(n); b.head = (0, 0, 0); b.tail = (0, 0, 1)
bpy.ops.object.mode_set(mode='OBJECT')
# The mesh binds to the rig's rest (the pack's rest pose, world space), then the
# rig's animation carries every bone into camera space.
mesh.parent = rig; mod = mesh.modifiers.new('Armature', 'ARMATURE'); mod.object = rig
mesh.name = 'ArmsMesh'; mesh.data.name = 'ArmsMesh'
rig.animation_data_create()
fps = scene.render.fps
for name, a, m, g, first, last in CLIPS:
    use(arms, a); use(mover, m); use(gun, g)
    act = bpy.data.actions.new(name); rig.animation_data.action = act
    for f in range(first, last + 1):
        scene.frame_set(f)
        dg = bpy.context.evaluated_depsgraph_get(); ae = arms.evaluated_get(dg); ge = gun.evaluated_get(dg)
        frame_no = f - first + 1
        for n in deform:
            world = ae.matrix_world @ ae.pose.bones[n].matrix
            pb = rig.pose.bones[n]; pb.matrix = cam_space(world)
            pb.keyframe_insert('location', frame=frame_no); pb.keyframe_insert('rotation_quaternion', frame=frame_no); pb.keyframe_insert('scale', frame=frame_no)
        main = ge.pose.bones['Main']; pump = ge.pose.bones['Forearm']
        main_rest = gun.data.bones['Main'].matrix_local; pump_rest = gun.data.bones['Forearm'].matrix_local
        gun_world = ge.matrix_world @ main.matrix @ main_rest.inverted() @ FIT
        pump_world = ge.matrix_world @ pump.matrix @ pump_rest.inverted() @ FIT
        for prefix, mat in (('gun', gun_world), ('pump', pump_world)):
            for axis, p in (('o', (0, 0, 0)), ('x', (1, 0, 0)), ('y', (0, 1, 0)), ('z', (0, 0, 1))):
                q = cam_space(mat @ Matrix.Translation(eng_to_blender(p))).to_translation()
                pb = rig.pose.bones[f'{prefix}_{axis}']; pb.matrix = Matrix.Translation(q)
                pb.keyframe_insert('location', frame=frame_no); pb.keyframe_insert('rotation_quaternion', frame=frame_no)
    print('CLIP', name, last - first + 1, 'frames', round((last - first + 1)/fps, 3), 's')
# Leave only the rig and her arms, and export.
for o in list(bpy.data.objects):
    if o not in (rig, mesh): bpy.data.objects.remove(o, do_unlink=True)
# Only our clips go out, at metres as written (the pack's scene is in centimetres).
clip_names = {c[0] for c in CLIPS}
for act in list(bpy.data.actions):
    if act.name not in clip_names: bpy.data.actions.remove(act)
scene.unit_settings.system = 'METRIC'; scene.unit_settings.scale_length = 1.0
rig.animation_data.action = bpy.data.actions['idle']
bpy.ops.object.select_all(action='DESELECT'); rig.select_set(True); mesh.select_set(True)
bpy.ops.export_scene.fbx(filepath=out_path, use_selection=True, add_leaf_bones=False, bake_anim=True,
    bake_anim_use_all_actions=True, bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
    bake_anim_simplify_factor=0, path_mode='STRIP', axis_forward='-Z', axis_up='Y', global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE')
print('EXPORTED', out_path)
