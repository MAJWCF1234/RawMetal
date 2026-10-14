"""Mirror Assault's intact authored hand onto its wrist stub.

Writes a staged OBJ and matching RMC2 binding for the new Mixamo rig.
The donor UVs remain unchanged; reflection reverses triangle winding.
All body triangles and the intact hand are retained exactly.
"""
from pathlib import Path
import json
import struct
import numpy as np

root = Path(__file__).resolve().parents[1]
source = root / 'src/assets/actors/cleanup'
output = root / 'source-assets/cleanup-repairs'
output.mkdir(parents=True, exist_ok=True)
lines = (source / 'assault.obj').read_text().splitlines()
points, uvs, faces, materials = [], [], [], []
material = 'cleanup_body'
for line in lines:
    tokens = line.split()
    if not tokens:
        continue
    if tokens[0] == 'v':
        points.append(tuple(map(float, tokens[1:4])))
    elif tokens[0] == 'vt':
        uvs.append(tuple(map(float, tokens[1:3])))
    elif tokens[0] == 'usemtl':
        material = tokens[1]
    elif tokens[0] == 'f':
        assert len(tokens) == 4
        face = [tuple(int(v) - 1 for v in token.split('/')[:2]) for token in tokens[1:]]
        faces.append(face)
        materials.append(material)
points, uvs, faces = np.array(points), np.array(uvs), np.array(faces)
assert np.array_equal(faces[:, :, 0].reshape(-1), np.arange(len(points)))
calibration = (source / 'assault-arms.bin').read_bytes()
assert calibration[:4] == b'RMC2'
weights = np.frombuffer(calibration, dtype='u1', offset=104).reshape(-1, 3, 4)
assert len(weights) == len(faces)
hand = (weights[:, :, 2] > 128).any(axis=1)
donor = hand & (weights[:, :, 3] == 0).all(axis=1)
stub = hand & (weights[:, :, 3] == 1).all(axis=1)
assert donor.sum() > 10 * stub.sum(), 'Expected intact hand and opposite stub'
keep = ~stub
retained = points[faces[keep, :, 0]]
mirrored = points[faces[donor, :, 0]][:, ::-1].copy()
mirrored[:, :, 0] *= -1
stub_points = points[faces[stub, :, 0]]
body_nodes = set(map(tuple, np.round(retained.reshape(-1, 3), 6)))
old_boundary = set(map(tuple, np.round(stub_points.reshape(-1, 3), 6))) & body_nodes
new_boundary = set(map(tuple, np.round(mirrored.reshape(-1, 3), 6))) & body_nodes
assert len(old_boundary) == 13 and old_boundary == new_boundary, 'Cuff seam must match exactly'
new_points = np.concatenate((retained, mirrored)).reshape(-1, 3)
new_uvs = np.concatenate((uvs[faces[keep, :, 1]], uvs[faces[donor, :, 1]][:, ::-1])).reshape(-1, 2)
mirrored_weights = weights[donor, ::-1].copy()
mirrored_weights[:, :, 3] = 1
new_weights = np.concatenate((weights[keep], mirrored_weights)).reshape(-1, 4)
new_materials = [m for m, retain in zip(materials, keep) if retain]
new_materials.extend(m for m, selected in zip(materials, donor) if selected)
obj = ['# Depthworks Assault; missing hand replaced with mirrored intact authored hand', 'o Cleanup']
for p, uv in zip(new_points, new_uvs):
    obj += ['v ' + ' '.join(format(float(v), '.7g') for v in p), 'vt ' + ' '.join(format(float(v), '.7g') for v in uv)]
for triangle, material in enumerate(new_materials):
    obj.append('usemtl ' + material)
    obj.append('f ' + ' '.join(f'{i + 1}/{i + 1}' for i in range(triangle * 3, triangle * 3 + 3)))
(output / 'assault.obj').write_text('\n'.join(obj) + '\n')
header = bytearray(calibration[:104])
struct.pack_into('<I', header, 100, len(new_points))
(output / 'assault-arms.bin').write_bytes(header + new_weights.tobytes())
# Report the vertex-bank size for the later animated bake. Do not create a
# placeholder animation or pair this new topology with the old game bank.
keys = np.concatenate((np.round(new_points, 6), new_weights.astype(float)), axis=1)
_, first, indices = np.unique(keys, axis=0, return_index=True, return_inverse=True)
bind = new_points[first]
assert len(bind) < 65536
report = dict(donor_triangles=int(donor.sum()), removed_stub_triangles=int(stub.sum()),
              cuff_points=len(new_boundary), retained_triangles=int(keep.sum()),
              new_triangles=len(new_materials), corner_count=len(new_points),
              animation_vertices=len(bind), donor_uvs_preserved=True,
              reflected_winding_reversed=True, body_and_donor_preserved=True)
(output / 'assault-hand-repair.json').write_text(json.dumps(report, indent=2))
print(json.dumps(report, indent=2))
