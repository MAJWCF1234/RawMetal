"""Add the two missing pinky chains to Engineer's user-placed Mixamo rig.

Run in Blender with -- --source <original FBX> --output <directory>.
All existing bones and mesh vertices are preserved. Only pinky-region skin
weights are reassigned; all other authored/auto-rigged fingers retain theirs.
"""
import argparse
import json
import math
from pathlib import Path
import sys
import bpy
import numpy as np
from mathutils import Vector

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, required=True)
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
args.output.mkdir(parents=True, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(args.source))
rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
mesh = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
rig.scale = (1, 1, 1)
if rig.animation_data:
    rig.animation_data.action = None
bpy.context.view_layer.update()
mesh_to_rig = rig.matrix_world.inverted() @ mesh.matrix_world
points = np.array([list(mesh_to_rig @ v.co) for v in mesh.data.vertices])
root = Path(__file__).resolve().parents[1]
source = root / 'src/assets/actors/cleanup'
rest = np.array([list(map(float, line.split()[1:4])) for line in (source / 'engineer.obj').read_text().splitlines()
                 if line.startswith('v ')])
cal = np.frombuffer((source / 'engineer-arms.bin').read_bytes(), dtype='u1', offset=104).reshape(-1, 4)
# Mixamo retains this OBJ's corner order but its unit conversion introduces
# sub-micrometer float differences. Validate correspondence instead of
# rounded-coordinate dictionary keys, which lose points at rounding ties.
assert points.shape == rest.shape and np.max(np.abs(points - rest)) < .000002
hands = cal[:, 2] > 128
assert hands.sum() > 4000
face_weights = cal.reshape(-1, 3, 4)
hand_faces = (face_weights[:, :, 2] > 128).any(axis=1) & (face_weights[:, :, 3] == 1).all(axis=1)
hand, inverse = np.unique(np.round(rest.reshape(-1, 3, 3)[hand_faces].reshape(-1, 3), 6),
                          axis=0, return_inverse=True)
triangles = inverse.reshape(-1, 3)
edges = np.unique(np.sort(np.concatenate([triangles[:, [0, 1]], triangles[:, [1, 2]],
                                         triangles[:, [2, 0]]]), axis=1), axis=0)
parent = list(range(len(hand)))
distal = (hand[:, 1] < .710) & (hand[:, 0] > .23)
def find(index):
    while parent[index] != index:
        parent[index] = parent[parent[index]]
        index = parent[index]
    return index
for a, b in edges:
    if distal[a] and distal[b]:
        parent[find(a)] = find(b)
parts = {}
for index in np.flatnonzero(distal):
    parts.setdefault(find(index), []).append(index)
digits = [part for part in parts.values() if len(part) > 20]
assert len(digits) == 4
pinky_part = min(digits, key=lambda part: hand[part, 2].mean())
pinky = hand[pinky_part]
assert len(pinky) > 30
# Keep whole distal surfaces together, including the pinky's outermost tip.
# A z-plane selection left several tip vertices on the old ring chain.
fixed = (hand[:, 1] >= .733) | (hand[:, 0] <= .23)
member = np.zeros(len(hand))
for digit in digits:
    fixed[digit] = True
member[pinky_part] = 1
a, b = np.concatenate((edges[:, 0], edges[:, 1])), np.concatenate((edges[:, 1], edges[:, 0]))
strength = 1 / np.maximum(.003, np.linalg.norm(hand[a] - hand[b], axis=1))
total = np.bincount(a, weights=strength, minlength=len(hand))
for iteration in range(1200):
    average = np.bincount(a, weights=strength * member[b], minlength=len(hand)) / np.maximum(1, total)
    updated = np.where(fixed, member, average)
    if np.max(np.abs(updated - member)) < 1e-8:
        break
    member = updated
canonical = rest[hands].copy(); canonical[:, 0] = np.abs(canonical[:, 0])
nearest = np.argmin(np.sum((canonical[:, None] - hand[None]) ** 2, axis=2), axis=1)
memberships = np.zeros(len(points)); memberships[hands] = member[nearest]
fit_x = np.polyfit(pinky[:, 1], pinky[:, 0], 1)
fit_z = np.polyfit(pinky[:, 1], pinky[:, 2], 1)
ys = np.linspace(.725, float(pinky[:, 1].min()) + .002, 4)
chain = np.array([[np.polyval(fit_x, y), y, np.polyval(fit_z, y)] for y in ys])
before_bones = {b.name: (b.head_local.copy(), b.tail_local.copy()) for b in rig.data.bones}
rolls = {side: rig.data.bones[f'mixamorig:{side}HandRing1'].matrix_local.col[2].to_3d()
         for side in ('Left', 'Right')}
