"""Blender -b --python tools/BakeCleanupSkin.py

Drive Quaternius UAL1 pose bones in Blender, retarget onto each cleanup mesh with
calibrated sleeve/palm skin weights, bake the six standard clips into existing
RMA2 topology, then re-append Medic treatment from UAL2.
"""
import bpy, struct, shutil, json
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
source = root / "source-assets" / "medic-motion"
source.mkdir(parents=True, exist_ok=True)
ual1 = source / "UAL1_Standard.fbx"
ual1_origin = Path(r"O:\retro official\Animations\Universal Animation Library[Standard]\Unity\UAL1_Standard.fbx")
if not ual1.exists():
    shutil.copyfile(ual1_origin, ual1)

CLIPS = [
    "Idle_Loop",
    "Walk_Loop",
    "Punch_Cross",
    "Hit_Chest",
    "Death01",
    "Crouch_Idle_Loop",
]
ROLES = ("assault", "engineer", "sharpshooter", "heavy", "medic", "rusher")
# 8 samples keeps locomotion readable under the OoT 32 MiB exe ceiling;
# UAL-driven frames compress worse than the old procedural fakes.
SAMPLES = 8


def engine(p):
    return Vector((p.x, p.z, -p.y))


def clear_scene():
    bpy.ops.wm.read_homefile(use_empty=True)


def import_ual(path):
    before = set(bpy.data.objects)
    bpy.ops.import_scene.fbx(filepath=str(path))
    created = [o for o in bpy.data.objects if o not in before]
    rig = next(o for o in created if o.type == "ARMATURE")
    for o in created:
        if o.type == "MESH":
            bpy.data.objects.remove(o, do_unlink=True)
    return rig


def find_action(name):
    for action in bpy.data.actions:
        if action.name.endswith("|" + name) or action.name == name:
            return action
    raise KeyError(name)


def joint(rig, name, posed=False):
    return engine((rig.pose.bones[name].head if posed else rig.data.bones[name].head_local))


def direction(rig, name, posed=False):
    b = rig.pose.bones[name] if posed else rig.data.bones[name]
    return engine((b.tail - b.head) if posed else (b.tail_local - b.head_local)).normalized()


def load_rest_from_obj(stem, count, indices, corners, fallback):
    """OBJ stores one vertex per corner; recover unique bind positions."""
    lines = (root / "src/assets/actors/cleanup" / f"{stem}.obj").read_text(encoding="utf-8").splitlines()
    corner_points = []
    for line in lines:
        if line.startswith("v "):
            _, x, y, z = line.split()[:4]
            corner_points.append(Vector((float(x), float(y), float(z))))
    assert len(corner_points) == corners, (stem, len(corner_points), corners)
    points = [None] * count
    for corner, index in enumerate(indices):
        if points[index] is None:
            points[index] = corner_points[corner].copy()
    # A few authored verts sit outside triangles; keep their prior unique slots.
    for i, p in enumerate(points):
        if p is None:
            points[i] = fallback[i].copy()
    return points


def load_topology(stem):
    path = root / "src/assets/actors/cleanup" / f"{stem}-animation.bin"
    data = path.read_bytes()
    corners, samples, clips, count = struct.unpack_from("<4I", data, 4)
    assert data[:4] == b"RMA2"
    indices = struct.unpack_from(f"<{corners}H", data, 20)
    offset = 20 + corners * 2
    fallback = [Vector(tuple(v * 0.001 for v in struct.unpack_from("<3h", data, offset + i * 6))) for i in range(count)]
    points = load_rest_from_obj(stem, count, indices, corners, fallback)
    calibration = (root / "src/assets/actors/cleanup" / f"{stem}-arms.bin").read_bytes()
    joints = struct.unpack_from("<18f", calibration, 4)
    weights = [None] * count
    for corner, index in enumerate(indices):
        weights[index] = tuple(v / 255 for v in calibration[104 + corner * 4 : 107 + corner * 4])
    for i, w in enumerate(weights):
        if w is None:
            weights[i] = (0.0, 0.0, 0.0)
    return path, data, corners, samples, clips, count, indices, points, joints, weights


