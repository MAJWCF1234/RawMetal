"""Verify cleanup bakes against their source geometry without running Blender.

Requires NumPy. Optional --baseline checks a saved Name/Sha256 manifest before
validating every clip, loop, protected hand edge and calibrated joint frame.
Direct Mixamo bakes also require the source certificate in direct-motion.json;
their fingers may bend, so checks compare against evaluated source poses rather
than requiring every finger edge to equal the immutable T-pose.
"""
from pathlib import Path
import struct, json, hashlib, argparse
import numpy as np

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline',type=Path)
parser.add_argument('--output',type=Path)
parser.add_argument('--source-report',type=Path)
parser.add_argument('--assets',type=Path,help='Cleanup runtime asset directory; defaults to src/assets/actors/cleanup')
parser.add_argument('--roles',nargs='+',choices=('assault','engineer','sharpshooter','heavy','medic','rusher'),
                    default=('assault','engineer','sharpshooter','heavy','medic','rusher'))
args=parser.parse_args()
root = Path(__file__).resolve().parents[1]
assets = args.assets or root / 'src/assets/actors/cleanup'
source_report_path = args.source_report or assets / 'direct-motion.json'
source_roles = None


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def finite_metric(clip, name, maximum):
    value = float(clip[name])
    assert np.isfinite(value) and 0 <= value <= maximum, (name, value)
    return value


def padded_clips(frames, clip_samples):
    """Pad only for shared spatial checks; clocks always use actual counts."""
    samples = max(clip_samples)
    padded = np.empty((len(clip_samples), samples, *frames.shape[1:]), dtype=frames.dtype)
    offset = 0
    for clip, actual in enumerate(clip_samples):
        padded[clip, :actual] = frames[offset:offset + actual]
        padded[clip, actual:] = frames[offset + actual - 1]
        offset += actual
    assert offset == len(frames)
    return padded


