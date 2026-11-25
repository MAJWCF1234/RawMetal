# The sniper's .308 Winchester case and cartridge, from the purchased RetroWeaponPack
# Projectiles (its 5.56 mm shell and round, the pack's only bottlenecked rifle case),
# drawn out to .308 size. Blender, headless; nothing leaves the machine.
#   blender -b --python tools/player/import_308.py -- Projectiles.fbx OUTDIR
# Writes into OUTDIR (same texture atlas as brass.fbx, so both bind brass.png):
#   rifle-case.fbx    the spent case: metres, its axis along +z, mouth at +z, centred
#                     on its middle; 51.2 mm long, 11.9 mm across the body (.308 Win)
#   rifle-round.fbx   the loaded round, bullet at +z: 71.1 mm long, same body width
import bpy, sys, os
from mathutils import Matrix
args = sys.argv[sys.argv.index('--')+1:]
proj_fbx, out = args[:2]
os.makedirs(out, exist_ok=True)

def export(path, objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, add_leaf_bones=False, bake_anim=False, path_mode='STRIP',
        axis_forward='-Z', axis_up='Y', global_scale=1.0, apply_unit_scale=False, apply_scale_options='FBX_SCALE_NONE', mesh_smooth_type='OFF')
    print('EXPORTED', path, os.path.getsize(path))

def one(name, new_name, length, width):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=proj_fbx)
    for o in list(bpy.data.objects):
        if o.name != name: bpy.data.objects.remove(o, do_unlink=True)
    o = bpy.data.objects[name]
    o.data = o.data.copy(); o.data.transform(o.matrix_world); o.matrix_world = Matrix(); o.parent = None
    pts = [v.co for v in o.data.vertices]
    lo = min(p.x for p in pts); hi = max(p.x for p in pts); mid = (lo + hi) / 2
    # Which end is the head: the rim end is the wider one.
    def spread(end):
        near = [p for p in pts if abs(p.x - end) < (hi - lo) * 0.08]
        return max((p.y * p.y + p.z * p.z) ** 0.5 for p in near)
    head_low = spread(lo) >= spread(hi)
    cy = (max(p.y for p in pts) + min(p.y for p in pts)) / 2; cz = (max(p.z for p in pts) + min(p.z for p in pts)) / 2
    across = max(max(p.y for p in pts) - min(p.y for p in pts), max(p.z for p in pts) - min(p.z for p in pts))
    k = length / (hi - lo); w = width / across
    s = k if head_low else -k
    # Pack +x (head to mouth) -> engine +z (Blender -y); the cross-section scaled to the width.
    o.data.transform(Matrix(((0, w, 0, 0), (-s, 0, 0, 0), (0, 0, w, 0), (0, 0, 0, 1))) @ Matrix.Translation((-mid, -cy, -cz)))
    o.name = new_name; o.data.name = new_name
    print('PART', new_name, 'faces', len(o.data.polygons), 'head at low x' if head_low else 'head at high x',
          'length %.4f width %.4f' % (length, width))
    export(os.path.join(out, new_name.lower() + '.fbx'), [o])

one('5.56mm_Shell_Mesh', 'Rifle-Case', 0.0512, 0.0119)
one('5.56mm_Bullet_Mesh', 'Rifle-Round', 0.0711, 0.0119)
