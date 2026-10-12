"""Blender -b --python tools/ImportMilitaryDrone.py -- [--inspect]

Retains the downloaded originals, copies the supplied FBX/textures into the
source archive, and writes normalized Y-up derivatives with the original UVs.
"""
import bpy
import json
import math
import shutil
import struct
import sys
import zipfile
from pathlib import Path
from mathutils import Vector

root = Path(__file__).resolve().parents[1]
download = Path(r"C:\Users\majwc\Downloads\military-drone-low-poly")
source = root / "source-assets/military-drone"
source.mkdir(parents=True, exist_ok=True)
if not (source / "Drone1_Anims.fbx").exists():
    with zipfile.ZipFile(download / "source/Drone1.zip") as archive:
        for info in archive.infolist():
            if Path(info.filename).name != info.filename:
                raise ValueError("Unexpected archive path")
            (source / info.filename).write_bytes(archive.read(info))
bpy.ops.object.select_all(action="SELECT")
bpy.ops.object.delete(use_global=False)
bpy.ops.import_scene.fbx(filepath=str(source / "Drone1_Anims.fbx"))
objects = [obj for obj in bpy.data.objects if obj.type == "MESH"]
result = {"objects": [], "actions": []}
for obj in objects:
    raw = [obj.matrix_world @ v.co for v in obj.data.vertices]
    result["objects"].append({"name": obj.name, "vertices": len(raw), "polygons": len(obj.data.polygons),
        "bounds": [[min(p[i] for p in raw) for i in range(3)], [max(p[i] for p in raw) for i in range(3)]],
        "materials": [m.name if m else None for m in obj.data.materials]})
for action in bpy.data.actions:
    result["actions"].append({"name": action.name, "frames": list(action.frame_range)})
result["rigs"] = [{"name": obj.name, "bones": [bone.name for bone in obj.data.bones]} for obj in bpy.data.objects if obj.type == "ARMATURE"]
diagnostics = root / "diagnostics/military-drone-import.json"
diagnostics.parent.mkdir(parents=True, exist_ok=True)
diagnostics.write_text(json.dumps(result, indent=2), encoding="utf-8")
print("DRONE_INSPECT", json.dumps(result))
if "--inspect" in sys.argv:
    sys.exit(0)
