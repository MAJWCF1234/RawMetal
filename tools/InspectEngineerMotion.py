"""Render squad source poses and record hand trajectories before baking.

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
parser.add_argument('--character', choices=('engineer', 'medic', 'sharpshooter'), default='engineer')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
args.output.mkdir(parents=True, exist_ok=True)
catalog = json.loads((args.library / 'clip-catalog.json').read_text())
base = args.library / 'special/texting-while-standing-texting-on-a-smart-phone-while-standing.fbx'
selections = [
    ('texting', 'Texting While Standing - Texting On A Smart Phone While Standing',
     [0, .03, .06, .09, .12, .5, .88, .91, .94, .97, 1]),
    ('reach-underarm', 'Unarmed Equip Underarm', [0, .25, .5, .75, 1]),
    ('reach-shoulder', 'Unarmed Equip Over Shoulder', [0, .25, .5, .75, 1]),
]
if args.character == 'medic':
    base = args.library / 'special/opening-a-lid-opening-the-lid-on-a-chest-or-box.fbx'
    selections = [
        ('kit-open', 'Opening A Lid - Opening The Lid On A Chest Or Box', [0, .5, 1]),
        ('cpr', 'Administering Cpr - Administering Cpr To A Victim On The Ground', [.25, .5, .75]),
        ('pistol', '@pistol-handgun-locomotion-pack/pistol idle.fbx', [.5]),
    ]
elif args.character == 'sharpshooter':
    base = args.library / 'special/prone-idle-lying-down-prone-with-rifle.fbx'
    selections = [
        ('rifle-aim', '@pro-rifle-pack/idle aiming.fbx', [.5]),
        ('crouch-aim', '@pro-rifle-pack/idle crouching aiming.fbx', [.5]),
        ('prone-aim', 'Prone Idle - Lying Down Prone With Rifle', [.5]),
    ]
trajectories = []
for label, group, phases in selections:
    entry = next(c for c in catalog['clips']
                 if (c['file'] == group[1:] if group.startswith('@') else c['group'] == group))
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(args.library / entry['file']))
    target_rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    action = target_rig.animation_data.action
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    transferred = not meshes
    if not meshes:
        target_rig.hide_render = True
        # Pack exports use the pack character's bind pose. Mixing that action
        # with a separately exported clip's skin can rotate limbs incorrectly.
        skin_source = (args.library / Path(group[1:]).parent /
                       (args.character + '-full-fingers.fbx') if group.startswith('@') else base)
        bpy.ops.import_scene.fbx(filepath=str(skin_source))
        rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE' and o != target_rig)
        rig.animation_data.action = None
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
    material.use_nodes = False
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
    def set_source_frame(frame):
        scene.frame_set(int(frame), subframe=frame - int(frame))
        bpy.context.view_layer.update()
        if transferred:
            # F-curves encode rotations in the exporting rig's local bone
            # basis. Copying them onto another FBX bind pose changes the pose.
            # Preserve the source's complete evaluated bone matrices instead.
            relative = rig.matrix_world.inverted() @ target_rig.matrix_world
            desired = {b.name: relative @ b.matrix for b in target_rig.pose.bones}
            for bone in rig.pose.bones:
                rest = bone.bone.matrix_local
                if bone.parent:
                    bone.matrix_basis = bone.bone.convert_local_to_pose(
                        desired[bone.name], rest, parent_matrix=desired[bone.parent.name],
                        parent_matrix_local=bone.parent.bone.matrix_local, invert=True)
                else:
                    bone.matrix_basis = bone.bone.convert_local_to_pose(desired[bone.name], rest, invert=True)
            bpy.context.view_layer.update()
            assert max(max(abs(v) for row in (b.matrix - desired[b.name]) for v in row)
                       for b in rig.pose.bones) < .001
    samples = []
    for i in range(101):
        phase = i / 100
        frame = start + phase * (end - start)
        set_source_frame(frame)
        hips = rig.pose.bones['mixamorig:Hips'].head
        samples.append(dict(phase=phase, seconds=phase * (end - start) / entry['fps'],
                            left=list(rig.pose.bones['mixamorig:LeftHand'].head - hips),
                            right=list(rig.pose.bones['mixamorig:RightHand'].head - hips)))
    trajectories.append(dict(label=label, file=entry['file'], fps=entry['fps'], samples=samples))
    for phase in phases:
        frame = start + phase * (end - start)
        set_source_frame(frame)
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
        if args.character == 'medic' and label == 'pistol':
            center = rig.matrix_world @ ((rig.pose.bones['mixamorig:LeftHand'].head
                                          + rig.pose.bones['mixamorig:RightHand'].head) / 2)
            camera_data.ortho_scale = .35
            camera.location = center + Vector((.35, -.45, .20))
            camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
            scene.render.filepath = str(args.output / 'pistol-fingers.png')
            bpy.ops.render.render(write_still=True)
        elif args.character == 'sharpshooter' and label == 'rifle-aim':
            center = rig.matrix_world @ rig.pose.bones['mixamorig:RightHand'].head
            camera_data.ortho_scale = .32
            camera.location = center + Vector((-.30, -.45, .25))
            camera.rotation_euler = (center - camera.location).to_track_quat('-Z', 'Y').to_euler()
            scene.render.filepath = str(args.output / 'rifle-fingers.png')
            bpy.ops.render.render(write_still=True)
(args.output / 'hand-trajectories.json').write_text(json.dumps(trajectories, indent=2))
print('PASS: rendered original skinned poses and sampled both hand trajectories')