def verify_direct(role, data, layer_end, samples, clips, count, corners,
                  points, vertices, poses, weights, skin_data, mesh_stretch,
                  clip_samples, total_samples):
    global source_roles
    assert 2 <= samples <= 512 and clips == (9 if role == 'engineer' else 7 if role == 'medic' else 6)
    assert data[layer_end:layer_end + 4] == b'RMD1'
    assert struct.unpack_from('<I', data, layer_end + 4)[0] == clips
    assert len(data) == layer_end + 8 + clips * 4
    durations = np.frombuffer(data, dtype='<f4', count=clips, offset=layer_end + 8)
    assert np.isfinite(durations).all() and (durations > 0).all() and (durations <= 60).all()
    # Density follows actual source seconds. A fixed frame count is not a
    # license to reinterpret a 30-fps source take as 24 fps.
    assert ((np.array(clip_samples) - 1) / durations >= 23.9).all(), (role, 'sample density', durations)
    if source_roles is None:
        evidence = json.loads(source_report_path.read_text(encoding='utf-8-sig'))
        assert evidence['version'] == 1 and evidence['method'] == 'evaluated-skin'
        source_roles = {item['role']: item for item in evidence['roles']}
        assert len(source_roles) == len(evidence['roles'])
    evidence = source_roles[role]
    assert (evidence['vertices'], evidence['corners'], evidence['samples']) == (count, corners, samples)
    variable_samples = data[:4] == b'RMA3'
    if variable_samples:
        assert evidence['total_samples'] == total_samples, (role, 'total sample count')
    base_file = (root / evidence['base_file']).resolve()
    assert base_file.is_relative_to((root / 'source-assets').resolve()) and base_file.is_file()
    assert sha256(base_file) == evidence['base_sha256'].lower(), (role, 'source rig SHA-256')
    clip_reports = evidence['clips']
    assert len(clip_reports) == clips and [item['clip'] for item in clip_reports] == list(range(clips))
    required = {f'{role}.obj', f'{role}-animation.bin', f'{role}-arms.bin', f'{role}-skin.bin'}
    if role == 'medic':
        required.add('medic-treatment-anchors.bin')
    output_hashes = evidence['output_sha256']
    assert required.issubset(output_hashes), (role, 'missing output certificate')
    for name, digest in output_hashes.items():
        path = (assets / name).resolve()
        assert path.is_relative_to(assets.resolve()) and path.is_file()
        assert sha256(path) == digest.lower(), (role, 'stale output certificate', name)

    quantization, finger_motion, temporal_finger_motion, hand_motion, closure = [], [], [], [], []
    source_durations = []
    for clip in clip_reports:
        source = (root / clip['file']).resolve()
        assert source.is_relative_to((root / 'source-assets').resolve()) and source.is_file()
        assert sha256(source) == clip['sha256'].lower(), (role, clip['clip'], 'source SHA-256')
        fps = float(clip['source_fps'])
        first, last = float(clip['source_first_frame']), float(clip['source_last_frame'])
        seconds = float(clip['duration_seconds'])
        assert np.isfinite([fps, first, last, seconds]).all() and fps in (24, 30) and last > first
        assert abs(seconds - (last - first) / fps) < 1e-5, (role, clip['clip'], 'source clock')
        assert abs(float(durations[clip['clip']]) - seconds) < 1e-5
        if variable_samples:
            actual_samples = clip_samples[clip['clip']]
            assert clip['samples'] == actual_samples, (role, clip['clip'], 'per-clip sample count')
            assert actual_samples == max(2, int(np.ceil(seconds * 24 - 1e-6)) + 1), (role, clip['clip'], '24-fps sampling')
        source_durations.append(seconds)
        quantization.append(finite_metric(clip, 'max_quantization_error_m', np.sqrt(3) * .0005 + 1e-7))
        finger_motion.append(finite_metric(clip, 'max_finger_motion_m', 2))
        temporal_finger_motion.append(finite_metric(clip, 'max_finger_temporal_motion_m', 2))
        hand_motion.append(finite_metric(clip, 'max_hand_motion_m', 4))
        closure.append(finite_metric(clip, 'max_loop_closure_m', .15))
        finite_metric(clip, 'max_pose_transfer_error', .001)
    # A held aiming pose may deliberately keep its fingers still. Require
    # a changed source finger shape somewhere in the selected bank, measured
    # in the moving hand frame so whole-arm motion cannot satisfy this
    # condition. Temporal deformation is reported separately: a constant
    # authored grip must not be described as an animated finger curl.
    assert max(finger_motion) >= .002, (role, 'no source-relative finger deformation')
    assert max(hand_motion) >= .005, (role, 'no source hand motion')

    bind_joints = np.array(struct.unpack_from('<18f', (assets / f'{role}-arms.bin').read_bytes(), 4)).reshape(2, 3, 3)
    assert np.isfinite(bind_joints).all()
    source_limb_error = 0.0
    for side in (0, 1):
        for segment in (0, 1):
            a = 2 + side * 4 + segment
            lengths = np.linalg.norm(poses[:, :, a + 1] - poses[:, :, a], axis=-1)
            source_length = np.linalg.norm(bind_joints[side, segment + 1] - bind_joints[side, segment])
            assert .005 < source_length < 2
            source_limb_error = max(source_limb_error, float(np.abs(lengths - source_length).max()))
    assert source_limb_error <= .001, (role, 'source limb length', source_limb_error)
    assert np.max(np.abs(np.linalg.norm(poses[:, :, [5, 9]], axis=-1) - 1)) < .00001
    assert (weights[:, 3] <= 1).all()
    assert np.array_equal(np.frombuffer(skin_data, dtype=np.uint8, offset=104).reshape(corners, 4), weights)
    for side in (0, 1):
        assert np.count_nonzero((weights[:, 3] == side) & (weights[:, 0] > 127) & (weights[:, 2] > 127)) > 20
    assert vertices[:, :, :, 1].min() >= -.003 and np.abs(vertices).max() < 4
    loops = [item['clip'] for item in clip_reports if item['loop']]
    assert {0, 1, 5}.issubset(loops)
    loop = max(np.linalg.norm(vertices[c, 0] - vertices[c, -1], axis=-1).max() for c in loops)
    drift = max(np.linalg.norm(poses[c, :, 0][:, [0, 2]], axis=-1).max() for c in loops)
    assert loop <= .006 and drift <= .004, (role, 'loop/root seam', loop, drift)
    assert mesh_stretch < 8, (role, 'whole mesh stretch', mesh_stretch)
    if role == 'medic':
        anchors_flat = np.frombuffer((assets / 'medic-treatment-anchors.bin').read_bytes(), dtype='<f4').reshape(total_samples, 2, 3)
        anchors = padded_clips(anchors_flat, clip_samples)
        assert np.array_equal(anchors, poses[:, :, [4, 8]])
    return {
        'role': role, 'method': 'evaluated-skin', 'format': data[:4].decode('ascii'),
        'bytes': len(data), 'samples': samples, 'clip_samples': list(clip_samples), 'total_samples': total_samples,
        'clips': clips, 'source_durations_seconds': source_durations,
        'loop_error_mm': float(loop * 1000), 'planar_pelvis_drift_mm': float(drift * 1000),
        'source_quantization_max_mm': max(quantization) * 1000,
        'source_relative_finger_motion_max_mm': max(finger_motion) * 1000,
        'source_relative_finger_temporal_motion_max_mm': max(temporal_finger_motion) * 1000,
        'source_loop_closure_max_mm': max(closure) * 1000,
        'source_limb_length_error_mm': source_limb_error * 1000,
        'whole_mesh_edge_stretch': mesh_stretch,
        'crouch_height_min_m': float(vertices[5, :, :, 1].max(axis=1).min()),
        'crouch_height_max_m': float(vertices[5, :, :, 1].max(axis=1).max()),
        'death_end_height_m': float(vertices[4, -1, :, 1].max()),
        'source_and_output_hashes_match': True, 'finite_metadata': True,
    }


