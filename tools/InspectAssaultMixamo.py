"""Audit downloaded Assault clips and catalogue their actual durations.

Run in Blender with -- --library <assault-direct directory>. The manifest
is generated from verified browser downloads. This checks every FBX's full
finger skeleton, animation timing and finite sampled bone transforms.
"""
import argparse
import json
import math
from pathlib import Path
import sys
import bpy

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--library', type=Path, required=True)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:])
manifest = json.loads((args.library / 'manifest.json').read_text())
results = []
required = [f'mixamorig:{side}Hand{finger}{joint}' for side in ('Left', 'Right')
            for finger in ('Thumb', 'Index', 'Middle', 'Ring', 'Pinky') for joint in (1, 2, 3)]
for group in manifest['packs']:
    for entry in group['files']:
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.context.scene.render.fps = 24
        bpy.ops.import_scene.fbx(filepath=str(args.library / entry['file']))
        rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
        assert len(rig.data.bones) == 65, (entry['file'], len(rig.data.bones))
        assert all(name in rig.data.bones for name in required), entry['file']
        actions = list(bpy.data.actions)
        character_file = 'assault-full-fingers' in entry['file']
        item = dict(file=entry['file'], group=group['name'], bones=len(rig.data.bones),
                    kind='character' if character_file else 'animation',
                    fps=bpy.context.scene.render.fps, actions=[])
        if actions and not character_file:
            # The browser requests 24 fps for both flows. Mixamo's MotionPack
            # archives can retain 30 fps for legacy takes; individual exports
            # honour 24 fps. Preserve each file's real clock for duration.
            allowed = (24,) if group['kind'] == 'clip' else (24, 30)
            assert item['fps'] in allowed, (entry['file'], item['fps'], allowed)
        for action in actions:
            rig.animation_data_create()
            rig.animation_data.action = action
            if getattr(action, 'slots', None):
                rig.animation_data.action_slot = action.slots[0]
            start, end = action.frame_range
            assert math.isfinite(start) and math.isfinite(end) and end >= start
            if not character_file:
                assert end > start, entry['file']
            angles = []
            for phase in (0, .5, 1):
                frame = start + phase * (end - start)
                bpy.context.scene.frame_set(int(frame), subframe=frame - int(frame))
                for bone in rig.pose.bones:
                    assert all(math.isfinite(v) for row in bone.matrix for v in row), (entry['file'], bone.name)
                angles.append(max(float(rig.pose.bones[name].matrix_basis.to_quaternion().angle)
                                  for name in required))
            item['actions'].append(dict(name=action.name, first_frame=float(start), last_frame=float(end),
                                        duration_seconds=float((end - start) / item['fps']),
                                        max_finger_rotation_degrees=math.degrees(max(angles))))
        if not actions:
            assert 'assault-full-fingers' in entry['file'], entry['file']
        results.append(item)
        print('AUDITED', entry['file'], len(actions), flush=True)
report = dict(character=manifest['character'], requested_export_fps=24, target_bake_fps=24,
              source_timeline_counts={str(fps): sum(item['fps'] == fps for item in results)
                                      for fps in sorted({item['fps'] for item in results})},
              individual_source_fps=24, files=len(results),
              animated_files=sum(item['kind'] == 'animation' and bool(item['actions']) for item in results),
              full_finger_skeleton_verified=True, finite_sampled_poses=True, clips=results)
(args.library / 'clip-catalog.json').write_text(json.dumps(report, indent=2))
print('PASS', report['files'], 'FBX files;', report['animated_files'],
      'animated files; 65 bones; source timing preserved; target bake 24 fps')
