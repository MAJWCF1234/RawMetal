import bpy, sys, os, shutil
from mathutils import Vector

work = sys.argv[sys.argv.index('--') + 1]
bpy.ops.wm.open_mainfile(filepath=os.path.join(work, 'skinbot-assembly.blend'))
out = os.path.join(work, 'export')
os.makedirs(out, exist_ok=True)
shutil.copy2(os.path.join(work, 'base_glow.png'), os.path.join(out, 'skinbot_skin.png'))
picture = bpy.data.images.load(os.path.join(out, 'skinbot_skin.png'))
for m in bpy.data.materials:
    if m.use_nodes:
        for n in m.node_tree.nodes:
            if n.type == 'TEX_IMAGE':
                n.image = picture
legs = {o.name: list(o['joints']) for o in bpy.context.scene.objects if 'joints' in o.keys()}
meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:
    o.select_set(True)
bpy.context.view_layer.objects.active = meshes[0]
bpy.ops.object.join()
obj = bpy.context.view_layer.objects.active
obj.name = 'skinbot'
pts = [obj.matrix_world @ v.co for v in obj.data.vertices]
size = [max(p[i] for p in pts) - min(p[i] for p in pts) for i in range(3)]
# The joints as the importer will leave the model: footprint centred, base at 0, Y up (x, up, -forward).
cx = (min(p.x for p in pts) + max(p.x for p in pts)) / 2
cy = (min(p.y for p in pts) + max(p.y for p in pts)) / 2
low = min(p.z for p in pts)
lines = ['// Made by tools/creatures_assemble_skinbot_draft.py: each leg\'s joints, root to blade tip, in the mesh\'s own space',
         '// (metres at scale 1; x right, y up, z forward). A section k runs from joint k to joint k+1.',
         'inline constexpr float SkinbotJoints[4][4][3]={']
for name in ('arm_L', 'arm_R', 'rear_L', 'rear_R'):
    j = legs[name]
    lines.append(' {' + ','.join('{%.5ff,%.5ff,%.5ff}' % (j[3 * k] - cx, j[3 * k + 2] - low, -(j[3 * k + 1] - cy)) for k in range(4)) + '}, // ' + name)
lines.append('};')
open(os.path.join(out, 'SkinbotRigData.inc'), 'w').write('\n'.join(lines) + '\n')
print('SIZE', [round(s, 3) for s in size], 'TRIS', sum(len(p.vertices) - 2 for p in obj.data.polygons))
bpy.ops.wm.obj_export(filepath=os.path.join(out, 'skinbot.obj'), export_selected_objects=True, path_mode='STRIP',
                      export_materials=True)
