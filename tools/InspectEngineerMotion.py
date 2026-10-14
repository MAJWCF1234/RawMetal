"""Render Engineer source poses and record hand trajectories before baking.

Run with Blender --python this-file -- --library <engineer-direct> --output <dir>.
This inspects the original FBX skinning, without anatomical retargeting.
"""
import argparse
import json
from pathlib import Path
import sys
import bpy
from mathutils import Vector

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--library', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
args.output.mkdir(parents=True, exist_ok=True)
catalog = json.loads((args.library / 'clip-catalog.json').read_text())
base = args.library / 'special/texting-while-standing-texting-on-a-smart-phone-while-standing.fbx'
selections = [
    ('texting', 'Texting While Standing - Texting On A Smart Phone While Standing', [0, .5]),
    ('carry', 'Walk With Briefcase - Male Walk With Briefcase', [0, .5]),
    ('reach-underarm', 'Unarmed Equip Underarm', [0, .25, .5, .75, 1]),
    ('reach-shoulder', 'Unarmed Equip Over Shoulder', [0, .25, .5, .75, 1]),
]
trajectories = []
for label, group, phases in selections:
    entry = next(c for c in catalog['clips'] if c['group'] == group)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(args.library / entry['file']))
    target_rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    action = target_rig.animation_data.action
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    if not meshes:
        target_rig.hide_render = True
        bpy.ops.import_scene.fbx(filepath=str(base))
        rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE' and o != target_rig)
        rig.animation_data.action = action
        if getattr(action, 'slots', None):
            rig.animation_data.action_slot = action.slots[0]
        meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    else:
        rig = target_rig
    scene = bpy.context.scene
    scene.render.engine = 'BLENDER_EEVEE'
    scene.render.resolution_x = 600
    scene.render.resolution_y = 800
    scene.render.resolution_percentage = 100
    scene.world = bpy.data.worlds.new('source-inspection-world')
    scene.world.color = (.10, .10, .10)
    material = bpy.data.materials.new('inspection-clay')
    material.diffuse_color = (.38, .50, .62, 1)
    for mesh in meshes:
        mesh.data.materials.clear()
        mesh.data.materials.append(material)
        for polygon in mesh.data.polygons:
            polygon.material_index = 0
    camera_data = bpy.data.cameras.new('source-camera')
    camera = bpy.data.objects.new('source-camera', camera_data)
    scene.collection.objects.link(camera)
    scene.camera = camera
    camera_data.type = 'ORTHO'
    start, end = action.frame_range
    samples = []
    for i in range(101):
        phase = i / 100
        frame = start + phase * (end - start)
        scene.frame_set(int(frame), subframe=frame - int(frame))
        hips = rig.pose.bones['mixamorig:Hips'].head
        samples.append(dict(phase=phase, seconds=phase * (end - start) / entry['fps'],
                            left=list(rig.pose.bones['mixamorig:LeftHand'].head - hips),
                            right=list(rig.pose.bones['mixamorig:RightHand'].head - hips)))
    trajectories.append(dict(label=label, file=entry['file'], fps=entry['fps'], samples=samples))
    for phase in phases:
        frame = start + phase * (end - start)
        scene.frame_set(int(frame), subframe=frame - int(frame))
        bpy.context.view_layer.update()
        graph = bpy.context.evaluated_depsgraph_get()
        points = [mesh.matrix_world @ v.co for mesh in meshes
                  for v in mesh.evaluated_get(graph).data.vertices]
        low = Vector(tuple(min(p[j] for p in points) for j in range(3)))
        high = Vector(tuple(max(p[j] for p in points) for j in range(3)))
        center = (low + high) / 2
        height = high.z - low.z
        camera_data.ortho_scale = max(height * 1.25, 1.85)
        camera.location = center + Vector((height * .6, -height * 2, height * .25))
        camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
        for name, offset, energy in [('key', (2, -3, 3), 400), ('fill', (-2, -1, 2), 150)]:
            light_data = bpy.data.lights.get(name) or bpy.data.lights.new(name, 'AREA')
            light = bpy.data.objects.get(name)
            if light is None:
                light = bpy.data.objects.new(name, light_data)
                scene.collection.objects.link(light)
            light.location = center + Vector(offset)
            light.rotation_euler = (center - light.location).to_track_quat('-Z', 'Y').to_euler()
            light_data.energy = energy
            light_data.size = 2
        scene.render.filepath = str(args.output / f'{label}-{round(phase * 100):03d}.png')
        bpy.ops.render.render(write_still=True)
(args.output / 'hand-trajectories.json').write_text(json.dumps(trajectories, indent=2))
print('PASS: rendered original skinned poses and sampled both hand trajectories')
