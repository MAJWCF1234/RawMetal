"""Pull the CC0 Quaternius wolf's walk (Animal Pack Vol.2, Wolf.fbx) out as
sagittal swing angles per frame, for RigChimera.py to drive the Chimera's legs,
back and tail from a real four-legged gait instead of mirrored sines.

  blender -b --python tools/ExtractWolfGait.py -- Wolf.fbx tools/data/wolf_walk.json

Angles are radians from each bone's rest direction in the wolf's side plane
(wolf faces +X, Z up); positive swings the bone's tip forward.
"""
import bpy, sys, json, math
fbx, out = sys.argv[sys.argv.index('--') + 1], sys.argv[sys.argv.index('--') + 2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=fbx)
arm = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
walk = next(a for a in bpy.data.actions if a.name.endswith('Walking'))
arm.animation_data.action = walk
BONES = {'ThighFL': 'Bone.017', 'ShinFL': 'Bone.018', 'FootFL': 'Bone.019',
         'ThighFR': 'Bone.014', 'ShinFR': 'Bone.015', 'FootFR': 'Bone.016',
         'ThighBL': 'Bone.008', 'ShinBL': 'Bone.009', 'FootBL': 'Bone.010',
         'ThighBR': 'Bone.011', 'ShinBR': 'Bone.012', 'FootBR': 'Bone.013',
         'Back': 'Bone.001', 'Neck': 'Bone.002', 'Head': 'Bone.003', 'Tail': 'Bone.004', 'Root': 'Bone'}
def angle(v):  # direction in the side plane; tip forward is +
    return math.atan2(v.x, -v.z) if abs(v.z) > abs(v.x) * .2 or True else 0
first, last = map(int, walk.frame_range)
rest = {}
frames = []
sc = bpy.context.scene
for f in range(first, last):
    sc.frame_set(f)
    row = {}
    for key, name in BONES.items():
        pb = arm.pose.bones[name]
        d = (pb.tail - pb.head)
        r = arm.data.bones[name]
        rd = r.tail_local - r.head_local
        row[key] = math.atan2(d.x, -d.z) - math.atan2(rd.x, -rd.z)
        if key == 'Root':
            row['RootZ'] = pb.head.z - r.head_local.z
    # child angles relative to parent swing
    for leg in ('FL', 'FR', 'BL', 'BR'):
        row['Foot' + leg] -= row['Shin' + leg]; row['Shin' + leg] -= row['Thigh' + leg]
    frames.append({k: round(v, 4) for k, v in row.items()})
json.dump({'source': 'Quaternius Animal Pack Vol.2 Wolf.fbx (CC0 1.0)', 'fps': 24, 'frames': frames}, open(out, 'w'), indent=0)
for k in frames[0]:
    vals = [fr[k] for fr in frames]
    print('GAIT', k, 'min', round(min(vals), 3), 'max', round(max(vals), 3))
