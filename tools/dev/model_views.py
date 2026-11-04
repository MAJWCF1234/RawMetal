"""Render a model from its four sides, for deciding which way it faces.

Blender (headless) imports each OBJ and renders it with the workbench engine
from the OBJ's +x, -x, +z and -z sides, then a 2x2 sheet is labelled with
those axes. The game draws a fixture at yaw 0 with OBJ +z pointing to world +y
(south) and OBJ +x to world +x (east); see Scene3D facility().

usage: blender -b --python tools/dev/model_views.py -- OUTDIR NAME=PATH [NAME=PATH ...]
"""
import glob, json, math, os, sys
import bpy
from mathutils import Vector

args = sys.argv[sys.argv.index('--') + 1:]
out = os.path.abspath(args[0])
os.makedirs(out, exist_ok=True)
scene = bpy.context.scene
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.light = 'STUDIO'
scene.display.shading.color_type = 'TEXTURE'
scene.display.shading.show_cavity = True
scene.render.resolution_x = scene.render.resolution_y = 384
scene.render.film_transparent = False
scene.world = scene.world or bpy.data.worlds.new('w')
# OBJ axis -> Blender direction after the Y-up import (OBJ z becomes Blender -y).
SIDES = {'+x': Vector((1, 0, 0)), '-x': Vector((-1, 0, 0)), '+z': Vector((0, -1, 0)), '-z': Vector((0, 1, 0))}
for spec in args[1:]:
    name, path = spec.split('=', 1)
    for o in list(bpy.data.objects): bpy.data.objects.remove(o, do_unlink=True)
    bpy.ops.wm.obj_import(filepath=path, forward_axis='NEGATIVE_Z', up_axis='Y')
    meshes = [o for o in bpy.data.objects if o.type == 'MESH']
    if not meshes: continue
    # Textures: the library's own json map, else a PNG named after the material.
    folder = os.path.dirname(path); table = {}
    meta = os.path.splitext(path)[0] + '.json'
    if os.path.exists(meta): table = json.load(open(meta)).get('materials', {})
    for o in meshes:
        for slot in o.material_slots:
            mat = slot.material
            if mat is None: continue
            key = mat.name.split('.')[0]
            image = table.get(key)
            image = os.path.join(folder, image) if image else os.path.join(folder, key + '.png')
            if not os.path.exists(image):
                found = glob.glob(os.path.join(folder, key + '*.png')) + glob.glob(os.path.join(folder, key + '*.jpg'))
                if not found: continue
                image = found[0]
            mat.use_nodes = True; nodes = mat.node_tree.nodes
            tex = nodes.new('ShaderNodeTexImage'); tex.image = bpy.data.images.load(image, check_existing=True)
            bsdf = next((n for n in nodes if n.type == 'BSDF_PRINCIPLED'), None)
            if bsdf: mat.node_tree.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
            nodes.active = tex
    lo = Vector((1e9,) * 3); hi = Vector((-1e9,) * 3)
    for o in meshes:
        for c in o.bound_box:
            w = o.matrix_world @ Vector(c); lo = Vector(map(min, lo, w)); hi = Vector(map(max, hi, w))
    centre = (lo + hi) / 2; size = max((hi - lo).length, 1e-3)
    cam_data = bpy.data.cameras.new('c'); cam_data.type = 'ORTHO'; cam_data.ortho_scale = size * 1.05
    cam = bpy.data.objects.new('c', cam_data); scene.collection.objects.link(cam); scene.camera = cam
    for label, d in SIDES.items():
        cam.location = centre + d * size * 2 + Vector((0, 0, size * .35))
        cam.rotation_euler = (centre - cam.location).to_track_quat('-Z', 'Y').to_euler()
        scene.render.filepath = os.path.join(out, '%s_%s.png' % (name, label.replace('+', 'p').replace('-', 'm')))
        bpy.ops.render.render(write_still=True)