manifest = json.loads(args.baseline.read_text(encoding='utf-8-sig')) if args.baseline else []
for item in manifest:
    assert hashlib.sha256((assets / item['Name']).read_bytes()).hexdigest().upper() == item['Sha256'], item['Name']
results = []
for role in args.roles:
    data = (assets / (role + '-animation.bin')).read_bytes()
    corners, header_samples, clips, count = struct.unpack_from('<4I', data, 4)
    assert data[:4] in (b'RMA2', b'RMA3') and 6 <= clips <= 9 and corners > 0 and count > 0
    offset = 20 + corners * 2
    variable_samples = data[:4] == b'RMA3'
    if variable_samples:
        clip_samples = list(struct.unpack_from(f'<{clips}I', data, offset))
        assert all(2 <= actual <= 512 for actual in clip_samples)
        samples, total_samples = max(clip_samples), sum(clip_samples)
        assert total_samples == header_samples
        offset += clips * 4
        layer_header = (2, total_samples, clips, 120)
    else:
        samples = header_samples
        assert 2 <= samples <= 512
        clip_samples, total_samples = [samples] * clips, samples * clips
        layer_header = (1, samples, clips, 120)
    end = offset + total_samples * count * 6
    assert data[end:end + 4] == b'RML1' and struct.unpack_from('<4I', data, end + 4) == layer_header
    layer_end = end + 20 + total_samples * 120
    direct = data[layer_end:layer_end + 4] == b'RMD1'
    if not direct:
        assert not variable_samples
        assert 2 <= samples <= 128 and clips == (7 if role == 'medic' else 6)
        assert len(data) == layer_end
    indices = np.frombuffer(data, dtype='<u2', count=corners, offset=20)
    assert indices.max() < count
    vertex_frames = np.frombuffer(data, dtype='<i2', count=total_samples * count * 3, offset=offset).reshape(total_samples, count, 3) * .001
    pose_frames = np.frombuffer(data, dtype='<f4', count=total_samples * 30, offset=end + 20).reshape(total_samples, 10, 3)
    vertices = padded_clips(vertex_frames, clip_samples)
    poses = padded_clips(pose_frames, clip_samples)
    assert np.isfinite(poses).all() and np.abs(poses).max() < 64
    for side in (0, 1):
        lengths = np.linalg.norm(poses[:, :, 3 + side * 4] - poses[:, :, 2 + side * 4], axis=-1)
        lower = np.linalg.norm(poses[:, :, 4 + side * 4] - poses[:, :, 3 + side * 4], axis=-1)
        assert lengths.min() > .001 and lengths.max() < 4 and lower.min() > .001 and lower.max() < 4
        bind_joints = struct.unpack_from('<18f', (assets / (role + '-arms.bin')).read_bytes(), 4)
        expected_upper = np.linalg.norm(np.array(bind_joints[side * 9 + 3:side * 9 + 6]) - bind_joints[side * 9:side * 9 + 3])
        expected_lower = np.linalg.norm(np.array(bind_joints[side * 9 + 6:side * 9 + 9]) - bind_joints[side * 9 + 3:side * 9 + 6])
        # Treatment fades from immutable bind to donor pose, so its elbow can
        # bend via interpolation. All six core calibrated limb lengths persist.
        if not direct:
            assert np.abs(lengths[:6] - expected_upper).max() < .000001
            assert np.abs(lower[:6] - expected_lower).max() < .000001
    assert np.max(np.abs(np.linalg.norm(poses[:, :, [5, 9]], axis=-1) - 1)) < .00001
    loop = max(np.linalg.norm(vertices[c, 0] - vertices[c, -1], axis=-1).max() for c in (0, 1, 5))
    root_drift = max(np.linalg.norm(poses[c, :, 0][:, [0, 2]], axis=-1).max() for c in (0, 1, 5))
    assert loop <= .006 and root_drift <= .004
    if not direct:
        assert vertices[:, :, :, 1].min() >= -.001
    weights = np.frombuffer((assets / (role + '-arms.bin')).read_bytes(), dtype=np.uint8, offset=104).reshape(corners, 4)
    owned = weights[:, 0] / 255 * weights[:, 2] / 255 > .15
    points = np.array([list(map(float, line.split()[1:4])) for line in (assets / (role + '.obj')).read_text().splitlines() if line.startswith('v ')])
    assert points.shape == (corners, 3) and np.isfinite(points).all()
    arm_data = (assets / (role + '-arms.bin')).read_bytes()
    assert arm_data[:4] == b'RMC2' and len(arm_data) == 104 + corners * 4
    assert struct.unpack_from('<I', arm_data, 100)[0] == corners
    skin_data = (assets / (role + '-skin.bin')).read_bytes()
    assert skin_data[:104] == (assets / (role + '-arms.bin')).read_bytes()[:104]
    assert len(skin_data) == 104 + corners * 4
    # Inspect all edges, not only pairs already labelled as fingers. That old
    # subset missed hanging sleeve sheets and a thigh mislabelled as a hand.
    faces = indices.reshape(-1, 3)
    bind_faces = points.reshape(-1, 3, 3)
    mesh_stretch = 1.0
    for corner in range(3):
        next_corner = (corner + 1) % 3
        rest_length = np.maximum(.01, np.linalg.norm(bind_faces[:, corner] - bind_faces[:, next_corner], axis=-1))
        posed_length = np.linalg.norm(vertices[:, :, faces[:, corner]] - vertices[:, :, faces[:, next_corner]], axis=-1)
        mesh_stretch = max(mesh_stretch, float(np.max(posed_length / rest_length)))
    assert mesh_stretch < 8, (role, 'whole mesh stretch', mesh_stretch)
    if direct:
        results.append(verify_direct(role, data, layer_end, samples, clips, count,
                                     corners, points, vertices, poses, weights,
                                     skin_data, mesh_stretch, clip_samples,
                                     total_samples))
        continue
    edges = []
    for a in range(corners):
        b = a // 3 * 3 + (a + 1) % 3
        if owned[a] and owned[b] and weights[a, 3] == weights[b, 3]:
            baseline = np.linalg.norm(points[a] - points[b])
            edges.append((int(indices[a]), int(indices[b]), baseline))
    assert edges
    a = np.array([e[0] for e in edges]); b = np.array([e[1] for e in edges]); rest = np.array([e[2] for e in edges])
    error = np.abs(np.linalg.norm(vertices[:, :, a] - vertices[:, :, b], axis=-1) - rest).max()
    mid = (vertices[:, :-1] + vertices[:, 1:]) * .5
    mid_errors = np.abs(np.linalg.norm(mid[:, :, a] - mid[:, :, b], axis=-1) - rest)
    mid_error = mid_errors.max()
    assert error <= .006 and mid_error <= .006, (role, error, mid_error, mid_errors.max(axis=(1, 2)))
    if role == 'medic':
        anchors = np.frombuffer((assets / 'medic-treatment-anchors.bin').read_bytes(), dtype='<f4').reshape(clips, samples, 2, 3)
        assert np.max(np.abs(anchors - poses[:, :, [4, 8]])) == 0
    crouch = vertices[5, :, :, 1].max(axis=1)
    results.append({'role': role, 'bytes': len(data), 'samples': samples, 'clips': clips, 'loop_error_mm': loop * 1000, 'planar_pelvis_drift_mm': root_drift * 1000, 'all_hand_keyframe_error_mm': error * 1000, 'all_hand_midframe_error_mm': mid_error * 1000, 'whole_mesh_edge_stretch': mesh_stretch, 'crouch_height_min_m': float(crouch.min()), 'crouch_height_max_m': float(crouch.max()), 'death_end_height_m': float(vertices[4, -1, :, 1].max()), 'finite_metadata': True})
report = {'protected_files_unchanged': len(manifest) if args.baseline else None, 'roles': results}
print(json.dumps(report, indent=2, default=float))
if args.output:
    args.output.parent.mkdir(parents=True,exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2, default=float))
