"""Run with Blender -b source-assets/cleanup-team/extracted/MercenariesPack/final.blend --python this_file -- --inspect."""
import bpy
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
if "--inspect" in args:
    result = {"objects": [], "actions": [], "images": []}
    for obj in bpy.data.objects:
        item = {"name": obj.name, "type": obj.type, "location": list(obj.location), "scale": list(obj.scale), "parent": obj.parent.name if obj.parent else None}
        if obj.type == "MESH":
            item.update(vertices=len(obj.data.vertices), polygons=len(obj.data.polygons), materials=[m.name if m else None for m in obj.data.materials], groups=[g.name for g in obj.vertex_groups], modifiers=[{"type": m.type, "object": m.object.name if m.type == "ARMATURE" and m.object else None} for m in obj.modifiers])
        if obj.type == "ARMATURE":
            item["bones"] = [{"name": b.name, "parent": b.parent.name if b.parent else None, "head": list(b.head_local), "tail": list(b.tail_local)} for b in obj.data.bones]
        result["objects"].append(item)
    for action in bpy.data.actions:
        result["actions"].append({"name": action.name, "frames": list(action.frame_range)})
    for image in bpy.data.images:
        result["images"].append({"name": image.name, "path": image.filepath, "size": list(image.size)})
    out = root / "diagnostics" / "cleanup-actor-import.json"
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("CLEANUP_INSPECTION", out)
if "--preview" in args:
    from mathutils import Vector
    actor_name = args[args.index("--actor")+1] if "--actor" in args else "Assault"
    obj = bpy.data.objects[actor_name]
    obj.modifiers.clear()
    for other in bpy.data.objects:
        other.hide_render = other != obj
    for image in bpy.data.images:
        if actor_name in image.name:
            image.filepath = str(Path(bpy.data.filepath).parent / actor_name / (actor_name+ ("Head" if "Head" in image.name else "Body")+".png"))
            image.reload()
    corners = [obj.matrix_world @ Vector(v) for v in obj.bound_box]
    center = sum(corners, Vector()) / 8
    size = max((max(v[i] for v in corners) - min(v[i] for v in corners)) for i in range(3))
    print("CLEANUP_PREVIEW_BOUNDS", actor_name, [min(v[i] for v in corners) for i in range(3)], [max(v[i] for v in corners) for i in range(3)])
    camera_data = bpy.data.cameras.new("inspection-camera")
    camera = bpy.data.objects.new("inspection-camera", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = center + Vector((size * 2, -size * .5, size * .1))
    camera.rotation_euler = (center - camera.location).to_track_quat("-Z", "Y").to_euler()
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = size * 1.2
    bpy.context.scene.camera = camera
    light_data = bpy.data.lights.new("inspection-light", "AREA")
    light_data.energy = 500
    light_data.shape = "DISK"
    light_data.size = size * 2
    light = bpy.data.objects.new("inspection-light", light_data)
    bpy.context.collection.objects.link(light)
    light.location = center + Vector((size, -size, size * 1.5))
    light.rotation_euler = (center - light.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.render.engine = "BLENDER_EEVEE"
    bpy.context.scene.render.resolution_x = 640
    bpy.context.scene.render.resolution_y = 640
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.render.filepath = str(root / "diagnostics" / f"cleanup-{actor_name.lower()}-source.png")
    bpy.ops.render.render(write_still=True)
if "--export" in args:
    import math
    import shutil
    import struct
    from mathutils import Vector
    actor_name = args[args.index("--actor")+1] if "--actor" in args else "Assault"
    stem = actor_name.lower()
    obj = bpy.data.objects[actor_name]
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    # Apply scale first so later world sampling stays uniform.
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    # Geometry Nodes on Assault/Medic/Heavy/Rusher build the authored hands.
    # Clearing modifiers without applying them previously exported the broken
    # cage mesh (spaghetti fingers). Apply every modifier in order, skipping
    # live Armature so we keep the rest pose rather than a posed deform.
    for mod in list(obj.modifiers):
        if mod.type == "ARMATURE":
            obj.modifiers.remove(mod)
            continue
        try:
            bpy.ops.object.modifier_apply(modifier=mod.name)
        except Exception as ex:
            print("CLEANUP_MODIFIER_SKIP", actor_name, mod.name, mod.type, ex)
            obj.modifiers.remove(mod)
    mesh = obj.data
    mesh.calc_loop_triangles()
    raw = [obj.matrix_world @ v.co for v in mesh.vertices]
    lo, hi = min(p.z for p in raw), max(p.z for p in raw)
    scale = 1.6 / (hi - lo)
    cx = (min(p.x for p in raw) + max(p.x for p in raw)) * .5
    cy = (min(p.y for p in raw) + max(p.y for p in raw)) * .5
    # The supplied Assault faces +X in Blender; normalize its forward to +Z.
    points = [Vector(((p.y-cy)*scale, (p.z-lo)*scale, (p.x-cx)*scale)) for p in raw]
    groups = [{obj.vertex_groups[g.group].name: g.weight for g in vertex.groups} for vertex in mesh.vertices]
    if "--stats-only" in args:
        statistics = {}
        for group in obj.vertex_groups:
            selected = [p for p,g in zip(points,groups) if g.get(group.name,0)>.5]
            statistics[group.name] = {"vertices":len(selected),"bounds":[[round(min(p[i] for p in selected),4) for i in range(3)],[round(max(p[i] for p in selected),4) for i in range(3)]] if selected else None}
        print("CLEANUP_GROUPS",actor_name,json.dumps(statistics))
        print("CLEANUP_DIMENSIONS",actor_name,[round(max(p[i] for p in points)-min(p[i] for p in points),4) for i in range(3)])
        parents=list(range(len(points)))
        def find(i):
            while parents[i]!=i:
                parents[i]=parents[parents[i]]
                i=parents[i]
            return i
        for edge in mesh.edges:
            a,b=(find(i) for i in edge.vertices)
            parents[a]=b
        components={}
        for index in range(len(points)):
            components.setdefault(find(index),[]).append(index)
        print("CLEANUP_COMPONENTS",actor_name,json.dumps([{"vertices":len(part),"hands":sum(groups[i].get("hands",0)>.5 for i in part),"bounds":[[round(min(points[j][i] for j in part),4) for i in range(3)],[round(max(points[j][i] for j in part),4) for i in range(3)]]} for part in components.values() if len(part)>12]))
        sys.exit(0)
    # Keep real authored arm groups where available. Several other source
    # models contain empty sleeve groups; their populated hand group gives the
    # wrist boundary for a narrow geometric sleeve mask above the belt.
    if actor_name in ("Assault", "Heavy"):
        arm_weights = [min(1, g.get("arms", 0) + g.get("hands", 0)) for g in groups]
        hand_weights = [g.get("hands", 0) for g in groups]
        # Heavy's authored arm group also includes 28 torso vertices. They are
        # jacket/gear anchors, not sleeves; preserve them in the standing body.
        if actor_name == "Heavy":
            arm_weights = [0 if h<=.5 and any(g.get(name,0)>.5 for name in ("torso","belt","holsters")) else a for a,h,g in zip(arm_weights,hand_weights,groups)]
    elif actor_name in ("Engineer", "Medic", "Rusher"):
        hand_weights = [g.get("hands", 0) for g in groups]
        sleeve_base = .80 if actor_name == "Engineer" else max(p.y for p,h in zip(points,hand_weights) if h>.5)-.025
        arm_weights = [max(h,max(0,min(1,(abs(p.x)-.165)/.055)) if sleeve_base < p.y < 1.355 else 0) for p,h in zip(points,hand_weights)]
    else:
        arm_weights = [max(0, min(1, (abs(p.x)-.165)/.055)) if .775 < p.y < 1.31 else 0 for p in points]
        hand_weights = [1 if p.y < .80 and p.y > .56 and abs(p.x) > .19 else 0 for p in points]
        arm_weights = [max(a,h) for a,h in zip(arm_weights,hand_weights)]
    joints, palms = [], []
    for side in (-1, 1):
        hand = [p for p,h in zip(points,hand_weights) if h > .5 and p.x*side > .04]
        wrist_y = max(p.y for p in hand) - .008
        wrist_band = [p for p in hand if p.y > wrist_y-.022]
        wrist = Vector((sum(p.x for p in wrist_band)/len(wrist_band), wrist_y, sum(p.z for p in wrist_band)/len(wrist_band)))
        shoulder_y = {"Sharpshooter":1.24,"Heavy":1.30,"Rusher":1.30}.get(actor_name,1.28)
        for height in (shoulder_y, (shoulder_y+wrist_y)*.5):
            band = [p for p,a in zip(points,arm_weights) if a>.5 and abs(p.y-height)<.065 and p.x*side>.04]
            joint = Vector((sum(p.x for p in band)/len(band),height,sum(p.z for p in band)/len(band)))
            joints.extend(joint)
        joints.extend(wrist)
        if actor_name in ("Heavy","Medic","Rusher"):
            palm_band = [p for p in hand if wrist_y-.095 < p.y < wrist_y-.055]
            assert palm_band, f"{actor_name} has no usable palm band on side {side}"
            palm = sum(palm_band,Vector())/len(palm_band)-wrist
            palms.extend(palm)
        else:
            palms.extend((0,-.075,.030))
    # Do not bake a closed gun grip into the rest mesh. An earlier import pass
    # rotated every distal "hands" vertex around a guessed knuckle (~-1.65 rad)
    # and permanently wrote spaghetti fingers / corkscrewed wrists into the OBJ
    # and animation bins. Runtime arm IK already wraps palms onto the weapon.
    output = root / "src/assets/actors/cleanup"
    output.mkdir(parents=True, exist_ok=True)
    uv = mesh.uv_layers.active.data
    corners, corner_indices, lines = [], [], [f"# Depthworks cleanup {actor_name}; normalized Y-up; authored UVs", "o Cleanup"]
    materials = []
    for tri in mesh.loop_triangles:
        name = obj.data.materials[tri.material_index].name.lower()
        material = "cleanup_head" if "head" in name else "cleanup_body"
        materials.append(material)
        for vi, li in zip(tri.vertices, tri.loops):
            p = points[vi]
            corners.append(p)
            corner_indices.append(vi)
            lines.append("v %.7f %.7f %.7f" % tuple(p))
            lines.append("vt %.7f %.7f" % tuple(uv[li].uv))
    for index, material in enumerate(materials):
        lines.append("usemtl " + material)
        i = index*3+1
        lines.append(f"f {i}/{i} {i+1}/{i+1} {i+2}/{i+2}")
    (output / f"{stem}.obj").write_text("\n".join(lines)+"\n", encoding="utf-8")
    assert len(corners) < 65536
    samples, clips = 16, 6
    def rotate_x(p, pivot, angle):
        d = p-pivot
        c, s = math.cos(angle), math.sin(angle)
        return pivot + Vector((d.x, d.y*c-d.z*s, d.y*s+d.z*c))
    with (output / f"{stem}-animation.bin").open("wb") as stream:
        stream.write(b"RMA2" + struct.pack("<4I", len(corners), samples, clips, len(points)))
        stream.write(struct.pack("<%dH" % len(corners), *corner_indices))
        for clip in range(clips):
            for sample in range(samples):
                phase = sample/(samples-1)
                posed = []
                for source in points:
                    p = source.copy()
                    if clip == 1 and p.y < .80:
                        direction = 1 if p.x > 0 else -1
                        angle = .48*math.sin(phase*2*math.pi)*direction
                        p = rotate_x(p, Vector((p.x, .78, 0)), angle*min(1, (.80-p.y)/.15))
                        if source.y < .40:
                            knee = max(0, -direction*math.sin(phase*2*math.pi))*.55
                            p = rotate_x(p, Vector((p.x, .40, p.z)), -knee)
                    elif clip == 2:
                        p = rotate_x(p, Vector((0,.78,0)), -.16*math.sin(phase*math.pi))
                    elif clip == 3:
                        p = rotate_x(p, Vector((0,.78,0)), .12*math.sin(phase*math.pi))
                    elif clip == 4:
                        p = rotate_x(p, Vector((0,0,0)), min(1, phase*1.5)*math.pi*.5)
                    elif clip == 5 and p.y > .95:
                        a = .12*math.sin(phase*2*math.pi)
                        p.x, p.z = p.x*math.cos(a)+p.z*math.sin(a), -p.x*math.sin(a)+p.z*math.cos(a)
                    posed.append(p)
                # Torso lean can drive a boot below zero as well as walking or
                # falling. Ground every sample before quantization.
                ground = min(p.y for p in posed)
                for p in posed:
                    values = (p.x, p.y-ground, p.z)
                    stream.write(struct.pack("<3h", *(round(v*1000) for v in values)))
    attachment = bytearray(b"RMC2" + struct.pack("<24fI", *(joints+palms), len(corners)))
    for index in corner_indices:
        p = points[index]
        side = 0 if p.x < 0 else 1
        shoulder,elbow,wrist = [Vector(joints[side*9+i:side*9+i+3]) for i in (0,3,6)]
        influence = arm_weights[index] * max(0,min(1,(shoulder.y+.045-p.y)/.13))
        lower = max(0,min(1,(elbow.y+.05-p.y)/.10))
        palm = hand_weights[index] * max(0,min(1,(wrist.y+.015-p.y)/.025))
        attachment.extend((round(influence*255),round(lower*255),round(palm*255),side))
    (output / f"{stem}-arms.bin").write_bytes(attachment)
    source = Path(bpy.data.filepath).parent / actor_name
    shutil.copyfile(source / f"{actor_name}Body.png", output / f"{stem}-body.png")
    shutil.copyfile(source / f"{actor_name}Head.png", output / f"{stem}-head.png")
    rigid_groups = ("belt","torso","armor","holsters")
    rigid_affected = sum(a>.01 and any(g.get(name,0)>.5 for name in rigid_groups) for a,g in zip(arm_weights,groups))
    print("CLEANUP_EXPORT", actor_name, len(materials), "triangles", len(points), "vertices", clips, "newly authored clips", "joints", joints,"palms",palms,"arm_vertices",sum(a>.5 for a in arm_weights),"hand_vertices",sum(h>.5 for h in hand_weights),"rigid_group_vertices_affected",rigid_affected)
    if actor_name in ("Assault","Heavy") and rigid_affected:
        # Prefer a clean sleeve mask, but do not abort a successful mesh write.
        # Assault's authored groups overlap a few jacket anchors; zeroing those
        # arm weights below keeps attachment calibration from yanking the torso.
        print("CLEANUP_WARNING", actor_name, "arm mask overlaps", rigid_affected, "belt/torso/gear verts; zeroing those arm weights")
        for i,g in enumerate(groups):
            if arm_weights[i]>.01 and any(g.get(name,0)>.5 for name in rigid_groups):
                arm_weights[i]=0
        attachment = bytearray(b"RMC2" + struct.pack("<24fI", *(joints+palms), len(corners)))
        for index in corner_indices:
            p = points[index]
            side = 0 if p.x < 0 else 1
            shoulder,elbow,wrist = [Vector(joints[side*9+i:side*9+i+3]) for i in (0,3,6)]
            influence = arm_weights[index] * max(0,min(1,(shoulder.y+.045-p.y)/.13))
            lower = max(0,min(1,(elbow.y+.05-p.y)/.10))
            palm = hand_weights[index] * max(0,min(1,(wrist.y+.015-p.y)/.025))
            attachment.extend((round(influence*255),round(lower*255),round(palm*255),side))
        (output / f"{stem}-arms.bin").write_bytes(attachment)
    if actor_name == "Medic":
        # The medical interaction is retargeted from the retained UAL2 source
        # after the six baseline clips and attachment calibration exist.
        import runpy
        runpy.run_path(str(root / "tools/BakeMedicTreatment.py"), run_name="__main__")
