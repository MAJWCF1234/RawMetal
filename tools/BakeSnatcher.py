"""Build the Snatcher's runtime FBX: its own rig and idle, plus four Mixamo
motions retargeted onto it.

  blender -b --python tools/BakeSnatcher.py -- SNATCHER.glb OUT.fbx

The Snatcher (O:/retro official/psx-snatcher-low-poly-horror.zip) is modelled in
an A-pose; the motions are Mixamo takes on a T-pose rig with the same 65 bones.
Each bone copies the motion bone's absolute orientation (so arms go where the
take's arms go, not where they would from a different rest), the hips copy its
position with the take's rig scaled to the Snatcher's hip height, and every clip
is baked to keys over the window that holds the action. The clips go out as NLA
strips, one take each, for RawMetalAuthoredCreatureBake (clip names below).
"""
import bpy, sys
from pathlib import Path

root = Path(__file__).resolve().parents[1]
glb, out = Path(sys.argv[sys.argv.index('--') + 1]), Path(sys.argv[sys.argv.index('--') + 2])

# name: (take, first frame, last frame); a None take is the Snatcher's own idle
# (24 fps), of which three seconds make the loop.
IDLE_WINDOW = (1, 73)
CLIPS = {
    'Snatcher_Idle': None,
    # It limps rather than runs: the injured walk (in place, 1.9 m a cycle at
    # 1.17 m/s planted-foot speed) is its creep and, played faster, its fade.
    'Snatcher_Creep': (root / 'source-assets' / 'survivor_injured_walk.fbx', 1, 49),
    'Snatcher_Snatch': (root / 'source-assets' / 'transformed_neck_bite.fbx', 6, 36),
    'Snatcher_Stagger': (root / 'source-assets' / 'transformed_hit_stagger.fbx', 11, 34),
    'Snatcher_Death': (root / 'source-assets' / 'transformed_death_back.fbx', 20, 62),
}

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(glb))
for o in list(bpy.context.scene.objects):
    if o.type == 'MESH' and o.parent is None:  # a stray icosphere ships with the model
        bpy.data.objects.remove(o)
arm = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
arm.name = 'Snatcher'
hips = 'mixamorig:Hips'
idle = arm.animation_data.action
idle.name = 'Snatcher_Idle'; idle.use_fake_user = True
idle_range = IDLE_WINDOW
arm.animation_data.action = None
for b in arm.pose.bones:
    b.rotation_mode = 'QUATERNION'

baked = {}
for name, clip in CLIPS.items():
    if clip is None:
        continue
    take, first, last = clip
    before = set(bpy.context.scene.objects)
    bpy.ops.import_scene.fbx(filepath=str(take))
    added = [o for o in bpy.context.scene.objects if o not in before]
    src = next(o for o in added if o.type == 'ARMATURE')
    # Scale the take's rig so its hips stand at the Snatcher's hip height.
    ratio = arm.data.bones[hips].head_local.length / src.data.bones[hips].head_local.length
    src.scale = [s * ratio * arm.scale[0] / src.scale[0] for s in src.scale]
    src.location = arm.location
    bpy.context.view_layer.update()
    for b in arm.pose.bones:
        if b.name not in src.pose.bones:
            continue
        c = b.constraints.new('COPY_ROTATION'); c.target = src; c.subtarget = b.name
        c.target_space = c.owner_space = 'WORLD'
        if b.name == hips:
            c = b.constraints.new('COPY_LOCATION'); c.target = src; c.subtarget = b.name
            c.target_space = c.owner_space = 'WORLD'
    bpy.context.view_layer.objects.active = arm
    for o in bpy.context.scene.objects:
        o.select_set(o == arm)
    bpy.ops.object.mode_set(mode='POSE')
    bpy.ops.pose.select_all(action='SELECT')
    bpy.ops.nla.bake(frame_start=first, frame_end=last, only_selected=True, visual_keying=True,
                     clear_constraints=True, use_current_action=False, bake_types={'POSE'})
    bpy.ops.object.mode_set(mode='OBJECT')
    action = arm.animation_data.action
    action.name = name; action.use_fake_user = True
    baked[name] = (action, first, last)
    arm.animation_data.action = None
    for o in added:
        bpy.data.objects.remove(o, do_unlink=True)

# One NLA strip per clip, each on its own track, exported as its own take.
ad = arm.animation_data or arm.animation_data_create()
ad.action = None
for name in CLIPS:
    action, first, last = (idle, *idle_range) if CLIPS[name] is None else baked[name]
    track = ad.nla_tracks.new(); track.name = name
    strip = track.strips.new(name, first, action)
    if hasattr(strip, 'action_slot') and action.slots:
        strip.action_slot = action.slots[0]
    strip.action_frame_start, strip.action_frame_end = first, last
    track.mute = False

out.parent.mkdir(parents=True, exist_ok=True)
bpy.ops.export_scene.fbx(filepath=str(out), use_selection=False, object_types={'ARMATURE', 'MESH'},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
                         bake_anim_use_nla_strips=True, bake_anim_force_startend_keying=True,
                         path_mode='COPY', embed_textures=False)
print('SNATCHER exported', out, 'clips', list(CLIPS))
