# The sniper rifle: the PSX Weapon Pack's Remington M700 (a bolt-action hunting rifle: Barrel, Base (stock and receiver), Bolt, Scope).
#   blender -b --python tools/player/import_remington700.py -- Remington-m700.fbx OUTDIR
# Writes OUTDIR/sniper.obj      the whole rifle in the Remington 870's frame (bore toward -z, +y up; the shotgun's hold, the pickup, the
#                               third person and other players' views carry it as they carry the shotgun)
#        OUTDIR/sniper-fp.fbx   first person: the MAC-11's engine frame (metres, +z the bore, +y up, +x the gun's left), origin the
#                               scope's eyepiece; two nodes: Sniper (barrel, stock, scope) and MacBolt (the bolt, moved by the bake)
#        OUTDIR/rifle.json      the points tools/player/build_claire_fp_remington.py fits to the pack's rig
# The rifle's own units: bore +y, up +z, +x the gun's right (Blender, as the pack saved it); 9.7 units long.
import bpy, sys, os, json, math
from mathutils import Matrix, Vector
fbx, outdir = sys.argv[sys.argv.index('--')+1:][:2]
os.makedirs(outdir, exist_ok=True)
S = .10                                     # metres per pack unit: a 97 cm rifle
EYE = Vector((0, -2.124, 1.82))             # the eyepiece's centre, in the pack's units: the origin of the first-person frame
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
for o in bpy.data.objects:
    if o.type != 'MESH': bpy.data.objects.remove(o, do_unlink=True)
for o in bpy.data.objects:
    o.data = o.data.copy(); o.data.transform(o.matrix_world); o.matrix_world = Matrix(); o.parent = None
def join(names, new):
    bpy.ops.object.select_all(action='DESELECT')
    for n in names: bpy.data.objects[n].select_set(True)
    bpy.context.view_layer.objects.active = bpy.data.objects[names[0]]
    if len(names) > 1: bpy.ops.object.join()
    o = bpy.context.view_layer.objects.active; o.name = new; o.data.name = new; return o
def export(path, names):
    bpy.ops.object.select_all(action='DESELECT')
    for n in names: bpy.data.objects[n].select_set(True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, add_leaf_bones=False, bake_anim=False, path_mode='STRIP', axis_forward='-Z', axis_up='Y',
        global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='OFF')
    print('EXPORTED', path, os.path.getsize(path))
# --- the world model, in the Remington's frame: the wrist lands on the shotgun's trigger hand, the bore at the shotgun's height ---
SO = .95
grip = Vector((0, -2.75, 1.0)); bore_z = 1.52
fit_obj = Matrix.Translation((-.064, -1.25 - SO*grip.y, 1.29 - SO*bore_z)) @ Matrix.Diagonal((SO, SO, SO, 1))
whole = {n: bpy.data.objects[n].data.copy() for n in ('Barrel', 'Base', 'Bolt', 'Scope')}
dup = []
for n, d in whole.items():
    o = bpy.data.objects.new('w_'+n, d); bpy.context.scene.collection.objects.link(o); o.data.transform(fit_obj); dup.append(o.name)
w = join(dup, 'Sniper'); 
bpy.ops.object.select_all(action='DESELECT'); w.select_set(True)
bpy.ops.wm.obj_export(filepath=os.path.join(outdir, 'sniper.obj'), export_selected_objects=True, forward_axis='NEGATIVE_Z', up_axis='Y',
    export_materials=False, export_normals=True, export_uv=True)
bpy.data.objects.remove(w, do_unlink=True)
# --- the first-person model: export axes (x_e, -z_e, y_e) with x_e the gun's left, y_e up, z_e the bore; origin at the eyepiece ---
# rifle (x, y, z) -> engine (-x, z, y), then the metres; Blender export (x_e, -z_e, y_e)
fit_fp = Matrix.Diagonal((S, S, S, 1)) @ Matrix(((-1, 0, 0, 0), (0, -1, 0, -EYE.y*-1), (0, 0, 1, -EYE.z), (0, 0, 0, 1)))
# (X, Y, Z) = (-x, -(y - EYE.y), z - EYE.z) * S: Y is -z_e (the bore forward is -Y), Z is y_e
fit_fp = Matrix.Diagonal((S, S, S, 1)) @ Matrix(((-1, 0, 0, 0), (0, -1, 0, EYE.y), (0, 0, 1, -EYE.z), (0, 0, 0, 1)))
for o in bpy.data.objects: o.data.transform(fit_fp)
body = join(['Barrel', 'Base', 'Scope'], 'Sniper'); bolt = bpy.data.objects['Bolt']; bolt.name = 'MacBolt'; bolt.data.name = 'MacBolt'
export(os.path.join(outdir, 'sniper-fp.fbx'), ['Sniper', 'MacBolt'])
def eng(rx, ry, rz):    # a point in the rifle's units -> engine metres
    return [round(-rx*S, 5), round((rz-EYE.z)*S, 5), round((ry-EYE.y)*S, 5)]
ref = {'eye': [0, 0, 0], 'front': eng(0, .708, 1.82), 'muzzle': eng(0, 4.87, 1.52),
       'trigger': eng(0, -1.95, 1.15), 'wrist': eng(0, -2.75, 1.0), 'forend': eng(0, .5, .95),
       'knob': eng(.3, -1.66, 1.26), 'bolt_axis': eng(0, -1.29, 1.38), 'bolt_back': .09}
json.dump(ref, open(os.path.join(outdir, 'rifle.json'), 'w'), indent=1)
print('REF', ref)
for o in bpy.data.objects:
    vs = [v.co for v in o.data.vertices]; print('PART', o.name, [tuple(round(min(v[i] for v in vs), 3) for i in range(3)), tuple(round(max(v[i] for v in vs), 3) for i in range(3))])
