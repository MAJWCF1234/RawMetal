"""Blender -b --python tools/BakeCleanupMixamo.py

Retarget Mixamo donor packs onto each cleanup mesh with the same anatomical
limb/skin path as BakeCleanupSkin.py. Idle / walk / hit / crouch come from
Mixamo; punch and death stay on Quaternius UAL1 (no usable Mixamo donors).
Medic treatment clip 7 is re-appended from UAL2 afterward.
"""
import bpy, struct, shutil, json
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
mixamo = root / "source-assets" / "mixamo-motion"
ual_source = root / "source-assets" / "medic-motion"
ual1 = ual_source / "UAL1_Standard.fbx"
ual1_origin = Path(r"O:\retro official\Animations\Universal Animation Library[Standard]\Unity\UAL1_Standard.fbx")
if not ual1.exists():
    shutil.copyfile(ual1_origin, ual1)

ROLES = ("assault", "engineer", "sharpshooter", "heavy", "medic", "rusher")
SAMPLES = 8

# Runtime clip order stays fixed. Mixamo fills the ones that read as armed
# guards; UAL keeps melee and death.
MIXAMO_CLIPS = {
    0: mixamo / "basic-shooter" / "rifle aiming idle.fbx",  # Idle
    1: mixamo / "basic-shooter" / "walking.fbx",  # Walk
    3: mixamo / "basic-shooter" / "hit reaction.fbx",  # Hit
    5: mixamo / "pistol-handgun" / "pistol kneeling idle.fbx",  # Crouch
}
UAL_CLIPS = {
    2: "Punch_Cross",
    4: "Death01",
}

# Quaternius UAL bone → Mixamo Y-Bot bone
MIXAMO_BONES = {
    "pelvis": "mixamorig:Hips",
    "spine_02": "mixamorig:Spine2",
    "upperarm_r": "mixamorig:RightArm",
    "lowerarm_r": "mixamorig:RightForeArm",
    "upperarm_l": "mixamorig:LeftArm",
    "lowerarm_l": "mixamorig:LeftForeArm",
    "thigh_r": "mixamorig:RightUpLeg",
    "calf_r": "mixamorig:RightLeg",
    "foot_r": "mixamorig:RightFoot",
    "thigh_l": "mixamorig:LeftUpLeg",
    "calf_l": "mixamorig:LeftLeg",
    "foot_l": "mixamorig:LeftFoot",
}


def engine(p):
    return Vector((p.x, p.z, -p.y))


def clear_scene():
    bpy.ops.wm.read_homefile(use_empty=True)


def is_mixamo(rig):
    return any(b.name.startswith("mixamorig:") for b in rig.data.bones)


def bone_name(rig, logical):
    if logical in rig.data.bones:
        return logical
    mapped = MIXAMO_BONES.get(logical)
    if mapped and mapped in rig.data.bones:
        return mapped
    raise KeyError(f"{logical} missing on {rig.name}; have {[b.name for b in list(rig.data.bones)[:12]]}")


def joint(rig, logical, posed=False):
    name = bone_name(rig, logical)
    if is_mixamo(rig):
        # Mixamo FBX keeps cm keys under a 0.01 object scale. Read through
        # matrix_world so rest and posed samples stay in meters.
        if posed:
            return engine((rig.matrix_world @ rig.pose.bones[name].matrix).to_translation())
        return engine(rig.matrix_world @ rig.data.bones[name].head_local)
    return engine((rig.pose.bones[name].head if posed else rig.data.bones[name].head_local))


def direction(rig, logical, posed=False):
    name = bone_name(rig, logical)
    if is_mixamo(rig):
        if posed:
            bone = rig.pose.bones[name]
            world = rig.matrix_world @ bone.matrix
            head = world.to_translation()
            axis = (world.to_3x3() @ Vector((0, 1, 0))).normalized() * bone.length
            return engine(axis).normalized()
        bone = rig.data.bones[name]
        head = rig.matrix_world @ bone.head_local
        tip = rig.matrix_world @ bone.tail_local
        return engine(tip - head).normalized()
    b = rig.pose.bones[name] if posed else rig.data.bones[name]
    return engine((b.tail - b.head) if posed else (b.tail_local - b.head_local)).normalized()


def import_rig(path, strip_mesh=True):
    before = set(bpy.data.objects)
    before_actions = set(bpy.data.actions)
    bpy.ops.import_scene.fbx(filepath=str(path))
    created = [o for o in bpy.data.objects if o not in before]
    rig = next(o for o in created if o.type == "ARMATURE")
    if strip_mesh:
        for o in created:
            if o.type == "MESH":
                bpy.data.objects.remove(o, do_unlink=True)
    new_actions = [a for a in bpy.data.actions if a not in before_actions]
    return rig, new_actions


