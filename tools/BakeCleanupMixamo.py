"""Bake donor motion onto immutable repaired cleanup geometry.

RMA2 vertices retain their corner map. RML1 describes the same grounded,
in-place calibrated joints so upper/lower composition has a shared body frame.
"""
import bpy, struct, shutil, json, math, time, bisect
from pathlib import Path
from mathutils import Vector, Matrix

root = Path(__file__).resolve().parents[1]
mixamo = root / "source-assets" / "mixamo-motion"
ual_source = root / "source-assets" / "medic-motion"
ual1 = ual_source / "UAL1_Standard.fbx"
ual1_origin = Path(r"O:\retro official\Animations\Universal Animation Library[Standard]\Unity\UAL1_Standard.fbx")
if not ual1.exists():
    shutil.copyfile(ual1_origin, ual1)

ROLES = ("assault", "engineer", "sharpshooter", "heavy", "medic", "rusher")
SAMPLES = 32

# Runtime clip order stays fixed.
MIXAMO_CLIPS = {
    0: mixamo / "basic-shooter" / "rifle aiming idle.fbx",  # Idle
    1: mixamo / "basic-shooter" / "walking.fbx",  # Walk
    2: mixamo / "basic-shooter" / "firing rifle.fbx",  # Punch / fire telegraph
    3: mixamo / "basic-shooter" / "hit reaction.fbx",  # Hit
    5: mixamo / "pro-rifle" / "idle crouching aiming.fbx",  # Crouch
}
PISTOL_CLIPS = {
    0: mixamo / "pistol-handgun" / "pistol idle.fbx",
    1: mixamo / "pistol-handgun" / "pistol walk.fbx",
    # This official pack has no firing take. Retain its handgun carry pose;
    # the live handgun action supplies recoil rather than a rifle hand pose.
    2: mixamo / "pistol-handgun" / "pistol idle.fbx",
    5: mixamo / "pistol-handgun" / "pistol kneeling idle.fbx",
}
UAL_CLIPS = {
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


def orientation(rig, logical, posed=False):
    name = bone_name(rig, logical)
    matrix = rig.pose.bones[name].matrix if posed else rig.data.bones[name].matrix_local
    if is_mixamo(rig):
        matrix = rig.matrix_world @ matrix
    axes = [engine(matrix.to_3x3().col[i]).normalized() for i in range(3)]
    return Matrix(axes).transposed().to_quaternion()


def limb_rotation(rig, logical, calibrated_direction):
    # Use the donor's full rest-relative basis, including roll. A shortest
    # vector-only rotation flips its roll when the forearm passes 180 degrees.
    rest = orientation(rig, logical)
    posed = orientation(rig, logical, True)
    alignment = calibrated_direction.normalized().rotation_difference(direction(rig, logical))
    return posed @ rest.conjugated() @ alignment


def collapse_samples(rig, action, samples):
    """Bounded forward samples derived from the donor's actual hand rotation."""
    assign_action(rig, action)
    start, end = action.frame_range
    steps = 200
    cumulative, previous = [0.0], None
    for index in range(steps + 1):
        frame = start + (end - start) * index / steps
        bpy.context.scene.frame_set(int(frame), subframe=frame - int(frame))
        rotations = [orientation(rig, "lowerarm_" + suffix, True) for suffix in ("r", "l")]
        if previous is not None:
            angles = [a.rotation_difference(b).angle for a, b in zip(previous, rotations)]
            travel = max(min(angle, 2 * math.pi - angle) for angle in angles)
            # Retain a little timing resolution through the static tail.
            cumulative.append(cumulative[-1] + travel + .1 / steps)
        previous = rotations
    phases = [0.0]
    for index in range(1, samples - 1):
        target = cumulative[-1] * index / (samples - 1)
        high = bisect.bisect_left(cumulative, target)
        low = high - 1
        fraction = (target - cumulative[low]) / (cumulative[high] - cumulative[low])
        phases.append((low + fraction) / steps)
    phases.append(1.0)
    print("UAL_COLLAPSE_SAMPLES", "angular_travel", cumulative[-1], "phases", phases)
    return phases


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


def load_rest_from_obj(stem, count, indices, corners):
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
            # Repaired topology can leave unused IDs in the retained index
            # bank. Duplicate a real bind vertex rather than rebaking a posed
            # frame as bind, which accumulates deformation on every rebuild.
            points[i] = points[indices[0]].copy()
    return points


def load_topology(stem):
    path = root / "src/assets/actors/cleanup" / f"{stem}-animation.bin"
    data = path.read_bytes()
    corners, samples, clips, count = struct.unpack_from("<4I", data, 4)
    assert data[:4] == b"RMA2"
    indices = struct.unpack_from(f"<{corners}H", data, 20)
    points = load_rest_from_obj(stem, count, indices, corners)
    calibration = (root / "src/assets/actors/cleanup" / f"{stem}-skin.bin").read_bytes()
    assert calibration[:4] == b"RMC2"
    joints = struct.unpack_from("<18f", calibration, 4)
    palms = struct.unpack_from("<6f", calibration, 76)
    weights = [None] * count
    for corner, index in enumerate(indices):
        weight = calibration[104 + corner * 4 : 108 + corner * 4]
        assert len(weight) == 4 and weight[3] in (0, 1)
        influence, lower, palm = (v / 255 for v in weight[:3])
        weights[index] = (influence, lower, palm, weight[3])
    for i, w in enumerate(weights):
        if w is None:
            weights[i] = weights[indices[0]]
    return path, data, corners, samples, clips, count, indices, points, joints, palms, weights


def atomic_write(path, data):
    if path.exists() and path.read_bytes() == data:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_bytes(data)
    # Windows resource compilation can briefly hold a shared asset handle.
    for attempt in range(30):
        try:
            temporary.replace(path)
            return
        except PermissionError:
            if attempt == 29:
                raise
            time.sleep(.1)


def anatomical_frame(rig, points, joints, palms, weights, action, phase):
    start, end = action.frame_range
    frame = start + (end - start) * phase
    bpy.context.scene.frame_set(int(frame), subframe=frame - int(frame))
    blend = 1.0
    rest_pelvis = joint(rig, "pelvis")
    pelvis = Vector((0, 0.78, 0))
    root_delta = (joint(rig, "pelvis", True) - rest_pelvis) * 0.9
    # Runtime navigation owns planar travel. Keep donor vertical articulation.
    root_delta.x = root_delta.z = 0.0
    torso_rotation = direction(rig, "spine_02").rotation_difference(direction(rig, "spine_02", True))
    root_rotation = direction(rig, "pelvis").rotation_difference(direction(rig, "pelvis", True))

    def torso(p):
        return pelvis + root_delta + torso_rotation @ (p - pelvis)

    arms, legs = [], []
    for side, suffix in enumerate(("r", "l")):
        S, E, H = (Vector(joints[side * 9 + i : side * 9 + i + 3]) for i in (0, 3, 6))
        target_s = torso(S)
        upper = limb_rotation(rig, "upperarm_" + suffix, E - S)
        lower = limb_rotation(rig, "lowerarm_" + suffix, H - E)
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
        influence, lower_weight, palm_weight, arm_side = weights[index]
        side = 0 if p.x < 0 else 1
        target = torso(p)
        if p.y < 0.88 and influence <= 0:
            hip, knee, ankle, H, K, A, thigh, calf, foot = legs[side]
            upper = H + thigh @ (p - hip)
            lower = K + calf @ (p - knee)
            bend = max(0, min(1, (0.46 - p.y) / 0.12))
            leg_target = upper.lerp(lower, bend)
            if p.y < 0.13:
                leg_target = leg_target.lerp(A + foot @ (p - ankle), max(0, min(1, (0.13 - p.y) / 0.06)))
            # A hard .79 m torso/leg cutoff tears the crouched waistband.
            # Blend the donor pelvis and spine frames across the whole waist.
            waist = max(0.0, min(1.0, (p.y - .70) / .18))
            waist = waist * waist * (3.0 - 2.0 * waist)
            target = leg_target.lerp(target, waist)
        if influence > 0:
            S, E, H, tS, tE, tH, upper, lower = arms[arm_side]
            if influence * palm_weight > 0.15:
                # Fingers remain one rigid hand, including repaired geometry
                # crossing the centre plane. RMC2 supplies its actual owner.
                target = tH + lower @ (p - H)
            else:
                arm = (tS + upper @ (p - S)).lerp(tE + lower @ (p - E), lower_weight)
                arm = arm.lerp(tH + lower @ (p - H), palm_weight)
                target = target.lerp(arm, influence)
        posed.append(p.lerp(target, blend))
    ground = min(p.y for p in posed)
    def grounded(p):
        return Vector((p.x, p.y - ground, p.z))
    layer = [grounded(pelvis + root_delta), grounded(torso(Vector((0, 0.92, 0))))]
    for side, arm in enumerate(arms):
        S, E, H, tS, tE, tH, upper, lower = arm
        palm = Vector(palms[side * 3:side * 3 + 3])
        layer.extend((grounded(tS), grounded(tE), grounded(tH), (lower @ palm).normalized()))
    return [grounded(p) for p in posed], layer


def bake_clip(out, rig, action, points, joints, palms, weights, samples, loop=False, source_phases=None):
    assign_action(rig, action)
    motion = 0.0
    rest = points
    frames, layers = [], []
    for sample in range(samples):
        phase = source_phases[sample] if source_phases else sample / (samples - 1) if samples > 1 else 0.0
        frame, layer = anatomical_frame(rig, points, joints, palms, weights, action, phase)
        frames.append(frame)
        layers.append(layer)
        if sample == samples // 2:
            motion = max((frame[i] - rest[i]).length for i in range(len(points)))
    if loop:
        # Sample a forward donor cycle; close its tiny residual wrap without
        # reversing the walk or substituting bind geometry for idle frame zero.
        frames[-1] = [p.copy() for p in frames[0]]
        layers[-1] = [p.copy() for p in layers[0]]
    for frame in frames:
        for p in frame:
            assert all(math.isfinite(v) and abs(v) < 32 for v in p)
            out.extend(struct.pack("<3h", *(int(round(v * 1000)) for v in p)))
    return motion, layers, max(p.y for f in frames for p in f)


def bake_role(stem, ual_rig, mixamo_actions, collapse_phases):
    path, data, corners, _old_samples, clips, count, indices, points, joints, palms, weights = load_topology(stem)
    assert clips >= 6
    samples = SAMPLES
    out = bytearray(data[: 20 + corners * 2])
    struct.pack_into("<I", out, 8, samples)
    struct.pack_into("<I", out, 12, 6)
    evidence_clips = []
    layers = []
    sources = dict(MIXAMO_CLIPS)
    if stem in ("medic", "engineer"):
        sources.update(PISTOL_CLIPS)
    for clip in range(6):
        if clip in sources:
            rig, action = mixamo_actions[sources[clip]]
            # RML1 describes these exact calibrated limbs for runtime layer
            # composition, torso aim, and final posed weapon attachment.
            motion, clip_layers, height = bake_clip(out, rig, action, points, joints, palms, weights, samples, loop=clip in (0, 1, 5))
            evidence_clips.append({"clip": clip, "source": "mixamo", "file": str(sources[clip].relative_to(root)), "action": action.name, "motion": round(motion, 4), "height": round(height, 5)})
            print("CLEANUP_MIXAMO_CLIP", stem, clip, sources[clip].name, "motion", round(motion, 4), "height", round(height, 5))
        else:
            action = find_ual_action(UAL_CLIPS[clip])
            # Death01 spends its last half nearly motionless. Allocate the
            # bounded sample budget to its actual collapse so interpolated
            # rigid hands do not flatten through an under-sampled rotation.
            motion, clip_layers, height = bake_clip(out, ual_rig, action, points, joints, palms, weights, samples, source_phases=collapse_phases)
            evidence_clips.append({"clip": clip, "source": "ual1", "action": action.name, "source_phases": collapse_phases, "motion": round(motion, 4), "height": round(height, 5)})
            print("CLEANUP_MIXAMO_CLIP", stem, clip, UAL_CLIPS[clip], "motion", round(motion, 4))
        layers.extend(clip_layers)
    out.extend(b"RML1" + struct.pack("<4I", 1, samples, 6, 120))
    for layer in layers:
        values = tuple(v for p in layer for v in p)
        assert len(values) == 30 and all(math.isfinite(v) for v in values)
        out.extend(struct.pack("<30f", *values))
    # The Medic's treatment append must finish before publishing its runtime
    # file. Never expose a temporary six-clip Medic to the resource packer.
    target = root / "diagnostics/medic-core-animation.bin" if stem == "medic" else path
    atomic_write(target, out)
    return {"stem": stem, "vertices": count, "corners": corners, "samples": samples, "clips": evidence_clips}


def main():
    import runpy
    runpy.run_path(str(root / "tools/BuildCleanupSkin.py"))
    sources = set(MIXAMO_CLIPS.values()) | set(PISTOL_CLIPS.values())
    for path in sources:
        if not path.exists():
            raise FileNotFoundError(path)

    # Each Mixamo FBX carries its own armature + single action. Keep them
    # resident so role bakes can switch without re-importing four times.
    clear_scene()
    mixamo_actions = {}
    for path in sorted(sources):
        rig, actions = import_rig(path)
        if not actions:
            raise RuntimeError(f"No action imported from {path}")
        action = next((a for a in actions if "mixamo" in a.name.lower()), actions[0])
        mixamo_actions[path] = (rig, action)
        print("MIXAMO_IMPORT", path.name, action.name, tuple(action.frame_range))

    ual_rig, _ = import_rig(ual1)
    tpose = find_ual_action("A_TPose")
    assign_action(ual_rig, tpose)
    bpy.context.scene.frame_set(int(tpose.frame_range[0]))
    collapse_phases = collapse_samples(ual_rig, find_ual_action("Death01"), SAMPLES)

    evidence = {
        "method": f"{SAMPLES} in-place grounded samples; connected skin bindings and calibrated RMC2 limbs; rigid hands; blended waist; RML1 body frame; 1 mm vertex quantization; closed forward loops; UAL1 death",
        "samples": SAMPLES,
        "mixamo": {str(k): str(v.relative_to(root)) for k, v in MIXAMO_CLIPS.items()},
        "ual1": str(ual1.relative_to(root)),
        "roles": [],
    }
    for stem in ROLES:
        evidence["roles"].append(bake_role(stem, ual_rig, mixamo_actions, collapse_phases))

    # Fresh Blender process so Mixamo armatures cannot steal UAL2 bone lookups.
    import subprocess, sys

    blender = Path(bpy.app.binary_path)
    medic = subprocess.run([str(blender), "-b", "--python-exit-code", "1", "--python", str(root / "tools" / "BakeMedicTreatment.py"), "--", "--core", str(root / "diagnostics/medic-core-animation.bin")], check=False)
    if medic.returncode != 0:
        raise RuntimeError(f"BakeMedicTreatment failed with {medic.returncode}")
    out = root / "diagnostics" / "cleanup-mixamo-bake.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(evidence, indent=2), encoding="utf-8")
    print("CLEANUP_MIXAMO_BAKE", out)
    print(json.dumps(evidence))


if __name__ == "__main__":
    main()
