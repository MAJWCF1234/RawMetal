"""Rig and animate the Chimera, then write its runtime FBX.

  blender -b --python tools/RigChimera.py -- CHIMERA.glb OUT.fbx

The Chimera (O:/retro official/chimera.zip) ships as eleven rigid parts and no
skeleton: a horse-like lower body with its four legs in one mesh, a torso with
arms, a petalled flower-mouth head, a tail. This builds a skeleton with knees
and elbows (each leg split out of the lower body below its barrel and again at
the knee; each arm at the elbow), weights every part rigidly to its bone, and
keys five clips at 30 fps for RawMetalAuthoredCreatureBake:

  Chimera_Idle    2.5 s loop: laboured breathing, the head jerking round in
                  twitches, a forehoof pawing, the petals fluttering
  Chimera_Walk    1.25 s loop: a wolf's gait (the CC0 Quaternius wolf's walk,
                  tools/data/wolf_walk.json from ExtractWolfGait.py) under a
                  hunched torso that claws forward, head low and thrusting
  Chimera_Attack  1.2 s: it dips, rears onto its hind legs with the petals
                  flaring open, and slams down arms first
  Chimera_Pain    0.7 s: a jolt back, the petals snapping shut, an overshoot
  Chimera_Death   1.8 s: it rears in agony, buckles at the front knees and
                  rolls onto its side

Blender axes: Z up, the creature faces -Y. For a bone hanging down (legs,
arms) a negative X rotation swings it forward; for the torso, positive leans
it forward; for the body, negative lifts the nose.
"""
import bpy, sys, math, json
from pathlib import Path
from mathutils import Vector

