"""Verify cleanup bakes against repaired geometry without running Blender.

Requires NumPy. Optional --baseline checks a saved Name/Sha256 manifest before
validating every clip, loop, protected hand edge and calibrated joint frame.
"""
from pathlib import Path
import struct, json, hashlib, argparse
import numpy as np

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--baseline',type=Path)
parser.add_argument('--output',type=Path)
args=parser.parse_args()
root = Path(__file__).resolve().parents[1]
assets = root / 'src/assets/actors/cleanup'
manifest = json.loads(args.baseline.read_text(encoding='utf-8-sig')) if args.baseline else []
for item in manifest:
    assert hashlib.sha256((assets / item['Name']).read_bytes()).hexdigest().upper() == item['Sha256'], item['Name']
results = []
for role in ('assault', 'engineer', 'sharpshooter', 'heavy', 'medic', 'rusher'):
    data = (assets / (role + '-animation.bin')).read_bytes()
    corners, samples, clips, count = struct.unpack_from('<4I', data, 4)
    assert data[:4] == b'RMA2' and 2 <= samples <= 128 and clips == (7 if role == 'medic' else 6)
    offset = 20 + corners * 2
    end = offset + clips * samples * count * 6
    assert data[end:end + 4] == b'RML1' and struct.unpack_from('<4I', data, end + 4) == (1, samples, clips, 120)
    assert len(data) == end + 20 + clips * samples * 120
    indices = np.frombuffer(data, dtype='<u2', count=corners, offset=20)
    assert indices.max() < count
    vertices = np.frombuffer(data, dtype='<i2', count=clips * samples * count * 3, offset=offset).reshape(clips, samples, count, 3) * .001
    poses = np.frombuffer(data, dtype='<f4', count=clips * samples * 30, offset=end + 20).reshape(clips, samples, 10, 3)
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
        assert np.abs(lengths[:6] - expected_upper).max() < .000001
        assert np.abs(lower[:6] - expected_lower).max() < .000001
    assert np.max(np.abs(np.linalg.norm(poses[:, :, [5, 9]], axis=-1) - 1)) < .00001
    loop = max(np.linalg.norm(vertices[c, 0] - vertices[c, -1], axis=-1).max() for c in (0, 1, 5))
    root_drift = max(np.linalg.norm(poses[c, :, 0][:, [0, 2]], axis=-1).max() for c in (0, 1, 5))
    assert loop <= .006 and root_drift <= .004
    assert vertices[:, :, :, 1].min() >= -.001
    weights = np.frombuffer((assets / (role + '-arms.bin')).read_bytes(), dtype=np.uint8, offset=104).reshape(corners, 4)
    owned = weights[:, 0] / 255 * weights[:, 2] / 255 > .15
    points = np.array([list(map(float, line.split()[1:4])) for line in (assets / (role + '.obj')).read_text().splitlines() if line.startswith('v ')])
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
