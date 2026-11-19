# The MAC-11 machine pistol, its .380 box and spent brass, from the purchased
# packs into engine models (Blender, headless; nothing leaves the machine).
#   blender -b --python tools/player/import_mac11.py -- MAC-11.blend Auto_Ammo_240.fbx Auto_Ammo_240px.png Projectiles.fbx Projectiles_Albedo.png OUTDIR
# Writes into OUTDIR:
#   mac11.fbx / mac11.png      the gun in engine coordinates: metres, the grip's
#                              top at the origin, +z the muzzle, +y up, +x the
#                              gun's LEFT (as medic-handgun.obj). Nodes MacBody,
#                              MacMagazine (part 8) and MacBolt (the cocking knob,
#                              part 9); the texture at 256 px.
#   mac11-ammo.fbx / .png      the pack's automatic-ammo box (PSXAmmoBoxes), 128 px
#   brass.fbx / brass.png      the pack's 9 mm case (RetroWeaponPack Projectiles):
#                              metres, its axis along +z, mouth at +z; 32 px
import bpy, sys, os
from mathutils import Matrix, Vector
args = sys.argv[sys.argv.index('--')+1:]
mac_blend, ammo_fbx, ammo_png, proj_fbx, proj_png, out = args[:6]
os.makedirs(out, exist_ok=True)
S = 0.11                                  # pack units to metres (about 0.28 m stock to muzzle)
GRIP_TOP = Vector((-0.0221, -0.479, 1.716))  # the grip's top centre, pack coordinates (z up, y the bore)
def to_engine_blender():
    # Pack (X right, Y bore, Z up) -> engine (x left, y up, z bore), written in
    # Blender axes so the FBX export (Y up, -Z forward) lands on them: Blender
    # (x_e, -z_e, y_e) = (-(X-X0), -(Y-Y0), Z-Z0) * S.
    return Matrix.Diagonal((-S, -S, S, 1)) @ Matrix.Translation(-GRIP_TOP)

def export(path, objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, add_leaf_bones=False, bake_anim=False, path_mode='STRIP',
        axis_forward='-Z', axis_up='Y', global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='OFF')
    print('EXPORTED', path, os.path.getsize(path))

def save_png(src, dst, size):
    img = bpy.data.images.load(src); img.scale(size, size)
    img.filepath_raw = dst; img.file_format = 'PNG'; img.save()
    print('TEXTURE', dst, os.path.getsize(dst))

def bake(o, m):
    o.data = o.data.copy(); o.data.transform(m @ o.matrix_world); o.matrix_world = Matrix(); o.parent = None

# --- the gun -------------------------------------------------------------------
bpy.ops.wm.open_mainfile(filepath=mac_blend)
T = to_engine_blender()
for o in list(bpy.data.objects):
    if o.type != 'MESH' or not o.material_slots or o.name.startswith('Cube'): bpy.data.objects.remove(o, do_unlink=True)
objs = list(bpy.data.objects)
for o in objs: bake(o, T)
body = [o for o in objs if o.name not in ('Magazine', 'Bolt')]
bpy.ops.object.select_all(action='DESELECT')
for o in body: o.select_set(True)
bpy.context.view_layer.objects.active = body[0]; bpy.ops.object.join(); merged = bpy.context.view_layer.objects.active
merged.name = 'MacBody'; merged.data.name = 'MacBody'
mag = bpy.data.objects['Magazine']; mag.name = 'MacMagazine'; mag.data.name = 'MacMagazine'
knob = bpy.data.objects['Bolt']; knob.name = 'MacBolt'; knob.data.name = 'MacBolt'
for o in (merged, mag, knob):
    pts = [o.matrix_world @ v.co for v in o.data.vertices]
    # Engine axes: x = Blender x, y = Blender z, z = -Blender y.
    lo = [min(p.x for p in pts), min(p.z for p in pts), min(-p.y for p in pts)]; hi = [max(p.x for p in pts), max(p.z for p in pts), max(-p.y for p in pts)]
    print('PART', o.name, 'engine box', [round(x, 4) for x in lo], [round(x, 4) for x in hi])
save_png(os.path.join(os.path.dirname(mac_blend), 'MAC-11_texture.jpg'), os.path.join(out, 'mac11.png'), 256)
export(os.path.join(out, 'mac11.fbx'), [merged, mag, knob])

# --- the .380 box ----------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=ammo_fbx)
box = [o for o in bpy.data.objects if o.type == 'MESH']
for o in box: bake(o, Matrix())
save_png(ammo_png, os.path.join(out, 'mac11-ammo.png'), 128)
export(os.path.join(out, 'mac11-ammo.fbx'), box)

# --- the brass ----------------------------------------------------------------------
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=proj_fbx)
for o in list(bpy.data.objects):
    if o.name != '9mm_Shell_Mesh': bpy.data.objects.remove(o, do_unlink=True)
case = bpy.data.objects['9mm_Shell_Mesh']
# Its axis runs along the pack's +x from the head (0) to the mouth: to engine
# +z (Blender -y), centred on its middle, at 19 mm long (a 9 mm case).
bake(case, Matrix())
pts = [v.co for v in case.data.vertices]; length = max(p.x for p in pts) - min(p.x for p in pts); mid = (max(p.x for p in pts) + min(p.x for p in pts))/2
k = 0.019/length
case.data.transform(Matrix(((0, k, 0, 0), (-k, 0, 0, 0), (0, 0, k, 0), (0, 0, 0, 1))) @ Matrix.Translation((-mid, 0, 0)))
case.name = 'Brass'; case.data.name = 'Brass'
save_png(proj_png, os.path.join(out, 'brass.png'), 64)
export(os.path.join(out, 'brass.fbx'), [case])