assert len(before_bones) == 57
old_names = {g.index: g.name for g in mesh.vertex_groups}
old_weights = [{old_names[g.group]: g.weight for g in v.groups} for v in mesh.data.vertices]
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
for side, sign in [('Left', 1), ('Right', -1)]:
    fitted = chain * [sign, 1, 1]
    parent = rig.data.edit_bones[f'mixamorig:{side}Hand']
    for i in range(4):
        name = f'mixamorig:{side}HandPinky{i + 1}'
        assert name not in rig.data.edit_bones
        bone = rig.data.edit_bones.new(name)
        bone.head = Vector(fitted[min(i, 3)])
        bone.tail = Vector(fitted[i + 1] if i < 3 else fitted[3] + (fitted[3] - fitted[2]) * .25)
        bone.parent = parent
        bone.use_connect = i > 0
        bone.use_deform = i < 3
        bone.align_roll(rolls[side])
        parent = bone
bpy.ops.object.mode_set(mode='OBJECT')
for name, (head, tail) in before_bones.items():
    assert (rig.data.bones[name].head_local - head).length < 1e-6
    assert (rig.data.bones[name].tail_local - tail).length < 1e-6
assert len(rig.data.bones) == 65
for side in ('Left', 'Right'):
    for i in range(3):
        mesh.vertex_groups.new(name=f'mixamorig:{side}HandPinky{i + 1}')

def closest(p, a, b):
    progress = float(np.clip(np.dot(p - a, b - a) / np.dot(b - a, b - a), 0, 1))
    return np.linalg.norm(p - a - progress * (b - a)), progress

changed = []
for index in np.flatnonzero(hands):
    p = points[index].copy()
    side = 'Left' if p[0] > 0 else 'Right'
    p[0] = abs(p[0])
    if p[1] >= .733 or memberships[index] <= 1e-8:
        continue
    # Below the web the pinky is a separate surface. Blend only near its
    # knuckle, leaving the upper palm and the neighboring ring finger intact.
    influence = float(np.clip((.733 - p[1]) / .023, 0, 1))
    influence = influence * influence * (3 - 2 * influence) * memberships[index]
    if influence <= 1e-8:
        continue
    choices = [(distance, i + progress) for i in range(3)
               for distance, progress in [closest(p, chain[i], chain[i + 1])]]
    _, progress = min(choices)
    location = float(np.clip(progress - .5, 0, 2))
    low = int(math.floor(location)); high = min(2, low + 1)
    blend = location - low
    weights = {name: value * (1 - influence) for name, value in old_weights[index].items()}
    for joint, value in [(low, 1 - blend), (high, blend)]:
        name = f'mixamorig:{side}HandPinky{joint + 1}'
        weights[name] = weights.get(name, 0) + influence * value
    assert abs(sum(weights.values()) - 1) < .00001
    for group in list(mesh.data.vertices[index].groups):
        mesh.vertex_groups[group.group].remove([int(index)])
    for name, value in weights.items():
        if value > 1e-8:
            mesh.vertex_groups[name].add([int(index)], value, 'REPLACE')
    changed.append(int(index))
assert len(changed) > 300
checks = []
depsgraph = bpy.context.evaluated_depsgraph_get()
for side in ('Left', 'Right'):
    for digit in ('Thumb', 'Index', 'Middle', 'Ring', 'Pinky'):
        for bone in rig.pose.bones:
            bone.rotation_mode = 'XYZ'; bone.rotation_euler = (0, 0, 0)
        bpy.context.view_layer.update()
        before = np.array([list(v.co) for v in mesh.evaluated_get(depsgraph).data.vertices])
        for i, angle in enumerate((35, 45, 30)):
            rig.pose.bones[f'mixamorig:{side}Hand{digit}{i + 1}'].rotation_euler.x = math.radians(angle)
        bpy.context.view_layer.update()
        after = np.array([list(v.co) for v in mesh.evaluated_get(depsgraph).data.vertices])
        motion = np.linalg.norm(after - before, axis=1)
        assert (motion > .002).sum() > 20 and motion.max() > .01, (side, digit)
        checks.append(dict(side=side, finger=digit, moved_vertices=int((motion > .002).sum()),
                           max_motion_m=float(motion.max())))
for bone in rig.pose.bones:
    bone.rotation_euler = (0, 0, 0)
bpy.context.view_layer.update()
assert np.max(np.abs(points - np.array([list(mesh_to_rig @ v.co) for v in mesh.data.vertices]))) == 0
bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True); mesh.select_set(True)
bpy.ops.export_scene.fbx(filepath=str(args.output / 'engineer-full-fingers.fbx'), use_selection=True,
                         object_types={'ARMATURE', 'MESH'}, add_leaf_bones=False, bake_anim=False,
                         use_armature_deform_only=False, axis_forward='-Z', axis_up='Y')
bpy.ops.wm.save_as_mainfile(filepath=str(args.output / 'engineer-full-fingers.blend'))
report = dict(bones=65, preserved_existing_bones=len(before_bones), mesh_vertices_preserved=True,
              added_pinky_chains=2, changed_pinky_vertices=len(changed), independent_curl_checks=checks)
(args.output / 'engineer-finger-rig-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