glb, out = sys.argv[sys.argv.index('--') + 1], sys.argv[sys.argv.index('--') + 2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.context.scene.render.fps = 30
bpy.ops.import_scene.gltf(filepath=glb)
parts = {o.name: o for o in bpy.context.scene.objects if o.type == 'MESH'}
for o in parts.values():
    o.select_set(True)
bpy.context.view_layer.objects.active = next(iter(parts.values()))
bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

LEG_TOP, KNEE, ELBOW = .68, .36, 1.62
LEGS = {'FL': (.22, .30), 'FR': (-.22, .30), 'BL': (.27, 1.35), 'BR': (-.27, 1.35)}
BONES = {'Root': ((0, .70, 0), (0, 1.0, 0), None), 'Body': ((0, .75, .95), (0, 1.25, .95), 'Root')}
for leg, (x, y) in LEGS.items():
    BONES['Thigh' + leg] = ((x, y, .75), (x, y, KNEE), 'Body')
    BONES['Shin' + leg] = ((x, y, KNEE), (x, y, 0), 'Thigh' + leg)
BONES.update({
    'Torso': ((0, .05, 1.15), (0, .05, 1.95), 'Body'),
    'UpperArmL': ((.24, .05, 2.0), (.28, .03, ELBOW), 'Torso'), 'ForearmL': ((.28, .03, ELBOW), (.30, 0, 1.2), 'UpperArmL'),
    'UpperArmR': ((-.24, .05, 2.0), (-.28, .03, ELBOW), 'Torso'), 'ForearmR': ((-.28, .03, ELBOW), (-.30, 0, 1.2), 'UpperArmR'),
    'Head': ((0, 0, 2.12), (0, 0, 2.5), 'Torso'),
    'Petals': ((0, 0, 2.3), (0, -.1, 2.6), 'Head'),
    'Tail': ((0, 1.4, 1.2), (0, 1.6, 1.0), 'Body'),
})
bpy.ops.object.select_all(action='DESELECT')
arm_data = bpy.data.armatures.new('ChimeraRig')
rig = bpy.data.objects.new('Chimera', arm_data)
bpy.context.scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
bpy.ops.object.mode_set(mode='EDIT')
for name, (head, tail, parent) in BONES.items():
    b = arm_data.edit_bones.new(name)
    b.head, b.tail, b.roll = Vector(head), Vector(tail), 0
    if parent:
        b.parent = arm_data.edit_bones[parent]
bpy.ops.object.mode_set(mode='OBJECT')

def bone_for(part, p):
    if part.startswith('Head.Petals'):
        return 'Petals'
    if part.startswith('Head'):
        return 'Head'
    if 'Tail' in part:
        return 'Tail'
    if part.startswith('UpperBody'):
        if part == 'UpperBody' and abs(p.x) > .2 and p.z < 2.08:
            return ('UpperArm' if p.z >= ELBOW else 'Forearm') + ('L' if p.x > 0 else 'R')
        return 'Torso'
    if part == 'LowerBody' and p.z < LEG_TOP:
        leg = ('F' if p.y < .7 else 'B') + ('L' if p.x > 0 else 'R')
        return ('Thigh' if p.z >= KNEE else 'Shin') + leg
    return 'Body'

for name, o in parts.items():
    groups = {b: o.vertex_groups.new(name=b) for b in BONES}
    for v in o.data.vertices:
        groups[bone_for(name, v.co)].add([v.index], 1.0, 'REPLACE')
    o.parent = rig
    o.modifiers.new('Rig', 'ARMATURE').object = rig
for pb in rig.pose.bones:
    pb.rotation_mode = 'XYZ'
rig.animation_data_create()

def put(action, frame, pose):
    """pose: bone -> dict(rot=(x,y,z), loc=(x,y,z), scale=s); unnamed bones rest."""
    rig.animation_data.action = action
    for pb in rig.pose.bones:
        p = pose.get(pb.name, {})
        pb.rotation_euler = p.get('rot', (0, 0, 0)); pb.location = p.get('loc', (0, 0, 0))
        s = p.get('scale', 1.0); pb.scale = (s, s, s)
        for path in ('rotation_euler', 'location', 'scale'):
            pb.keyframe_insert(path, frame=frame)

clips = {}
def sampled(name, frames, pose_at, step=2):
    """Keys pose_at(t) for t in [0,1] every `step` frames (and the last)."""
    action = bpy.data.actions.new(name); action.use_fake_user = True
    keys = list(range(1, frames + 1, step))
    if keys[-1] != frames:
        keys.append(frames)
    for f in keys:
        put(action, f, pose_at((f - 1) / (frames - 1)))
    clips[name] = (action, 1, frames)

def mix(a, b, w):
    return a + (b - a) * w
def ease(w):
    w = min(1, max(0, w)); return w * w * (3 - 2 * w)
def ramp(t, t0, t1):
    return ease((t - t0) / max(1e-6, t1 - t0))
BUD = .55
S = math.sin; TAU = 2 * math.pi

def snap(t, at, hold):
    """A twitch: 0 until `at`, snaps to 1 in a frame or two, holds, eases back."""
    return ramp(t, at, at + .03) * (1 - ramp(t, at + hold, at + hold + .12))

def idle(t):
    a = TAU * t
    breath = S(2 * a) * .5 + .5  # two heaving breaths a loop
    # The head does not look round smoothly: it jerks to a bearing, holds, jerks on.
    look = .45 * snap(t, .12, .22) - .55 * snap(t, .52, .2) + .2 * snap(t, .83, .06)
    cock = .3 * snap(t, .15, .18) - .25 * snap(t, .55, .16)
    paw = max(0, S(TAU * ramp(t, .62, .9))) if .62 < t < .9 else 0
    return {'Root': {'loc': (0, 0, .02 * breath)},
            'Body': {'rot': (.03 * breath, 0, .02 * S(a))},
            'Torso': {'rot': (.32 + .07 * breath, 0, .05 * S(a + 1))},
            'Head': {'rot': (.18 - .1 * breath + .12 * snap(t, .4, .05), cock, look)},
            'Petals': {'scale': BUD + .08 * breath + .25 * snap(t, .4, .05) + .03 * S(13 * a)},
            'UpperArmL': {'rot': (-.35 + .06 * breath, 0, .12)}, 'UpperArmR': {'rot': (-.3 + .06 * S(2 * a + .7), 0, -.12)},
            'ForearmL': {'rot': (-.55 - .25 * snap(t, .3, .1), 0, 0)}, 'ForearmR': {'rot': (-.5 - .2 * snap(t, .7, .08), 0, 0)},
            'ThighFR': {'rot': (-.55 * paw, 0, 0)}, 'ShinFR': {'rot': (.9 * paw, 0, 0)},
            'Tail': {'rot': (.15, 0, .5 * S(a) * .5 + .25 * S(3 * a))}}
sampled('Chimera_Idle', 75, idle, step=1)

WOLF = json.load(open(Path(__file__).resolve().parent / 'data' / 'wolf_walk.json'))['frames']
def wolf(t, key):
    """The wolf's swing for `key` at loop phase t, interpolated between frames."""
    f = (t % 1) * len(WOLF); i = int(f) % len(WOLF); w = f - int(f)
    return mix(WOLF[i][key], WOLF[(i + 1) % len(WOLF)][key], w)

def walk(t):
    pose = {}
    # Legs from the wolf: a negative X swings a hanging bone's tip forward, and
    # the hind legs are scaled down to the horse-length legs they now drive.
    for leg, scale in (('FL', 1.0), ('FR', 1.0), ('BL', .8), ('BR', .8)):
        pose['Thigh' + leg] = {'rot': (-scale * wolf(t, 'Thigh' + leg), 0, 0)}
        pose['Shin' + leg] = {'rot': (-scale * wolf(t, 'Shin' + leg), 0, 0)}
    a = TAU * t
    # Weight drops onto each forefoot: a hard double bob and a pitching back.
    fall = abs(S(2 * a + .4)) ** .6
    pose['Root'] = {'loc': (0, 0, .07 * fall - .05)}
    pose['Body'] = {'rot': (.06 * S(2 * a + 1.1), 0, .06 * S(a))}
    # The human torso rides hunched over the forelegs, swaying, clawing ahead
    # of itself in a fast reach and a slow drag.
    pose['Torso'] = {'rot': (.5 + .08 * S(2 * a + .6), .08 * S(a + .3), -.1 * S(a))}
    reachL = ramp(t % 1, 0, .2) * (1 - ramp(t % 1, .2, .75))
    reachR = ramp((t + .5) % 1, 0, .2) * (1 - ramp((t + .5) % 1, .2, .75))
    pose['UpperArmL'] = {'rot': (-.55 - .75 * reachL, 0, .1)}; pose['ForearmL'] = {'rot': (-.7 + .45 * reachL, 0, 0)}
    pose['UpperArmR'] = {'rot': (-.55 - .75 * reachR, 0, -.1)}; pose['ForearmR'] = {'rot': (-.7 + .45 * reachR, 0, 0)}
    # Head low and thrust out, snapping side to side once a stride.
    pose['Head'] = {'rot': (.35 + .1 * S(2 * a + 2.2), .12 * S(a), .2 * (snap(t, .3, .08) - snap(t, .78, .06)))}
    pose['Petals'] = {'scale': BUD + .18 * fall + .06 * S(11 * a)}
    pose['Tail'] = {'rot': (.1 + .1 * S(2 * a), 0, .45 * S(a + .8))}
    return pose
sampled('Chimera_Walk', 38, walk, step=1)

def blend(a, b, w):
    """Pose a toward pose b by w, bone by bone (missing bones are at rest)."""
    out = {}
    for bone in set(a) | set(b):
        pa, pb = a.get(bone, {}), b.get(bone, {})
        q = {}
        for key, rest in (('rot', (0, 0, 0)), ('loc', (0, 0, 0))):
            x, y = pa.get(key, rest), pb.get(key, rest)
            q[key] = tuple(mix(i, j, w) for i, j in zip(x, y))
        q['scale'] = mix(pa.get('scale', BUD if bone == 'Petals' else 1.0), pb.get('scale', BUD if bone == 'Petals' else 1.0), w)
        out[bone] = q
    return out

REST = {'Petals': {'scale': BUD}}
DIP = {'Root': {'loc': (0, 0, -.05)}, 'Body': {'rot': (.08, 0, 0)}, 'Torso': {'rot': (.2, 0, 0)}, 'Head': {'rot': (.3, 0, 0)},
       'UpperArmL': {'rot': (.25, 0, .1)}, 'UpperArmR': {'rot': (.25, 0, -.1)}, 'ThighFL': {'rot': (.1, 0, 0)}, 'ThighFR': {'rot': (.1, 0, 0)},
       'Petals': {'scale': .45}, 'Tail': {'rot': (.2, 0, 0)}}
REAR = {'Root': {'loc': (0, .12, .28)}, 'Body': {'rot': (-.5, 0, 0)},
        'ThighFL': {'rot': (-1.0, 0, 0)}, 'ShinFL': {'rot': (1.3, 0, 0)}, 'ThighFR': {'rot': (-.8, 0, 0)}, 'ShinFR': {'rot': (1.15, 0, 0)},
        'ThighBL': {'rot': (.4, 0, 0)}, 'ShinBL': {'rot': (-.15, 0, 0)}, 'ThighBR': {'rot': (.4, 0, 0)}, 'ShinBR': {'rot': (-.15, 0, 0)},
        'Torso': {'rot': (-.35, 0, 0)}, 'Head': {'rot': (-.4, 0, 0)},
        'UpperArmL': {'rot': (-1.7, 0, .25)}, 'UpperArmR': {'rot': (-1.7, 0, -.25)}, 'ForearmL': {'rot': (-.7, 0, 0)}, 'ForearmR': {'rot': (-.7, 0, 0)},
        'Petals': {'scale': 1.18}, 'Tail': {'rot': (-.5, 0, 0)}}
SLAM = {'Root': {'loc': (0, 0, -.06)}, 'Body': {'rot': (.18, 0, 0)},
        'ThighFL': {'rot': (.3, 0, 0)}, 'ThighFR': {'rot': (.25, 0, 0)}, 'ThighBL': {'rot': (-.1, 0, 0)}, 'ThighBR': {'rot': (-.1, 0, 0)},
        'Torso': {'rot': (.4, 0, 0)}, 'Head': {'rot': (.35, 0, 0)},
        'UpperArmL': {'rot': (-.6, 0, .1)}, 'UpperArmR': {'rot': (-.6, 0, -.1)}, 'ForearmL': {'rot': (-.2, 0, 0)}, 'ForearmR': {'rot': (-.2, 0, 0)},
        'Petals': {'scale': 1.2}, 'Tail': {'rot': (.3, 0, 0)}}
def attack(t):
    pose = blend(REST, DIP, ramp(t, 0, .18))
    pose = blend(pose, REAR, ramp(t, .15, .45))
    pose = blend(pose, SLAM, ramp(t, .45, .58))
    return blend(pose, {'Petals': {'scale': .8}}, ramp(t, .65, 1))
sampled('Chimera_Attack', 36, attack)

def pain(t):
    hit = ramp(t, 0, .12) * (1 - ramp(t, .12, .55)); over = ramp(t, .4, .6) * (1 - ramp(t, .6, 1))
    return {'Body': {'rot': (0, 0, .12 * hit)}, 'Torso': {'rot': (-.4 * hit + .12 * over, 0, .15 * hit)},
            'Head': {'rot': (-.55 * hit + .15 * over, 0, -.35 * hit)},
            'Petals': {'scale': mix(BUD, .3, hit)},
            'UpperArmL': {'rot': (-.6 * hit, 0, .2 * hit)}, 'UpperArmR': {'rot': (-.4 * hit, 0, -.25 * hit)},
            'ForearmL': {'rot': (-.6 * hit, 0, 0)}, 'ForearmR': {'rot': (-.5 * hit, 0, 0)},
            'ThighFL': {'rot': (.2 * hit, 0, 0)}, 'ThighFR': {'rot': (-.15 * hit, 0, 0)},
            'Root': {'loc': (0, .06 * hit, -.03 * hit)}, 'Tail': {'rot': (.3 * hit, 0, 0)}}
sampled('Chimera_Pain', 21, pain, step=1)

def death(t):
    agony = ramp(t, 0, .18) * (1 - ramp(t, .2, .4)); buckle = ramp(t, .25, .5); roll = ramp(t, .45, .78); limp = ramp(t, .7, 1)
    return {
        'Root': {'loc': (0, .1 * buckle, -.35 * buckle - .2 * roll), 'rot': (0, 1.4 * roll, 0)},
        'Body': {'rot': (.35 * buckle - .2 * agony, 0, 0)},
        'ThighFL': {'rot': (1.05 * buckle - .3 * roll, 0, .2 * roll)}, 'ShinFL': {'rot': (-1.7 * buckle + .9 * roll, 0, 0)},
        'ThighFR': {'rot': (1.0 * buckle - .2 * roll, 0, 0)}, 'ShinFR': {'rot': (-1.6 * buckle + .8 * roll, 0, 0)},
        'ThighBL': {'rot': (.25 * buckle + .4 * roll, 0, .25 * roll)}, 'ShinBL': {'rot': (.3 * roll, 0, 0)},
        'ThighBR': {'rot': (.2 * buckle - .3 * roll, 0, 0)}, 'ShinBR': {'rot': (.5 * roll, 0, 0)},
        'Torso': {'rot': (-.45 * agony + .55 * buckle + .2 * limp, 0, .35 * roll)},
        'Head': {'rot': (-.7 * agony + .45 * buckle + .2 * limp, 0, .5 * roll)},
        'Petals': {'scale': mix(BUD, 1.2, agony) * (1 - buckle) + 1.0 * buckle},
        'UpperArmL': {'rot': (-1.4 * agony + .6 * buckle, 0, .5 * roll)}, 'UpperArmR': {'rot': (-1.2 * agony + .4 * buckle, 0, -.3 * roll)},
        'ForearmL': {'rot': (-.8 * agony - .2 * limp, 0, 0)}, 'ForearmR': {'rot': (-.6 * agony - .3 * limp, 0, 0)},
        'Tail': {'rot': (-.4 * agony + .5 * roll, 0, 0)},
    }
sampled('Chimera_Death', 54, death)

ad = rig.animation_data
ad.action = None
for name, (action, first, last) in clips.items():
    track = ad.nla_tracks.new(); track.name = name
    strip = track.strips.new(name, first, action)
    if hasattr(strip, 'action_slot') and action.slots:
        strip.action_slot = action.slots[0]
    strip.action_frame_start, strip.action_frame_end = first, last
for pb in rig.pose.bones:
    pb.rotation_euler, pb.location, pb.scale = (0, 0, 0), (0, 0, 0), (1, 1, 1)

bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={'ARMATURE', 'MESH'},
                         add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
                         bake_anim_use_nla_strips=True, bake_anim_force_startend_keying=True,
                         path_mode='COPY', embed_textures=False)
print('CHIMERA exported', out, 'clips', list(clips))
