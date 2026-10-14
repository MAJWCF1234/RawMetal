"""Repair a downloaded squad Mixamo rig's missing finger chains.

Run in Blender with -- --source <rigged FBX> --output <output directory>.
Preserves the user's body rig and mesh; fits five finger chains to the actual
authored hand geometry, mirrors the fitted rig and weights onto the new hand,
and verifies independent finger curls before exporting the rest rig.
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
parser.add_argument('--character', choices=('assault', 'medic', 'sharpshooter', 'rusher'), default='assault')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
args.output.mkdir(parents=True, exist_ok=True)
bpy.ops.object.select_all(action='SELECT')
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(args.source))
rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
mesh = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
if rig.animation_data:
    rig.animation_data.action = None
for bone in rig.pose.bones:
    bone.matrix_basis.identity()
# Mixamo treated the meter-sized OBJ coordinates as centimeters in its FBX.
# Normalize the rig object before re-export so animated preview and character
# framing agree on scale. The parented mesh follows without changing vertices.
rig.scale = (1, 1, 1)
bpy.context.view_layer.update()
mesh_to_rig = rig.matrix_world.inverted() @ mesh.matrix_world
points = np.array([list(mesh_to_rig @ v.co) for v in mesh.data.vertices])
original_points = points.copy()
root = Path(__file__).resolve().parents[1]
source_folder = root / ('source-assets/cleanup-repairs' if args.character == 'assault'
                        else 'src/assets/actors/cleanup')
source_points = np.array([list(map(float, line.split()[1:4])) for line in
                         (source_folder / (args.character + '.obj')).read_text().splitlines()
                         if line.startswith('v ')])
cal = np.frombuffer((source_folder / (args.character + '-arms.bin')).read_bytes(),
                   dtype='u1', offset=104).reshape(-1, 4)
assert points.shape == source_points.shape
assert np.max(np.abs(points - source_points)) < .000002
# Exported corner order is checked above. This avoids rounding ties in FBX
# unit conversion dropping otherwise identical authored hand corners.
hand_mask = cal[:, 2] > 128
assert hand_mask.sum() > 4000
old_groups = {group.index: group.name for group in mesh.vertex_groups}
old_weights = [{old_groups[g.group]: g.weight for g in vertex.groups}
               for vertex in mesh.data.vertices]
# The autorigger also assigned finger weights to the palm outside the source
# hand mask. Those must be repaired when replacing its bone positions.
hand_mask |= np.array([any('Hand' in name and not name.endswith('Hand') and value > 0
                          for name, value in weights.items()) for weights in old_weights])
# Fit to the original intact hand, represented on the positive side here.
hand = np.unique(np.round(points[hand_mask & (points[:, 0] < 0)] * [-1, 1, 1], 6), axis=0)
fingers = {}
centers = {'assault': (.129, .107, .085, .065), 'medic': (.108, .088, .066, .045),
           'sharpshooter': (.151, .129, .105, .081),
           'rusher': (.077, .059, .041, .023)}[args.character]
finger_base_y = {'assault': .715, 'medic': .715, 'sharpshooter': .675,
                 'rusher': .780}[args.character]
for name, center in zip(('Index', 'Middle', 'Ring', 'Pinky'), centers):
    distal = hand[(hand[:, 1] < finger_base_y - .010) &
                  (hand[:, 0] > {'assault': .25, 'medic': .25, 'sharpshooter': .22,
                                 'rusher': .21}[args.character]) &
                  (np.abs(hand[:, 2] - center) < .012)]
    assert len(distal) > 30, name
    # Distal geometry exposes four distinct fingers below the shared web.
    # Fit centerlines from the actual surface rings, rather than the broken
    # autorigger's single oversized index chain.
    xs = np.polyfit(distal[:, 1], distal[:, 0], 1)
    zs = np.polyfit(distal[:, 1], distal[:, 2], 1)
    base_y, tip_y = finger_base_y, float(distal[:, 1].min()) + .002
    ys = np.linspace(base_y, tip_y, 4)
    chain = np.array([[np.polyval(xs, y), y, np.polyval(zs, y)] for y in ys])
    fingers[name] = chain
fingers['Thumb'] = np.array([[.259, .777, .123], [.237, .757, .140],
                            [.222, .745, .152], [.208, .735, .153]] if args.character == 'assault'
                           else [[.270, .754, .102], [.253, .743, .115],
                                 [.239, .727, .124], [.223, .722, .125]])
if args.character == 'sharpshooter':
    fingers['Thumb'] = np.array([[.262, .737, .129], [.238, .717, .146],
                                 [.222, .704, .154], [.209, .697, .156]])
elif args.character == 'rusher':
    fingers['Thumb'] = np.array([[.224, .825, .057], [.207, .807, .072],
                                 [.192, .791, .079], [.181, .786, .082]])

bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
body_bones = {b.name: (b.head.copy(), b.tail.copy()) for b in rig.data.edit_bones
              if 'Hand' not in b.name or b.name.endswith('Hand')}
for bone in list(rig.data.edit_bones):
    if 'Hand' in bone.name and not bone.name.endswith('Hand'):
        rig.data.edit_bones.remove(bone)
for side, sign in [('Left', 1), ('Right', -1)]:
    wrist = rig.data.edit_bones[f'mixamorig:{side}Hand']
    wrist_center = {'assault': (.273, .735, .095), 'medic': (.280, .720, .071),
                    'sharpshooter': (.274, .695, .098),
                    'rusher': (.240, .795, .033)}[args.character]
    wrist.tail = Vector((wrist_center[0] * sign, wrist_center[1], wrist_center[2]))
    for name, fitted in fingers.items():
        chain = fitted * [sign, 1, 1]
        parent = wrist
        for i in range(4):
            bone = rig.data.edit_bones.new(f'mixamorig:{side}Hand{name}{i + 1}')
            bone.head = Vector(chain[min(i, 3)])
            bone.tail = Vector(chain[i + 1] if i < 3 else
                               chain[3] + (chain[3] - chain[2]) * .25)
            bone.parent = parent
            bone.use_connect = i > 0
            bone.use_deform = i < 3
            bone.align_roll(Vector((-sign, 0, 0)))
            parent = bone
bpy.ops.object.mode_set(mode='OBJECT')
assert len(rig.data.bones) == 65, len(rig.data.bones)
for name, (head, tail) in body_bones.items():
    assert (rig.data.bones[name].head_local - head).length < 1e-6
    if not name.endswith('Hand'):
        assert (rig.data.bones[name].tail_local - tail).length < 1e-6
for side in ('Left', 'Right'):
    for name in fingers:
        for i in range(3):
            key = f'mixamorig:{side}Hand{name}{i + 1}'
            if mesh.vertex_groups.get(key) is None:
                mesh.vertex_groups.new(name=key)

def distance_to_segment(p, a, b):
    t = float(np.clip(np.dot(p - a, b - a) / np.dot(b - a, b - a), 0, 1))
    return np.linalg.norm(p - (a + t * (b - a))), t

counts = {}
for index in np.flatnonzero(hand_mask):
    p = points[index].copy()
    side = 'Left' if p[0] > 0 else 'Right'
    p[0] = abs(p[0])
    choices = []
    for name, chain in fingers.items():
        for i in range(3):
            distance, t = distance_to_segment(p, chain[i], chain[i + 1])
            choices.append((distance, name, i + t))
    _, name, progress = min(choices)
    # Keep the palm and cuff on the hand. Smooth the knuckle transition;
    # distal fingers receive only their own chain, never another digit's.
    if name == 'Thumb':
        base_y, base_x = (.780, .262) if args.character == 'assault' else (.760, .270)
        if args.character == 'sharpshooter':
            base_y, base_x = .740, .265
        elif args.character == 'rusher':
            base_y, base_x = .829, .228
        influence = float(np.clip((base_y - p[1]) / .028, 0, 1))
        influence *= float(np.clip((base_x - p[0]) / .028, 0, 1))
    else:
        influence = float(np.clip((finger_base_y - p[1]) / .016, 0, 1))
    influence = influence * influence * (3 - 2 * influence)
    retained = {name: weight for name, weight in old_weights[index].items() if 'Hand' not in name}
    hand_total = 1 - sum(retained.values())
    weights = {f'mixamorig:{side}Hand': (1 - influence) * hand_total, **retained}
    # Blend adjacent bone centers around each joint, not the whole finger.
    location = float(np.clip(progress - .5, 0, 2))
    low = int(math.floor(location)); high = min(2, low + 1)
    blend = location - low
    for joint, amount in [(low, 1 - blend), (high, blend)]:
        key = f'mixamorig:{side}Hand{name}{joint + 1}'
        weights[key] = weights.get(key, 0) + influence * amount * hand_total
    for group in list(mesh.data.vertices[index].groups):
        mesh.vertex_groups[group.group].remove([int(index)])
    for key, weight in weights.items():
        if weight > 1e-8:
            mesh.vertex_groups[key].add([int(index)], weight, 'REPLACE')
            counts[key] = counts.get(key, 0) + 1
    assert abs(sum(weights.values()) - 1) < 1e-6
for side in ('Left', 'Right'):
    for name in fingers:
        for i in range(3):
            assert counts.get(f'mixamorig:{side}Hand{name}{i + 1}', 0) > 3
assert np.array_equal(points, original_points)

# Independent motion checks: bend one digit at a time and require its own
# bound surface to move. This catches a skeleton with unused finger bones.
checks = []
depsgraph = bpy.context.evaluated_depsgraph_get()
for side in ('Left', 'Right'):
    for name in fingers:
        for pb in rig.pose.bones:
            pb.rotation_mode = 'XYZ'; pb.rotation_euler = (0, 0, 0)
            pb.location = (0, 0, 0); pb.scale = (1, 1, 1)
        bpy.context.view_layer.update()
        before = np.array([list(v.co) for v in mesh.evaluated_get(depsgraph).data.vertices])
        for i, angle in enumerate((45, 55, 40)):
            rig.pose.bones[f'mixamorig:{side}Hand{name}{i + 1}'].rotation_euler.x = math.radians(angle)
        bpy.context.view_layer.update()
        after = np.array([list(v.co) for v in mesh.evaluated_get(depsgraph).data.vertices])
        motion = np.linalg.norm(after - before, axis=1)
        moved = int((motion > .002).sum()); maximum = float(motion.max())
        assert moved > 20 and maximum > .015, (side, name, moved, maximum)
        checks.append(dict(side=side, finger=name, moved_vertices=moved, max_motion_m=maximum))
for pb in rig.pose.bones:
    pb.rotation_euler = (0, 0, 0)
    pb.location = (0, 0, 0); pb.scale = (1, 1, 1)
bpy.context.view_layer.update()
bpy.ops.object.select_all(action='DESELECT')
rig.select_set(True); mesh.select_set(True)
target = args.output / (args.character + '-full-fingers.fbx')
bpy.ops.export_scene.fbx(filepath=str(target), use_selection=True, object_types={'ARMATURE', 'MESH'},
                         add_leaf_bones=False, bake_anim=False, use_armature_deform_only=False,
                         axis_forward='-Z', axis_up='Y')
bpy.ops.wm.save_as_mainfile(filepath=str(args.output / (args.character + '-full-fingers.blend')))
world_points = np.array([list(mesh.matrix_world @ v.co) for v in mesh.data.vertices])
report = dict(bones=len(rig.data.bones), geometry_preserved=True, body_rig_preserved=True,
              normalized_height_m=float(np.ptp(world_points[:, 2])),
              finger_weights=counts, independent_curl_checks=checks, exported=str(target))
(args.output / 'finger-rig-report.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
