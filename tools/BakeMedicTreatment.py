"""Append UAL2 Chest_Open using immutable repaired Medic geometry.

The six RMA2 clips and their RML1 poses remain byte-for-byte intact. Treatment
gets the same calibrated, grounded joint metadata and actual wrist anchors.
"""
import bpy, struct, shutil, json, math, importlib.util, sys
from pathlib import Path
from mathutils import Vector, Quaternion

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("cleanup_bake", root / "tools/BakeCleanupMixamo.py")
bake = importlib.util.module_from_spec(spec)
spec.loader.exec_module(bake)
source = root / "source-assets/medic-motion"
source.mkdir(parents=True, exist_ok=True)
donor = source / "UAL2_Standard.fbx"
if not donor.exists():
    shutil.copyfile(Path(r"O:\retro official\Animations\Universal Animation Library 2[Standard]\Unity\UAL2_Standard.fbx"), donor)
bake.clear_scene()
rig, actions = bake.import_rig(donor)
action = next(a for a in actions if a.name.endswith("|Chest_Open"))
bake.assign_action(rig, action)
path, data, corners, samples, clips, count, indices, points, joints, palms, weights = bake.load_topology("medic")
if "--" in sys.argv:
    arguments = sys.argv[sys.argv.index("--") + 1:]
    if len(arguments) != 2 or arguments[0] != "--core":
        raise ValueError("Expected --core <staged animation>")
    staged = Path(arguments[1]).read_bytes()
    staged_corners, samples, clips, staged_count = struct.unpack_from("<4I", staged, 4)
    assert staged[:4] == b"RMA2" and staged_corners == corners and staged_count == count
    assert staged[20:20 + corners * 2] == data[20:20 + corners * 2]
    data = staged
assert clips in (6, 7) and samples == bake.SAMPLES
offset = 20 + corners * 2
vertex_end = offset + clips * samples * count * 6
assert data[vertex_end:vertex_end + 4] == b"RML1", "Rebake core Mixamo clips before appending treatment"
version, pose_samples, pose_clips, stride = struct.unpack_from("<4I", data, vertex_end + 4)
assert (version, pose_samples, pose_clips, stride) == (1, samples, clips, 120)
assert len(data) == vertex_end + 20 + clips * samples * stride
core = data[:offset + 6 * samples * count * 6]
core_poses = data[vertex_end + 20:vertex_end + 20 + 6 * samples * 120]
identity = Quaternion((1, 0, 0, 0))
bind_layer = [Vector((0, .78, 0)), Vector((0, .92, 0))]
for side in range(2):
    bind_layer.extend(Vector(joints[side * 9 + i:side * 9 + i + 3]) for i in (0, 3, 6))
    bind_layer.append(Vector(palms[side * 3:side * 3 + 3]).normalized())
frames, layers = [], []
for sample in range(samples):
    phase = sample / (samples - 1)
    blend = max(0, min(1, phase / .18, (1 - phase) / .14))
    blend = blend * blend * (3 - 2 * blend)
    full, layer = bake.anatomical_frame(rig, points, joints, palms, weights, action, phase)
    posed_layer = [a.lerp(b, blend) for a, b in zip(bind_layer, layer)]
    hand_rotations = []
    for side, suffix in enumerate(("r", "l")):
        E = Vector(joints[side * 9 + 3:side * 9 + 6])
        H = Vector(joints[side * 9 + 6:side * 9 + 9])
        lower = bake.limb_rotation(rig, "lowerarm_" + suffix, H - E)
        rotation = identity.slerp(lower, blend)
        hand_rotations.append(rotation)
        posed_layer[5 + side * 4] = (rotation @ bind_layer[5 + side * 4]).normalized()
    frame = []
    for index, p in enumerate(points):
        influence, lower_weight, palm_weight, side = weights[index]
        if influence * palm_weight > .15:
            H = Vector(joints[side * 9 + 6:side * 9 + 9])
            frame.append(posed_layer[4 + side * 4] + hand_rotations[side] @ (p - H))
        else:
            frame.append(p.lerp(full[index], blend))
    ground = min(p.y for p in frame)
    for p in frame:
        p.y -= ground
    for index, p in enumerate(posed_layer):
        if index not in (5, 9):
            p.y -= ground
    frames.append(frame)
    layers.append(posed_layer)

result = bytearray(core)
struct.pack_into("<I", result, 12, 7)
for frame in frames:
    for p in frame:
        assert all(math.isfinite(v) and abs(v) < 32 for v in p)
        result.extend(struct.pack("<3h", *(round(v * 1000) for v in p)))
result.extend(b"RML1" + struct.pack("<4I", 1, samples, 7, 120))
result.extend(core_poses)
for layer in layers:
    result.extend(struct.pack("<30f", *(v for p in layer for v in p)))
bake.atomic_write(path, result)

anchor_data = bytearray()
all_poses = core_poses + b"".join(struct.pack("<30f", *(v for p in layer for v in p)) for layer in layers)
for clip in range(7):
    for sample in range(samples):
        pose = struct.unpack_from("<30f", all_poses, (clip * samples + sample) * 120)
        anchor_data.extend(struct.pack("<6f", *pose[12:15], *pose[24:27]))
bake.atomic_write(root / "src/assets/actors/cleanup/medic-treatment-anchors.bin", anchor_data)
evidence = {
    "source": str(donor.relative_to(root)), "action": action.name,
    "frames": list(action.frame_range), "samples": samples, "vertices": count,
    "runtime_clip": 6, "preserved_core_clips": 6, "preserved_core_layer_poses": 6,
    "method": "immutable repaired OBJ; calibrated RMC2 side ownership; rigid whole hands; in-place grounded UAL2; matching RML1 wrists and anchors",
}
(root / "diagnostics/medic-treatment-bake.json").write_text(json.dumps(evidence, indent=2))
print(json.dumps(evidence))