if "--preview" in sys.argv:
    points = [obj.matrix_world @ vertex.co for obj in objects for vertex in obj.data.vertices]
    center = sum(points, Vector())/len(points)
    for image in bpy.data.images:
        for filename in ("Drone1_tex.png", "Drone1_Emissive.png"):
            if filename.lower().split(".")[0] in image.name.lower():
                image.filepath = str(source / filename)
                image.reload()
    for material in bpy.data.materials:
        material.use_nodes = True
        bsdf = material.node_tree.nodes.get("Principled BSDF")
        if bsdf:
            bsdf.inputs["Roughness"].default_value = .65
    camera_data = bpy.data.cameras.new("drone-preview")
    camera = bpy.data.objects.new("drone-preview", camera_data)
    bpy.context.collection.objects.link(camera)
    camera.location = center+Vector((3,-4,2))
    camera.rotation_euler = (center-camera.location).to_track_quat("-Z", "Y").to_euler()
    camera_data.type = "ORTHO"
    camera_data.ortho_scale = 2.8
    bpy.context.scene.camera = camera
    for location in ((3,-4,4), (-3,1,2)):
        data = bpy.data.lights.new("drone-light", "AREA")
        data.energy = 300
        data.shape = "DISK"
        data.size = 3
        light = bpy.data.objects.new("drone-light", data)
        bpy.context.collection.objects.link(light)
        light.location = center+Vector(location)
        light.rotation_euler = (center-light.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.world.color = (.12,.12,.12)
    bpy.context.scene.render.engine = "BLENDER_EEVEE"
    bpy.context.scene.render.resolution_x = 640
    bpy.context.scene.render.resolution_y = 640
    bpy.context.scene.render.resolution_percentage = 100
    bpy.context.scene.render.filepath = str(root / "diagnostics/military-drone-source.png")
    bpy.ops.render.render(write_still=True)
    sys.exit(0)

output = root / "src/assets/actors/drone"
output.mkdir(parents=True, exist_ok=True)
points = [obj.matrix_world @ vertex.co for obj in objects for vertex in obj.data.vertices]
lo = Vector(tuple(min(p[i] for p in points) for i in range(3)))
hi = Vector(tuple(max(p[i] for p in points) for i in range(3)))
center = (lo + hi) * .5
scale = .90 / max(hi.x-lo.x, hi.y-lo.y)
def normalized(p):
    d = (p-center)*scale
    # Source gun points +Y; engine NPCs and weapons point +Z. Rotate the
    # coordinate basis without reversing face winding or original UVs.
    return Vector((-d.x, (p.z-lo.z)*scale, d.y))

# Bake the supplied animated transforms while keeping one compact OBJ topology.
bpy.context.scene.frame_set(1)
lines, references, indices, base_points = ["# Military Drone original UVs; centred Y-up; 0.90m span", "o MilitaryDrone"], [], [], []
for obj in objects:
    obj.data.calc_loop_triangles()
    uv = obj.data.uv_layers.active.data
    offset = len(base_points)
    base_points.extend(normalized(obj.matrix_world @ v.co) for v in obj.data.vertices)
    for triangle in obj.data.loop_triangles:
        for vertex, loop in zip(triangle.vertices, triangle.loops):
            p = normalized(obj.matrix_world @ obj.data.vertices[vertex].co)
            lines.extend(("v %.7f %.7f %.7f" % tuple(p), "vt %.7f %.7f" % tuple(uv[loop].uv)))
            indices.append(offset+vertex)
        n = len(indices)-2
        lines.append(f"f {n}/{n} {n+1}/{n+1} {n+2}/{n+2}")
    references.append(obj)
(output / "military-drone.obj").write_text("\n".join(lines)+"\n", encoding="utf-8")
samples = 16
actions = [next(action for action in bpy.data.actions if action.name.endswith("Drone1_Idle")),
           next(action for action in bpy.data.actions if action.name.endswith("Drone1_Walk"))]
armature = next(obj for obj in bpy.data.objects if obj.type == "ARMATURE")
with (output / "military-drone-animation.bin").open("wb") as stream:
    stream.write(b"RMA2" + struct.pack("<4I", len(indices), samples, len(actions), len(base_points)))
    stream.write(struct.pack("<%dH" % len(indices), *indices))
    for action in actions:
        armature.animation_data.action = action
        if action.slots:
            armature.animation_data.action_slot = action.slots[0]
        for sample in range(samples):
            start, end = action.frame_range
            frame = start+(end-start)*sample/(samples-1)
            bpy.context.scene.frame_set(int(frame), subframe=frame-int(frame))
            posed = []
            for obj in references:
                evaluated = obj.evaluated_get(bpy.context.evaluated_depsgraph_get())
                mesh = evaluated.to_mesh()
                if len(mesh.vertices) != len(obj.data.vertices):
                    raise ValueError("Drone animation changes topology")
                posed.extend(normalized(evaluated.matrix_world @ vertex.co) for vertex in mesh.vertices)
                evaluated.to_mesh_clear()
            ground = min(p.y for p in posed)
            for p in posed:
                values = (p.x, p.y-ground, p.z)
                stream.write(struct.pack("<3h", *(round(v*1000) for v in values)))
shutil.copyfile(source / "Drone1_tex.png", output / "military-drone.png")
shutil.copyfile(source / "Drone1_Emissive.png", output / "military-drone-emission.png")
print("DRONE_EXPORT", len(indices)//3, "triangles", "dimensions", list((hi-lo)*scale), "clips", [action.name for action in actions])