def find_ual_action(name):
    for action in bpy.data.actions:
        if action.name.endswith("|" + name) or action.name == name:
            return action
    raise KeyError(name)


def assign_action(rig, action):
    rig.animation_data_create()
    rig.animation_data.action = action
    if getattr(action, "slots", None):
        try:
            rig.animation_data.action_slot = action.slots[0]
        except Exception:
            pass


def load_rest_from_obj(stem, count, indices, corners, fallback):
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


def anatomical_frame(rig, points, joints, weights, action, phase, idle_rest=False):
    # Runtime bind check requires idle phase 0 == mesh rest within 4 mm.
    if idle_rest and phase <= 0:
        return [p.copy() for p in points]
    start, end = action.frame_range
    frame = start + (end - start) * phase
    bpy.context.scene.frame_set(int(frame), subframe=frame - int(frame))
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


def bake_clip(out, rig, action, points, joints, weights, samples, idle_rest=False):
    assign_action(rig, action)
    motion = 0.0
    rest = points
    for sample in range(samples):
        phase = sample / (samples - 1)
        frame = anatomical_frame(rig, points, joints, weights, action, phase, idle_rest=idle_rest)
        if sample == samples // 2:
            motion = max((frame[i] - rest[i]).length for i in range(len(points)))
        for p in frame:
            out.extend(struct.pack("<3h", *(int(round(v * 500)) * 2 for v in p)))
    return motion


def bake_role(stem, ual_rig, mixamo_actions):
    path, data, corners, _old_samples, clips, count, indices, points, joints, weights = load_topology(stem)
    assert clips >= 6
    samples = SAMPLES
    out = bytearray(data[: 20 + corners * 2])
    struct.pack_into("<I", out, 8, samples)
    struct.pack_into("<I", out, 12, 6)
    evidence_clips = []
    for clip in range(6):
        if clip in MIXAMO_CLIPS:
            rig, action = mixamo_actions[clip]
            motion = bake_clip(out, rig, action, points, joints, weights, samples, idle_rest=(clip == 0))
            evidence_clips.append({"clip": clip, "source": "mixamo", "file": MIXAMO_CLIPS[clip].name, "action": action.name, "motion": round(motion, 4)})
            print("CLEANUP_MIXAMO_CLIP", stem, clip, MIXAMO_CLIPS[clip].name, "motion", round(motion, 4))
        else:
            action = find_ual_action(UAL_CLIPS[clip])
            motion = bake_clip(out, ual_rig, action, points, joints, weights, samples, idle_rest=False)
            evidence_clips.append({"clip": clip, "source": "ual1", "action": action.name, "motion": round(motion, 4)})
            print("CLEANUP_MIXAMO_CLIP", stem, clip, UAL_CLIPS[clip], "motion", round(motion, 4))
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(out)
    temporary.replace(path)
    return {"stem": stem, "vertices": count, "corners": corners, "samples": samples, "clips": evidence_clips}


def main():
    for path in MIXAMO_CLIPS.values():
        if not path.exists():
            raise FileNotFoundError(path)

    # Each Mixamo FBX carries its own armature + single action. Keep them
    # resident so role bakes can switch without re-importing four times.
    clear_scene()
    mixamo_actions = {}
    for clip, path in sorted(MIXAMO_CLIPS.items()):
        rig, actions = import_rig(path)
        if not actions:
            raise RuntimeError(f"No action imported from {path}")
        action = next((a for a in actions if "mixamo" in a.name.lower()), actions[0])
        mixamo_actions[clip] = (rig, action)
        print("MIXAMO_IMPORT", clip, path.name, action.name, tuple(action.frame_range))

    ual_rig, _ = import_rig(ual1)
    tpose = find_ual_action("A_TPose")
    assign_action(ual_rig, tpose)
    bpy.context.scene.frame_set(int(tpose.frame_range[0]))

    evidence = {
        "method": "Mixamo anatomical limb retarget (idle/walk/hit/crouch) + UAL1 punch/death",
        "mixamo": {str(k): str(v.relative_to(root)) for k, v in MIXAMO_CLIPS.items()},
        "ual1": str(ual1.relative_to(root)),
        "roles": [],
    }
    for stem in ROLES:
        evidence["roles"].append(bake_role(stem, ual_rig, mixamo_actions))

    # Fresh Blender process so Mixamo armatures cannot steal UAL2 bone lookups.
    import subprocess, sys

    blender = Path(bpy.app.binary_path)
    medic = subprocess.run([str(blender), "-b", "--python", str(root / "tools" / "BakeMedicTreatment.py")], check=False)
    if medic.returncode != 0:
        raise RuntimeError(f"BakeMedicTreatment failed with {medic.returncode}")
    out = root / "diagnostics" / "cleanup-mixamo-bake.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    print("CLEANUP_MIXAMO_BAKE", out)
    print(json.dumps(evidence))


if __name__ == "__main__":
    main()
