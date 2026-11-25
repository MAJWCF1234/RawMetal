"""Prepare and audit a GoldSrc-informed Depthworks character in Blender.

The script imports a source FBX/GLB (or uses the open scene), preserves it in
DW_SOURCE, makes a triangulated decimation proposal in DW_GAME, and writes a
QC-like JSON handoff profile. It never silently exports an unreviewed game
asset: topology, UV seams, bake cages and gameplay readability need an artist.

Example:
  blender -b --python tools/PrepareDepthworksCharacter.py -- --source hero.fbx
      --output diagnostics/characters/hero --name hero --target-tris 2000
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
import bmesh

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--source', type=Path, help='FBX, glTF/GLB, or Blend source; omit for current scene')
parser.add_argument('--output', type=Path, default=ROOT / 'diagnostics' / 'characters' / 'unnamed')
parser.add_argument('--name', default='character')
parser.add_argument('--target-tris', type=int, default=2000)
parser.add_argument('--save-blend', action='store_true')
parser.add_argument('--apply-decimate', action='store_true', help='Apply proposed Decimate modifiers after review-free conversion')
args = parser.parse_args(sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else [])

HARD_TRIANGLES = 9000
SOURCE_MESH_LIMIT = 32
RUNTIME_CLIP_LIMIT = 32
SAMPLE_LIMIT = 512
ATTACHMENT_LIMIT = 4
CONTROLLER_LIMIT = 8
BONE_LIMIT = 65
INFLUENCE_LIMIT = 4


def collection(name):
    value = bpy.data.collections.get(name)
    if value is None:
        value = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(value)
    return value


def move_to(obj, target):
    for owner in list(obj.users_collection):
        owner.objects.unlink(obj)
    target.objects.link(obj)


def import_source(path):
    suffix = path.suffix.lower()
    if suffix == '.fbx':
        bpy.ops.import_scene.fbx(filepath=str(path))
    elif suffix in ('.gltf', '.glb'):
        bpy.ops.import_scene.gltf(filepath=str(path))
    elif suffix == '.blend':
        bpy.ops.wm.open_mainfile(filepath=str(path))
    else:
        raise ValueError('Expected FBX, glTF/GLB, or Blend source, got ' + suffix)


def triangles(objects):
    total = 0
    for obj in objects:
        obj.data.calc_loop_triangles()
        total += len(obj.data.loop_triangles)
    return total


def evaluated_triangles(objects):
    graph = bpy.context.evaluated_depsgraph_get()
    total = 0
    for obj in objects:
        evaluated = obj.evaluated_get(graph)
        mesh = evaluated.to_mesh()
        try:
            mesh.calc_loop_triangles()
            total += len(mesh.loop_triangles)
        finally:
            evaluated.to_mesh_clear()
    return total


def non_manifold(mesh):
    bm = bmesh.new(); bm.from_mesh(mesh)
    loose = sum(1 for v in bm.verts if not v.link_edges)
    bad_edges = sum(1 for edge in bm.edges if not edge.is_manifold)
    zero = sum(1 for face in bm.faces if face.calc_area() < 1e-12)
    bm.free()
    return dict(loose_vertices=loose, non_manifold_edges=bad_edges, zero_area_faces=zero)


def find_images(objects):
    images = set()
    materials = set()
    for obj in objects:
        for material in obj.data.materials:
            if not material:
                continue
            materials.add(material.name)
            if material.use_nodes:
                for node in material.node_tree.nodes:
                    if node.type == 'TEX_IMAGE' and node.image:
                        images.add(node.image)
    return sorted(materials), sorted(images, key=lambda image: image.name)


def influences(objects):
    maximum = 0
    for obj in objects:
        for vertex in obj.data.vertices:
            maximum = max(maximum, sum(1 for item in vertex.groups if item.weight > .0001))
    return maximum


def uv_report(objects):
    missing, outside = [], 0
    for obj in objects:
        layer = obj.data.uv_layers.active
        if layer is None:
            missing.append(obj.name)
            continue
        for item in layer.data:
            if item.uv.x < -.0001 or item.uv.x > 1.0001 or item.uv.y < -.0001 or item.uv.y > 1.0001:
                outside += 1
    return dict(missing=missing, outside_0_1=outside)


def copy_game_object(source, target, armature_map):
    game = source.copy(); game.data = source.data.copy(); game.name = 'CHR_' + args.name + '_' + source.name
    target.objects.link(game)
    for modifier in game.modifiers:
        if modifier.type == 'ARMATURE' and modifier.object in armature_map:
            modifier.object = armature_map[modifier.object]
    return game


if args.target_tris < 1000 or args.target_tris > 3000:
    raise ValueError('--target-tris must stay in the approved 1,000–3,000 authoring range')
if args.source:
    if not args.source.is_file():
        raise FileNotFoundError(args.source)
    bpy.ops.wm.read_factory_settings(use_empty=True)
    import_source(args.source)

source_collection = collection('DW_SOURCE')
game_collection = collection('DW_GAME')
for obj in list(game_collection.objects):
    bpy.data.objects.remove(obj, do_unlink=True)

scene_objects = list(bpy.context.scene.objects)
source_meshes = [obj for obj in scene_objects if obj.type == 'MESH']
source_rigs = [obj for obj in scene_objects if obj.type == 'ARMATURE']
if not source_meshes:
    raise RuntimeError('No mesh objects were found.')
for obj in scene_objects:
    if obj.name not in ('DW_SOURCE', 'DW_GAME'):
        move_to(obj, source_collection)

armature_map = {}
for rig in source_rigs:
    game_rig = rig.copy(); game_rig.data = rig.data.copy(); game_rig.name = 'CHR_' + args.name + '_RIG'
    game_collection.objects.link(game_rig); armature_map[rig] = game_rig
game_meshes = [copy_game_object(obj, game_collection, armature_map) for obj in source_meshes]

before = triangles(game_meshes)
ratio = min(1.0, args.target_tris / max(1, before))
for obj in game_meshes:
    bpy.context.view_layer.objects.active = obj; obj.select_set(True)
    triangulate = obj.modifiers.new('DW export triangulate', 'TRIANGULATE')
    if ratio < .999:
        decimate = obj.modifiers.new('DW review decimate', 'DECIMATE')
        decimate.ratio = ratio
        decimate.use_collapse_triangulate = True
        if args.apply_decimate:
            bpy.ops.object.modifier_apply(modifier=decimate.name)
    if args.apply_decimate:
        bpy.ops.object.modifier_apply(modifier=triangulate.name)
    obj.select_set(False)

after = evaluated_triangles(game_meshes)
materials, images = find_images(source_meshes)
all_actions = list(bpy.data.actions)
attachments = [obj for obj in source_collection.objects if obj.name.upper().startswith('ATT_')]
controllers = [key for rig in source_rigs for key in rig.keys() if str(key).upper().startswith('CTRL_')]
issues = []
if before > HARD_TRIANGLES:
    issues.append('Source exceeds 9,000 triangles; clean or split it before preparing.')
if len(source_meshes) > SOURCE_MESH_LIMIT:
    issues.append('More than 32 source meshes.')
if len(all_actions) > RUNTIME_CLIP_LIMIT:
    issues.append('More than 32 actions; Depthworks runtime rejects more than 32 baked clips.')
if len(attachments) > ATTACHMENT_LIMIT:
    issues.append('More than 4 ATT_* anchors.')
if len(controllers) > CONTROLLER_LIMIT:
    issues.append('More than 8 CTRL_* rig properties.')
if any(len(rig.data.bones) > BONE_LIMIT for rig in source_rigs):
    issues.append('More than 64 deform bones.')
max_influences = influences(source_meshes)
if max_influences > INFLUENCE_LIMIT:
    issues.append('A vertex has more than 4 non-zero bone influences.')

geometry = {obj.name: non_manifold(obj.data) for obj in source_meshes}
uv = uv_report(source_meshes)
if uv['missing'] or uv['outside_0_1']:
    issues.append('UV handoff is incomplete: every export mesh needs one 0–1 UV set.')
if len(images) != 1 or (images and tuple(images[0].size) != (1024, 1024)):
    issues.append('Expected exactly one 1024×1024 diffuse image for the final game asset.')

profile = {
    'schema': 'depthworks-character-profile/v1',
    'name': 'CHR_' + args.name.upper(),
    'workflow': {'source_collection': 'DW_SOURCE', 'game_collection': 'DW_GAME',
                 'projection_source': 'keep DW_SOURCE out of export',
                 'decimation': 'proposal only; manually repair silhouette and clothing before baking'},
    'limits': {'target_triangles': args.target_tris, 'review_triangles': 3000, 'reject_triangles': HARD_TRIANGLES,
               'source_meshes_max': SOURCE_MESH_LIMIT, 'runtime_clips_max': RUNTIME_CLIP_LIMIT,
               'samples_per_clip_max': SAMPLE_LIMIT, 'attachments_max': ATTACHMENT_LIMIT,
               'controllers_max': CONTROLLER_LIMIT, 'bones_max': BONE_LIMIT, 'influences_per_vertex_max': INFLUENCE_LIMIT},
    'observed': {'source_triangles': before, 'proposed_game_triangles': after, 'source_meshes': len(source_meshes),
                 'game_meshes': len(game_meshes), 'bones': {rig.name: len(rig.data.bones) for rig in source_rigs},
                 'actions': [action.name for action in all_actions], 'attachments': [obj.name for obj in attachments],
                 'controllers': controllers, 'max_vertex_influences': max_influences, 'materials': materials,
                 'images': [{'name': image.name, 'size': list(image.size), 'path': image.filepath} for image in images],
                 'uv': uv, 'geometry': geometry},
    'issues': issues,
    'next_step': 'Manually repair DW_GAME, join approved game meshes, bake/project one 1024 diffuse, then run BakeCleanupDirect.py.'
}
args.output.mkdir(parents=True, exist_ok=True)
(args.output / 'depthworks-character-profile.json').write_text(json.dumps(profile, indent=2), encoding='utf-8')
if args.save_blend:
    bpy.ops.wm.save_as_mainfile(filepath=str(args.output / ('CHR_' + args.name + '_prepared.blend')))
print(json.dumps(profile, indent=2))
if issues:
    raise RuntimeError('Depthworks character profile failed: ' + ' | '.join(issues))