def anatomical_frame(rig, points, joints, weights, action, phase, samples):
    # Runtime bind check requires idle phase 0 == mesh rest within 4 mm.
    if (action.name.endswith("|Idle_Loop") or action.name.endswith("Idle_Loop")) and phase <= 0:
        return [p.copy() for p in points]
    start, end = action.frame_range
    frame = start + (end - start) * phase
    bpy.context.scene.frame_set(int(frame), subframe=frame - int(frame))
    blend = 1.0
    if action.name.endswith("|Idle_Loop") or action.name.endswith("Idle_Loop"):
        # Sample 0 stays exact rest; later samples take the full UAL idle so
        # standing NPCs read as animated once runtime phase advances.
        blend = 1.0
    rest_pelvis = joint(rig, "pelvis")
    pelvis = Vector((0, 0.78, 0))
    root_delta = (joint(rig, "pelvis", True) - rest_pelvis) * 0.9
    torso_rotation = direction(rig, "spine_02").rotation_difference(direction(rig, "spine_02", True))
    root_rotation = direction(rig, "pelvis").rotation_difference(direction(rig, "pelvis", True))

    def torso(p):
        return pelvis + root_delta + torso_rotation @ (p - pelvis)

    arms, legs = [], []
    for side, suffix in enumerate(("r", "l")):
        S, E, H = (Vector(joints[side * 9 + i : side * 9 + i + 3]) for i in (0, 3, 6))
        target_s = torso(S)
        upper = (E - S).normalized().rotation_difference(direction(rig, "upperarm_" + suffix, True))
        lower = (H - E).normalized().rotation_difference(direction(rig, "lowerarm_" + suffix, True))
        target_e = target_s + upper @ (E - S)
        target_h = target_e + lower @ (H - E)
        arms.append((S, E, H, target_s, target_e, target_h, upper, lower))
        sign = -1 if side == 0 else 1
        hip = Vector((sign * 0.12, 0.78, 0))
        knee = Vector((sign * 0.12, 0.40, 0))
        ankle = Vector((sign * 0.12, 0.08, 0))
        target_hip = pelvis + root_delta + root_rotation @ (hip - pelvis)
        thigh = (knee - hip).normalized().rotation_difference(direction(rig, "thigh_" + suffix, True))
        calf = (ankle - knee).normalized().rotation_difference(direction(rig, "calf_" + suffix, True))
        target_knee = target_hip + thigh @ (knee - hip)
        target_ankle = target_knee + calf @ (ankle - knee)
        foot = direction(rig, "foot_" + suffix).rotation_difference(direction(rig, "foot_" + suffix, True))
        legs.append((hip, knee, ankle, target_hip, target_knee, target_ankle, thigh, calf, foot))

    posed = []
    for index, p in enumerate(points):
        side = 0 if p.x < 0 else 1
        target = torso(p)
        if p.y < 0.79:
            hip, knee, ankle, H, K, A, thigh, calf, foot = legs[side]
            upper = H + thigh @ (p - hip)
            lower = K + calf @ (p - knee)
            bend = max(0, min(1, (0.46 - p.y) / 0.12))
            target = upper.lerp(lower, bend)
            if p.y < 0.13:
                target = target.lerp(A + foot @ (p - ankle), max(0, min(1, (0.13 - p.y) / 0.06)))
        influence, lower_weight, palm_weight = weights[index]
        if influence > 0:
            S, E, H, tS, tE, tH, upper, lower = arms[side]
            arm = (tS + upper @ (p - S)).lerp(tE + lower @ (p - E), lower_weight)
            arm = arm.lerp(tH + lower @ (p - H), palm_weight)
            target = target.lerp(arm, influence)
        posed.append(p.lerp(target, blend))
    ground = min(p.y for p in posed)
    return [Vector((p.x, p.y - ground, p.z)) for p in posed]


def bake_role(stem, rig):
    path, data, corners, _old_samples, clips, count, indices, points, joints, weights = load_topology(stem)
    assert clips >= 6
    samples = SAMPLES
    # Prefer anatomical retarget driven by live UAL pose bones: cleanup meshes are
    # unrigged low-poly and automatic weights on a foreign Mixamo-like skeleton
    # pull jacket/gear verts. Arm calibration already encodes sleeve/palm skin.
    out = bytearray(data[: 20 + corners * 2])
    struct.pack_into("<I", out, 8, samples)
    struct.pack_into("<I", out, 12, 6)
    evidence_clips = []
    for clip_name in CLIPS:
        action = find_action(clip_name)
        rig.animation_data_create()
        rig.animation_data.action = action
        if getattr(action, "slots", None):
            try:
                rig.animation_data.action_slot = action.slots[0]
            except Exception:
                pass
        motion = 0.0
        rest = points
        for sample in range(samples):
            phase = sample / (samples - 1)
            frame = anatomical_frame(rig, points, joints, weights, action, phase, samples)
            if sample == samples // 2:
                motion = max((frame[i] - rest[i]).length for i in range(count))
            for p in frame:
                out.extend(struct.pack("<3h", *(int(round(v * 500)) * 2 for v in p)))
        evidence_clips.append({"action": action.name, "motion": round(motion, 4)})
        print("CLEANUP_SKIN_CLIP", stem, clip_name, "motion", round(motion, 4))
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(out)
    temporary.replace(path)
    return {"stem": stem, "vertices": count, "corners": corners, "samples": samples, "clips": evidence_clips}


def main():
    clear_scene()
    rig = import_ual(ual1)
    # Keep A_TPose rest for bone local axes before assigning clip actions.
    tpose = find_action("A_TPose")
    rig.animation_data_create()
    rig.animation_data.action = tpose
    if getattr(tpose, "slots", None):
        try:
            rig.animation_data.action_slot = tpose.slots[0]
        except Exception:
            pass
    bpy.context.scene.frame_set(int(tpose.frame_range[0]))

    evidence = {"source": str(ual1.relative_to(root)), "method": "UAL1 anatomical limb retarget with calibrated sleeve/palm skin weights", "roles": []}
    for stem in ROLES:
        evidence["roles"].append(bake_role(stem, rig))

    # Medic treatment clip 7 stays on UAL2 Chest_Open.
    import runpy

    runpy.run_path(str(root / "tools" / "BakeMedicTreatment.py"), run_name="__main__")
    out = root / "diagnostics" / "cleanup-skin-bake.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    print("CLEANUP_SKIN_BAKE", out)
    print(json.dumps(evidence))


if __name__ == "__main__":
    main()
