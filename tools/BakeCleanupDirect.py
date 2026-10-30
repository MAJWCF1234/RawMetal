"""Bake the six squad roles' and the Investigator's required direct Mixamo motions.

Run in Blender --python-exit-code 1 --python this-file. Raw FBXs remain local.
Publishes matching mesh, evaluated skin vertices, semantic arm bindings and
source-clock metadata together. No anatomical retargeting or rigid fingers.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import struct
import sys

import bpy
from mathutils import Matrix, Vector

ROOT = Path(__file__).resolve().parents[1]
ASSETS = ROOT / 'src/assets/actors/cleanup'
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--roles', nargs='+', default=['assault', 'engineer', 'sharpshooter', 'heavy', 'medic', 'rusher'])
# Worktrees carry only the tracked manifests; the raw FBXs live in the main checkout.
parser.add_argument('--source-root', type=Path, default=ROOT)
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def engine(p):
    return Vector((p.x, p.z, -p.y))


def select_motion(actions):
    candidates = [a for a in actions if '|mixamo.com|Layer0' in a.name]
    assert candidates, [a.name for a in actions]
    return min(candidates, key=lambda a: a.name.count('|'))


def assign(rig, action):
    rig.animation_data_create()
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]


def investigator(library):
    # The Investigator wears a long coat that folds sharply in crouch and
    # prone, so every take is a standing one. IDs 0-5 keep the squad's core
    # slots; slot 5 (the squad's crouch) is a standing two-hand pistol ready.
    pickup = library / 'special/picking-up-object-picking-up-an-object-with-one-hand.fbx'
    shooting = library / 'special/shooting-pistol-shooting-a-pistol-from-behind-cover.fbx'
    specs = [
        dict(path=library / 'locomotion-pack/idle.fbx', loop=True),
        dict(path=library / 'male-locomotion-pack/walking.fbx', loop=True),
        dict(path=shooting, loop=False, center_seconds=.25),
        dict(path=library / 'male-injured-pack/injured hurting idle.fbx', loop=False, seconds=(0, .5)),
        dict(path=library / 'special/dying-backwards-shot-to-the-chest-falling-backwards.fbx', loop=False),
        dict(path=library / 'pistol-handgun-locomotion-pack/pistol idle.fbx', loop=True),
        # 6 radio: the hand held up at the mask, as to a handset.
        dict(path=pickup, loop=True, pingpong=True, seconds=(1.7, 2.2)),
        # 7 reading: head bowed over what the right hand holds at the chest.
        dict(path=library / 'special/picking-up-picking-up-an-object.fbx', loop=True, pingpong=True, seconds=(5.0, 6.1)),
        # 8 pointing: the arm raised straight ahead; hold phase 1 to keep it there.
        dict(path=pickup, loop=False, seconds=(0, .55)),
        # 9 directing: a flick of the hand and a sweep of the arm, rest to rest.
        dict(path=library / 'gestures-pack-basic/dismissing gesture.fbx', loop=False),
    ]
    for spec in specs:
        assert spec['path'].is_file(), spec['path']
    return specs


def selected(role, library):
    if role == 'investigator':
        return investigator(library)
    pistol = role in ('engineer', 'medic')
    def file(name):
        p = library / name
        assert p.is_file(), p
        return p
    def special(prefix):
        found = sorted((library / 'special').glob(prefix + '*.fbx'))
        assert len(found) == 1, (role, prefix, found)
        return found[0]
    # These IDs are the existing runtime's semantic slots.
    paths = [
        file('pistol-handgun-locomotion-pack/pistol idle.fbx' if pistol else 'pro-rifle-pack/idle aiming.fbx'),
        file('pistol-handgun-locomotion-pack/pistol walk.fbx' if pistol else 'pro-rifle-pack/walk forward.fbx'),
        special('shooting-pistol') if pistol else file('basic-shooter-pack/firing rifle.fbx'),
        file('male-injured-pack/injured hurting idle.fbx' if pistol else 'basic-shooter-pack/hit reaction.fbx'),
        special('dying-backwards') if pistol else file('pro-rifle-pack/death from front headshot.fbx'),
        file('pistol-handgun-locomotion-pack/pistol kneeling idle.fbx' if pistol else 'pro-rifle-pack/idle crouching aiming.fbx'),
    ]
    specs = [dict(path=p, loop=i in (0, 1, 5)) for i, p in enumerate(paths)]
    if pistol:
        # Keep the short shot around the raised weapon; the full cover-peek
        # sequence is not a 220 ms gameplay firing action.
        specs[2]['center_seconds'] = .25
        specs[3]['seconds'] = (0, .5)
    if role == 'medic':
        specs.append(dict(path=special('opening-a-lid'), loop=False))
    if role == 'engineer':
        texting = special('texting-while-standing')
        specs += [dict(path=texting, loop=False, seconds=(0, 2.5)),
                  dict(path=texting, loop=True, seconds=(10, 12)),
                  dict(path=texting, loop=False, tail_seconds=2.5)]
    return specs


def bake(role):
    library = args.source_root / 'source-assets/mixamo-motion' / (role + '-direct')
    manifest = json.loads((library / 'manifest.json').read_text())
    file_hashes = {f['file']: f['sha256'] for g in manifest['packs'] for f in g['files']}
    specs = selected(role, library)
    # Match each pack's own character bind pose. All poses are transferred as
    # evaluated bone matrices, rather than local rotation F-curves.
    pack = 'pistol-handgun-locomotion-pack' if role in ('medic', 'engineer') else 'pro-rifle-pack'
    base = library / pack / ('Investigator.fbx' if role == 'investigator' else manifest['character'] + '.fbx')
    if not base.exists():
        bases = list(library.glob('*/' + manifest['character'] + '.fbx'))
        assert len(bases) == 1, bases
        base = bases[0]
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(base))
    rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
    meshes = [o for o in bpy.context.scene.objects if o.type == 'MESH']
    rig.animation_data_clear()
    for bone in rig.pose.bones:
        bone.matrix_basis = Matrix.Identity(4)
    rig.data.pose_position = 'REST'
    bpy.context.view_layer.update()
    graph = bpy.context.evaluated_depsgraph_get()
    raw = [m.matrix_world @ v.co for m in meshes for v in m.evaluated_get(graph).data.vertices]
    low, high = min(p.z for p in raw), max(p.z for p in raw)
    scale = 1.6 / (high - low)
    hips = rig.matrix_world @ rig.data.bones['mixamorig:Hips'].head_local
    origin = Vector((hips.x, hips.y, low))
    def normalized(p):
        return engine(p - origin) * scale
    points = [normalized(p) for p in raw]
    # FBX uploads contain a triangle soup and omit the original material slots.
    # Recover authored texture ownership from unchanged UV triangles; rig
    # head/neck weights only resolve genuinely new triangles (such as hands).
    authored = ASSETS / (role + '.obj')
    backup = ROOT / '.build/direct-motion-before' / (role + '.obj')
    if backup.exists():
        authored = backup
    authored_uv, material_uv, material = [], {}, 'cleanup_body'
    # A new body has no earlier OBJ: its head material comes from the weights.
    for line in (authored.read_text().splitlines() if authored.exists() else []):
        if line.startswith('vt '):
            authored_uv.append(tuple(round(float(v), 5) for v in line.split()[1:3]))
        elif line.startswith('usemtl '):
            material = line.split()[1]
        elif line.startswith('f '):
            key = tuple(sorted(authored_uv[int(v.split('/')[1])-1] for v in line.split()[1:]))
            material_uv.setdefault(key, set()).add(material)
    corner_indices, corners, lines = [], [], [f'# Direct Mixamo {role}; evaluated source skin; Y-up, +Z forward', 'o Cleanup']
    groups = []
    offset = 0
    for m in meshes:
        m.data.calc_loop_triangles()
        uv = m.data.uv_layers.active.data
        groups += [{m.vertex_groups[g.group].name: g.weight for g in v.groups} for v in m.data.vertices]
        for tri in m.data.loop_triangles:
            key = tuple(sorted(tuple(round(v, 5) for v in uv[li].uv) for li in tri.loops))
            matches = material_uv.get(key, set())
            if len(matches) == 1:
                material = next(iter(matches))
            else:
                head = sum(sum(groups[offset + vi].get(n, 0) for n in ('mixamorig:Head', 'mixamorig:Neck')) for vi in tri.vertices) / 3
                material = 'cleanup_head' if head > .5 else 'cleanup_body'
            idx = len(corner_indices) + 1
            for vi, li in zip(tri.vertices, tri.loops):
                index = offset + vi
                corner_indices.append(index)
                corners.append(points[index])
                lines += ['v %.7f %.7f %.7f' % tuple(points[index]), 'vt %.7f %.7f' % tuple(uv[li].uv)]
            lines += ['usemtl ' + material, f'f {idx}/{idx} {idx+1}/{idx+1} {idx+2}/{idx+2}']
        offset += len(m.data.vertices)
    assert offset == len(points) == len(groups) and len(points) < 65536
    # Weld coincident corners only when every skin weight agrees. UVs remain
    # per-corner in OBJ; the animation bank need not store the same skin point
    # three times. Never weld coincident independently weighted fingers.
    unique, representatives, remap = {}, [], []
    for i, (p, g) in enumerate(zip(points, groups)):
        key = (tuple(round(v, 6) for v in p), tuple(sorted((name, round(w, 6)) for name, w in g.items() if w > 0)))
        if key not in unique:
            unique[key] = len(representatives)
            representatives.append(i)
        remap.append(unique[key])
    points = [points[i] for i in representatives]
    groups = [groups[i] for i in representatives]
    raw_unique = [raw[i] for i in representatives]
    corner_indices = [remap[i] for i in corner_indices]
    joints, palms = [], []
    for side in ('Right', 'Left'):
        for name in ('Arm', 'ForeArm', 'Hand'):
            joints += list(normalized(rig.matrix_world @ rig.data.bones[f'mixamorig:{side}{name}'].head_local))
        hand = rig.data.bones[f'mixamorig:{side}Hand']
        direction = engine(rig.matrix_world.to_3x3() @ (hand.tail_local - hand.head_local)).normalized()
        palms += list(direction)
    weights = []
    finger_vertices = [[], []]
    for index, g in enumerate(groups):
        values = []
        for side in ('Right', 'Left'):
            hand = sum(w for name, w in g.items() if name.startswith(f'mixamorig:{side}Hand'))
            forearm = g.get(f'mixamorig:{side}ForeArm', 0)
            upper = g.get(f'mixamorig:{side}Arm', 0)
            values.append((upper + forearm + hand, forearm + hand, hand))
        side = int(values[1][0] > values[0][0])
        influence, lower, palm = values[side]
        denom = max(influence, 1e-9)
        weights.append((round(min(1, influence) * 255), round(min(1, lower / denom) * 255), round(min(1, palm / denom) * 255), side))
        finger = sum(w for name, w in g.items() if name.startswith(f'mixamorig:{("Right", "Left")[side]}Hand') and any(d in name for d in ('Thumb', 'Index', 'Middle', 'Ring', 'Pinky')))
        if finger > .5:
            finger_vertices[side].append(index)
    calibration = b'RMC2' + struct.pack('<24fI', *(joints + palms), len(corner_indices))
    calibration += bytes(v for index in corner_indices for v in weights[index])
    bind_hand = [rig.matrix_world @ rig.data.bones[f'mixamorig:{side}Hand'].matrix_local for side in ('Right', 'Left')]
    bind_local = [[bind_hand[side].inverted() @ raw_unique[i] for i in indices] for side, indices in enumerate(finger_vertices)]
    rig.data.pose_position = 'POSE'
    all_frames, all_layers, evidence = [], [], []
    for clip, spec in enumerate(specs):
        path = spec['path']
        relative_path = path.relative_to(library).as_posix()
        assert sha(path) == file_hashes[relative_path], path
        existing_objects, existing_actions = set(bpy.data.objects), set(bpy.data.actions)
        bpy.ops.import_scene.fbx(filepath=str(path))
        imported = set(bpy.data.objects) - existing_objects
        source = next(o for o in imported if o.type == 'ARMATURE')
        action = select_motion(set(bpy.data.actions) - existing_actions)
        assign(source, action)
        fps = bpy.context.scene.render.fps / bpy.context.scene.render.fps_base
        first, last = action.frame_range
        if 'seconds' in spec:
            last = first + spec['seconds'][1] * fps
            first += spec['seconds'][0] * fps
        if 'tail_seconds' in spec:
            first = last - spec['tail_seconds'] * fps
        if 'center_seconds' in spec:
            center = (first + last) * .5
            half = spec['center_seconds'] * fps * .5
            first, last = center - half, center + half
        duration = (last - first) / fps
        # Per-role density is decided after importing all selected timelines.
        evidence.append(dict(clip=clip, file=path.relative_to(args.source_root).as_posix(), sha256=sha(path), source_fps=fps,
                             source_first_frame=first, source_last_frame=last, duration_seconds=duration, loop=spec['loop']))
        samples = max(2, math.ceil(duration * 24 - 1e-6) + 1)
        # First inspect clocks, then evaluate every published sample at the
        # common role stride. Do not reinterpret legacy pack clocks.
        spec.update(first=first, last=last, fps=fps, duration=duration, samples=samples)
        for obj in imported:
            bpy.data.objects.remove(obj, do_unlink=True)
        for a in set(bpy.data.actions) - existing_actions:
            if a.users == 0:
                bpy.data.actions.remove(a)
    max_samples = max(s['samples'] for s in specs)
    assert max_samples <= 512
    for clip, spec in enumerate(specs):
        samples = spec['samples']
        existing_objects, existing_actions = set(bpy.data.objects), set(bpy.data.actions)
        bpy.ops.import_scene.fbx(filepath=str(spec['path']))
        imported = set(bpy.data.objects) - existing_objects
        source = next(o for o in imported if o.type == 'ARMATURE')
        action = select_motion(set(bpy.data.actions) - existing_actions)
        assign(source, action)
        frames, layers = [], []
        max_error, finger_motion, finger_temporal = 0., 0., 0.
        first_fingers = None
        for sample in range(samples):
            phase = sample / (samples - 1)
            frame = spec['first'] + phase * (spec['last'] - spec['first'])
            bpy.context.scene.frame_set(int(frame), subframe=frame - int(frame))
            bpy.context.view_layer.update()
            relative = rig.matrix_world.inverted() @ source.matrix_world
            desired = {b.name: relative @ b.matrix for b in source.pose.bones}
            for b in rig.pose.bones:
                kwargs = dict(parent_matrix=desired[b.parent.name], parent_matrix_local=b.parent.bone.matrix_local) if b.parent else {}
                b.matrix_basis = b.bone.convert_local_to_pose(desired[b.name], b.bone.matrix_local, invert=True, **kwargs)
            bpy.context.view_layer.update()
            error = max(abs(v) for b in rig.pose.bones for row in (b.matrix - desired[b.name]) for v in row)
            max_error = max(max_error, error)
            assert error < .001, (role, clip, sample, error)
            graph = bpy.context.evaluated_depsgraph_get()
            raw_world = [m.matrix_world @ v.co for m in meshes for v in m.evaluated_get(graph).data.vertices]
            world = [raw_world[i] for i in representatives]
            root = normalized((rig.matrix_world @ rig.pose.bones['mixamorig:Hips'].matrix).to_translation())
            drift = Vector((root.x, 0, root.z))
            posed = [normalized(p) - drift for p in world]
            ground = min(p.y for p in posed)
            ground_offset = Vector((0, ground, 0))
            posed = [p - ground_offset for p in posed]
            def joint(name):
                return normalized((rig.matrix_world @ rig.pose.bones['mixamorig:' + name].matrix).to_translation()) - drift - ground_offset
            layer = [joint('Hips'), joint('Spine2')]
            current_fingers = []
            for side, name in enumerate(('Right', 'Left')):
                hand_bone = rig.pose.bones[f'mixamorig:{name}Hand']
                direction = engine(rig.matrix_world.to_3x3() @ (hand_bone.tail - hand_bone.head)).normalized()
                layer += [joint(name + 'Arm'), joint(name + 'ForeArm'), joint(name + 'Hand'), direction]
                hand_frame = rig.matrix_world @ hand_bone.matrix
                inverse_hand = hand_frame.inverted()
                hand_points = [inverse_hand @ world[i] for i in finger_vertices[side]]
                current_fingers += hand_points
                finger_motion = max(finger_motion, max(((hand_frame.to_3x3() @ (a - b)).length * scale for a, b in zip(hand_points, bind_local[side])), default=0))
                hand_scale = hand_frame.to_3x3().col[0].length * scale
            if first_fingers is None:
                first_fingers = [p.copy() for p in current_fingers]
            finger_temporal = max(finger_temporal, max(((a - b).length * hand_scale for a, b in zip(current_fingers, first_fingers)), default=0))
            frames.append(posed)
            layers.append(layer)
        if spec.get('pingpong'):
            # A held pose cut from a longer take does not return to its start:
            # play it out and back, so the loop seam is the same sample.
            frames += frames[-2::-1]
            layers += layers[-2::-1]
            samples = len(frames)
            spec.update(samples=samples, duration=spec['duration'] * 2)
            evidence[clip].update(duration_seconds=spec['duration'], pingpong=True)
        closure = 0.
        if spec['loop']:
            closure = max((a - b).length for a, b in zip(frames[0], frames[-1]))
            assert closure < .15, (role, clip, 'loop closure', closure)
            # Blend the final 1/8 of the loop into its actual first pose; avoid
            # a one-frame wrap pop while retaining the source's forward order.
            start = int((samples - 1) * .875)
            for i in range(start, samples):
                t = (i - start) / max(1, samples - 1 - start)
                t = t*t*(3-2*t)
                frames[i] = [a.lerp(b, t) for a, b in zip(frames[i], frames[0])]
                layers[i] = [a.lerp(b, t) for a, b in zip(layers[i], layers[0])]
                for side in range(2):
                    layers[i][5 + side * 4].normalize()
        all_frames += frames
        all_layers += layers
        quantization = max((p - Vector(tuple(round(v * 1000) / 1000 for v in p))).length for f in frames for p in f)
        hand_motion = max((f[i] - frames[0][i]).length for f in frames for ids in finger_vertices for i in ids)
        assert samples <= 512, (role, clip, samples)
        evidence[clip].update(samples=samples, max_quantization_error_m=quantization, max_hand_motion_m=hand_motion,
                              max_finger_motion_m=finger_motion, max_finger_temporal_motion_m=finger_temporal,
                              max_pose_transfer_error=max_error, max_loop_closure_m=closure)
        print('DIRECT_CLIP', role, clip, spec['path'].name, 'seconds', round(spec['duration'], 4), 'samples', samples, flush=True)
        for obj in imported:
            bpy.data.objects.remove(obj, do_unlink=True)
        for a in set(bpy.data.actions) - existing_actions:
            if a.users == 0:
                bpy.data.actions.remove(a)
    total_samples = sum(s['samples'] for s in specs)
    animation = bytearray(b'RMA3' + struct.pack('<4I', len(corner_indices), total_samples, len(specs), len(points)))
    animation += struct.pack('<%dH' % len(corner_indices), *corner_indices)
    animation += struct.pack('<%dI' % len(specs), *(s['samples'] for s in specs))
    for frame in all_frames:
        for p in frame:
            assert all(math.isfinite(v) and abs(v) < 32 for v in p), (role, p)
            animation += struct.pack('<3h', *(round(v * 1000) for v in p))
    animation += b'RML1' + struct.pack('<4I', 2, total_samples, len(specs), 120)
    for layer in all_layers:
        animation += struct.pack('<30f', *(v for p in layer for v in p))
    animation += b'RMD1' + struct.pack('<I', len(specs)) + struct.pack('<%df' % len(specs), *(s['duration'] for s in specs))
    outputs = {role + '.obj': ('\n'.join(lines) + '\n').encode(), role + '-animation.bin': animation,
               role + '-arms.bin': calibration, role + '-skin.bin': calibration}
    if role == 'medic':
        outputs['medic-treatment-anchors.bin'] = b''.join(struct.pack('<6f', *(list(layer[4]) + list(layer[8]))) for layer in all_layers)
    result = dict(role=role, vertices=len(points), corners=len(corner_indices), samples=max(s['samples'] for s in specs),
                  total_samples=total_samples, clips=evidence,
                  base_file=base.relative_to(args.source_root).as_posix(), base_sha256=sha(base),
                  output_sha256={name: hashlib.sha256(content).hexdigest() for name, content in outputs.items()})
    return result, outputs


report_path = ASSETS / 'direct-motion.json'
report = json.loads(report_path.read_text()) if report_path.exists() else dict(version=1, method='evaluated-skin', roles=[])
pending = {}
for role in args.roles:
    result, outputs = bake(role)
    pending.update(outputs)
    report['roles'] = [r for r in report['roles'] if r['role'] != role] + [result]
pending['direct-motion.json'] = (json.dumps(report, indent=2) + '\n').encode()
# Finish all selected roles before publishing any of their resources. A
# missing source or failed pose leaves the previously complete bank intact.
for name, content in pending.items():
    (ASSETS / (name + '.tmp')).write_bytes(content)
for name in pending:
    (ASSETS / (name + '.tmp')).replace(ASSETS / name)
print('DIRECT_BAKE_COMPLETE', len(args.roles), 'roles', flush=True)
