"""Derive connected arm bindings without modifying repaired OBJ or RMC2 files.

Diffuse membership over welded mesh edges, rather than Euclidean distance to
the torso (which mislabels an inner sleeve resting against the abdomen).
The resulting RMC2 skin resources are shared by the bake and live attachment.
"""
from pathlib import Path
import struct
import numpy as np

root = Path(__file__).resolve().parents[1] / 'src/assets/actors/cleanup'
for role in ('assault', 'engineer', 'sharpshooter', 'heavy', 'medic', 'rusher'):
    points = np.array([list(map(float, line.split()[1:4])) for line in (root / f'{role}.obj').read_text().splitlines() if line.startswith('v ')])
    source = (root / f'{role}-arms.bin').read_bytes()
    assert source[:4] == b'RMC2' and struct.unpack_from('<I', source, 100)[0] == len(points)
    assert len(source) == 104 + len(points) * 4
    original = np.frombuffer(source, dtype=np.uint8, offset=104).reshape(-1, 4)
    unique, inverse = np.unique(np.round(points, 6), axis=0, return_inverse=True)
    weights = np.zeros((len(unique), 4))
    for corner, node in enumerate(inverse):
        weights[node, :3] = np.maximum(weights[node, :3], original[corner, :3] / 255)
        weights[node, 3] = original[corner, 3]
    faces = inverse.reshape(-1, 3)
    edges = np.unique(np.sort(np.concatenate([faces[:, [0, 1]], faces[:, [1, 2]], faces[:, [2, 0]]]), axis=1), axis=0)
    edges = edges[edges[:, 0] != edges[:, 1]]
    a, b = np.concatenate((edges[:, 0], edges[:, 1])), np.concatenate((edges[:, 1], edges[:, 0]))
    strength = 1 / np.maximum(.005, np.linalg.norm(unique[a] - unique[b], axis=1))
    total = np.bincount(a, weights=strength, minlength=len(unique))
    owned = weights[:, 0] * weights[:, 2] > .15
    # Calibration can contain a single spatial false positive on a thigh near
    # a hanging finger. It has no protected hand edge and must not pull a leg
    # up to the gun. Keep connected repaired hand surfaces, reject lone seeds.
    neighbours = np.bincount(a, weights=owned[b], minlength=len(unique))
    isolated = owned & (neighbours == 0)
    owned[isolated] = False
    arm = owned | ((weights[:, 0] > .8) & (unique[:, 1] < 1.20))
    arm &= ~isolated
    # Long repaired fingers can hang below .70 m. A horizontal "leg" cutoff
    # there pins neighbouring hand vertices to the thigh. Only the feet and
    # central torso/head are stationary seeds; connectivity identifies cuffs.
    body = ((np.abs(unique[:, 0]) < .10) | (unique[:, 1] < .40) | (unique[:, 1] > 1.34)) & ~owned
    body |= isolated
    fixed = arm | body
    membership = weights[:, 0].copy()
    membership[arm] = 1
    membership[body] = 0
    for iteration in range(1200):
        average = np.bincount(a, weights=strength * membership[b], minlength=len(unique)) / np.maximum(1, total)
        updated = np.where(fixed, membership, average)
        if np.max(np.abs(updated - membership)) < 1e-7:
            break
        membership = updated
    # Leave the transition around the shoulder; distal sleeves are wholly arm.
    membership = np.clip((membership - .20) / .60, 0, 1)
    membership = membership * membership * (3 - 2 * membership)
    membership[owned] = 1
    result = original.copy()
    result[:, 0] = np.round(membership[inverse] * 255).astype(np.uint8)
    result[isolated[inverse], 2] = 0
    output = source[:104] + result.tobytes()
    path = root / f'{role}-skin.bin'
    if not path.exists() or path.read_bytes() != output:
        path.write_bytes(output)
    print(role, 'nodes', len(unique), 'arm seeds', int(arm.sum()), 'isolated false positives', int(isolated.sum()), 'iterations', iteration + 1)
