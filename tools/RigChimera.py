"""Rig and animate the Chimera, then write its runtime FBX.

  blender -b --python tools/RigChimera.py -- CHIMERA.glb OUT.fbx

The Chimera (O:/retro official/chimera.zip) ships as eleven rigid parts and no
skeleton: a four-legged lower body with its legs in one mesh, a torso with
arms, a petalled flower-mouth head, a tail. This builds a skeleton with three
joints a leg (each leg split out of the lower body below its barrel, at the
knee and again at the hock or wrist; each arm at the elbow), weights every part
rigidly to its bone, and keys five clips at 30 fps for
RawMetalAuthoredCreatureBake.

It moves like a predator, not a horse: it never stands up on straight legs. It
holds low on bent, loaded legs (the hind legs folded at the hock like a big
cat's), shoulders under its haunches, the human torso thrown out over its
forelegs with the arms hanging to claw at the floor, and the flower-mouth
aimed at what it watches. Every foot that is on the ground is solved onto it
(a two-bone solve to the hock plus the pastern at a set angle), so nothing
skates or floats.

  Chimera_Idle    2.5 s loop: crouched and still but for the breathing, the
                  maw held on its prey and jerking to a sound, the tail
                  lashing, the haunches shifting as it loads for a spring
  Chimera_Walk    1.25 s loop: a low stalking prowl, the feet planted for the
                  1.745 m the game moves it each cycle (gaitPerMetre 3.6), the
                  arms reaching ahead and dragging back like a second pair of
                  forelegs, the head held level and steady while the body rolls
  Chimera_Attack  1.2 s: it coils, springs, lands on its prey arms first at
                  .58 of the clip (the game's contact time), bites and worries
                  it, and backs into its crouch
  Chimera_Pain    0.7 s: a recoil back onto its haunches, the petals snapping
                  shut, a hiss forward
  Chimera_Death   1.8 s: it rears in agony, buckles at the front knees and
                  rolls onto its side

Blender axes: Z up, the creature faces -Y. Every bone here turns about X only
in the side view, with one convention: a positive X turn tips an upward bone
(torso, head) forward and swings a hanging bone (legs, arms) back; for the
body, positive drops the nose.
"""
import bpy, sys, math
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

LEG_TOP, KNEE, FOOT, ELBOW = .68, .36, .14, 1.62
LEGS = {'FL': (.22, .30), 'FR': (-.22, .30), 'BL': (.27, 1.35), 'BR': (-.27, 1.35)}
HIP_Z = .75
L1, L2, L3 = HIP_Z - KNEE, KNEE - FOOT, FOOT
PIVOT = (.75, .95)                       # the Body bone's head (y, z): the body pitches about it
BONES = {'Root': ((0, .70, 0), (0, 1.0, 0), None), 'Body': ((0, .75, .95), (0, 1.25, .95), 'Root')}
for leg, (x, y) in LEGS.items():
    BONES['Thigh' + leg] = ((x, y, HIP_Z), (x, y, KNEE), 'Body')
    BONES['Shin' + leg] = ((x, y, KNEE), (x, y, FOOT), 'Thigh' + leg)
    BONES['Foot' + leg] = ((x, y, FOOT), (x, y, 0), 'Shin' + leg)
BONES.update({
    # The torso pivots on the ring where it meets the lower body (z 1.55-1.6),
    # so leaning never lifts its base off the barrel.
    'Torso': ((0, .1, 1.57), (0, .05, 2.0), 'Body'),
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
        return ('Thigh' if p.z >= KNEE else 'Shin' if p.z >= FOOT else 'Foot') + leg
    return 'Body'

WAIST = (1.45, 1.75)
def waist(p):
    """The Torso's share of a vertex at the waist: one function for both meshes,
    so the shared ring where the torso sits on the barrel moves as one."""
    return min(1, max(0, (p.z - WAIST[0]) / (WAIST[1] - WAIST[0])))

for name, o in parts.items():
    groups = {b: o.vertex_groups.new(name=b) for b in BONES}
    for v in o.data.vertices:
        bone = bone_for(name, v.co)
        front = name.startswith('LowerBody') and bone == 'Body' and v.co.y < .45
        if (bone == 'Torso' or front) and waist(v.co) < 1:
            share = waist(v.co)
            if share > 0:
                groups['Torso'].add([v.index], share, 'REPLACE')
            groups['Body'].add([v.index], 1 - share, 'REPLACE')
        else:
            groups[bone].add([v.index], 1.0, 'REPLACE')
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
def sampled(name, frames, pose_at, step=1):
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
def snap(t, at, hold):
    """A twitch: 0 until `at`, snaps to 1 in a frame or two, holds, eases back."""
    return ramp(t, at, at + .03) * (1 - ramp(t, at + hold, at + hold + .12))
S = math.sin; C = math.cos; TAU = 2 * math.pi
BUD = .55

# ---- the legs: planted feet ------------------------------------------------------------------
# Side-view angles: a segment at absolute angle a runs from its top by L*(sin a, -cos a) in (y, z),
# the same sense as a bone's X turn (positive swings a hanging bone's tip back, toward +y).

def hip(leg, root, pitch):
    """Where a leg's hip is once the root has moved by root (dy, dz) and the body pitched."""
    ry, rz = root
    y0 = LEGS[leg][1]
    dy, dz = y0 - PIVOT[0], HIP_Z - PIVOT[1]
    return (PIVOT[0] + dy * C(pitch) - dz * S(pitch) + ry, PIVOT[1] + dy * S(pitch) + dz * C(pitch) + rz)

# The pastern's angle off vertical: the hind feet stand on their toes with the hock behind
# (a cat's), the forefeet on a short wrist, paw a little ahead of it.
PASTERN = {'F': -.18, 'B': -.42}
# Which way the first joint bends: the hind stifle forward, the fore elbow back.
BEND = {'F': -1, 'B': 1}

def solve(leg, root, pitch, foot):
    """Thigh, shin and foot turns that put the leg's toe at foot (y, z)."""
    hy, hz = hip(leg, root, pitch)
    a3 = PASTERN[leg[0]]
    ay, az = foot[0] - L3 * S(a3), foot[1] + L3 * C(a3)            # the hock / wrist
    vy, vz = ay - hy, az - hz
    d = min(max(math.hypot(vy, vz), abs(L1 - L2) + 1e-3), L1 + L2 - 1e-3)
    alpha = math.atan2(vy, -vz)                                     # the hip-to-hock line's angle
    beta = math.acos((L1 * L1 + d * d - L2 * L2) / (2 * L1 * d))
    a1 = alpha - BEND[leg[0]] * beta
    ky, kz = hy + L1 * S(a1), hz - L1 * C(a1)
    a2 = math.atan2(ay - ky, -(az - kz))
    return {'Thigh' + leg: {'rot': (a1 - pitch, 0, 0)}, 'Shin' + leg: {'rot': (a2 - a1, 0, 0)},
            'Foot' + leg: {'rot': (a3 - a2, 0, 0)}}

# The crouch every clip comes from: the barrel dropped, the nose down so the shoulders sit under
# the haunches, the paws set a little wide of the hips front and back.
CROUCH = {'root': (0., -.2), 'pitch': .14, 'torso': .95, 'head': .05, 'yaw': 0., 'roll': 0.}
STANCE = {'FL': -.02, 'FR': .04, 'BL': 1.5, 'BR': 1.44}

def body(p, feet, extra=None):
    """A whole pose from body parameters p and each foot's (y, z), the arms, head, petals and
    tail from p as well; extra overrides bones last."""
    root, pitch, torso = p['root'], p['pitch'], p['torso']
    pose = {'Root': {'loc': (0, root[0], root[1])},
            'Body': {'rot': (pitch, p.get('roll', 0.), p.get('yaw', 0.))},
            'Torso': {'rot': (torso - pitch * .5, p.get('torso_roll', 0.), p.get('torso_yaw', 0.))},
            'Head': {'rot': (p.get('head', 0.), p.get('cock', 0.), p.get('look', 0.))},
            'Petals': {'scale': p.get('petals', BUD)},
            'Tail': {'rot': (p.get('tail_lift', .15), 0, p.get('tail', 0.))}}
    # The arms hang toward the floor ahead of the forefeet: absolute angles, so a torso that
    # pitches does not throw them back behind it.
    lean = pitch + torso - pitch * .5
    for side, reach in (('L', p.get('reachL', 0.)), ('R', p.get('reachR', 0.))):
        spread = p.get('spread', .12) * (1 if side == 'L' else -1)
        upper = p.get('arm', -.35) - .55 * reach
        fore = upper + p.get('elbow', -.45) + .5 * reach
        pose['UpperArm' + side] = {'rot': (upper - lean, 0, spread)}
        pose['Forearm' + side] = {'rot': (fore - upper, 0, 0)}
    for leg, foot in feet.items():
        pose.update(solve(leg, root, pitch, foot))
    for bone, q in (extra or {}).items():
        pose[bone] = q
    return pose

def planted(p, lift=None):
    lift = lift or {}
    return {leg: (STANCE[leg] + lift.get(leg, (0, 0))[0], lift.get(leg, (0, 0))[1]) for leg in LEGS}

def with_(p, **kw):
    q = dict(p); q.update(kw); return q

# ---- Idle: crouched, watching -------------------------------------------------------------
def idle(t):
    a = TAU * t
    breath = S(2 * a) * .5 + .5                                  # two slow heaves a loop
    # The maw stays on its prey; a sound jerks it to a bearing, it holds, it snaps back.
    look = .4 * snap(t, .14, .2) - .5 * snap(t, .55, .18) + .18 * snap(t, .84, .05)
    cock = .28 * snap(t, .16, .16) - .22 * snap(t, .57, .14)
    # Loading for a spring: the haunches rise and shift side to side, a cat's wiggle.
    load = ramp(t, .3, .42) * (1 - ramp(t, .62, .8))
    wiggle = .07 * S(TAU * 3 * t) * load
    p = with_(CROUCH, root=(0, -.2 + .015 * breath - .03 * load), pitch=.14 + .05 * load,
              torso=.95 + .05 * breath, head=.05 - .06 * breath + .1 * snap(t, .4, .05),
              look=look, cock=cock, yaw=wiggle * .6, roll=wiggle,
              petals=BUD + .1 * breath + .3 * snap(t, .4, .05) + .03 * S(13 * a),
              tail=.6 * S(a) + .25 * S(3 * a + .4), tail_lift=.25 + .1 * S(2 * a),
              reachL=.15 * snap(t, .3, .1), reachR=.12 * snap(t, .72, .08), arm=-.3 + .04 * breath)
    return body(p, planted(p))
sampled('Chimera_Idle', 75, idle)

# ---- Walk: a low stalking prowl, feet planted -------------------------------------------------
CYCLE = 2 * math.pi / 3.6          # metres the game moves it per walk cycle (gaitPerMetre 3.6)
DUTY = .7                          # a slow, deliberate gait: each foot is down 70% of the cycle
STRIDE = DUTY * CYCLE              # how far a planted foot slides back under the body per step
# A lateral-sequence walk, the way a cat or a dog stalks: hind left, fore left, hind right, fore right.
PHASE = {'BL': 0., 'FL': .25, 'BR': .5, 'FR': .75}

def foot_on_cycle(leg, t):
    """(dy, dz) of a foot about its stance spot at walk phase t: back along the floor while down
    (the body moving over it), then lifted and carried forward low, the hind feet higher."""
    u = (t - PHASE[leg]) % 1
    if u < DUTY:
        return (-STRIDE * .5 + STRIDE * (u / DUTY), 0.)     # -y is forward: it lands ahead, ends behind
    s = (u - DUTY) / (1 - DUTY)
    w = ease(s)
    return (STRIDE * .5 - STRIDE * w, (.16 if leg[0] == 'B' else .12) * S(math.pi * s))

def walk(t):
    a = TAU * t
    feet = {leg: (STANCE[leg] + foot_on_cycle(leg, t)[0], foot_on_cycle(leg, t)[1]) for leg in LEGS}
    # The body rolls over each forefoot as it takes the weight and sinks a little at each footfall.
    sink = .025 * abs(S(2 * a))
    reachL = ramp((t + .1) % 1, 0, .25) * (1 - ramp((t + .1) % 1, .3, .8))
    reachR = ramp((t + .6) % 1, 0, .25) * (1 - ramp((t + .6) % 1, .3, .8))
    p = with_(CROUCH, root=(0, -.21 - sink), pitch=.15 + .02 * S(2 * a + .8), roll=.05 * S(a + .3),
              yaw=.04 * S(a), torso=1.0 + .04 * S(2 * a), torso_roll=-.06 * S(a + .3),
              # the head stays level and dead ahead while everything under it rolls
              head=.02 - .02 * S(2 * a + .8), cock=-.05 * S(a + .3), look=-.04 * S(a),
              petals=BUD + .1 * abs(S(2 * a + .4)), tail=.35 * S(a + 1.2), tail_lift=.08 + .05 * S(2 * a),
              reachL=reachL, reachR=reachR, arm=-.2, elbow=-.55)
    return body(p, feet)
sampled('Chimera_Walk', 38, walk)

# ---- Attack: coil, spring, land on it, bite and worry ------------------------------------------
def attack(t):
    coil = ramp(t, 0, .3) * (1 - ramp(t, .32, .4))
    air = ramp(t, .32, .46) * (1 - ramp(t, .46, .58))
    land = ramp(t, .5, .58) * (1 - ramp(t, .8, 1))
    back = ramp(t, .78, 1)
    reach = ramp(t, .32, .46)                                       # how far forward it has thrown itself
    fwd = -1.0 * reach * (1 - back)                                 # -y is forward
    worry = (snap(t, .62, .04) - snap(t, .7, .04) + snap(t, .76, .03)) * (1 - back)
    p = with_(CROUCH,
              root=(fwd + .12 * coil, -.2 - .14 * coil + .22 * air - .06 * land),
              pitch=.14 + .12 * coil - .12 * air + .22 * land,
              torso=.95 - .1 * coil - .02 * air + .35 * land,
              head=.05 + .2 * coil - .2 * air + .25 * land + .15 * worry,
              look=.35 * worry, cock=-.2 * worry,
              petals=mix(mix(mix(BUD, .3, coil), 1.45, air), .5, land) if t < .8 else mix(.5, BUD, back),
              tail=.1 * S(30 * t) * coil, tail_lift=.15 + .4 * coil - .2 * air,
              spread=.12 + .55 * air - .3 * land, arm=-.35 + .45 * coil - 1.1 * air + .55 * land,
              elbow=-.45 - .4 * coil + .3 * air - .5 * land)
    # The hind feet stay where it coiled until it leaves them; the forefeet leave the floor in the
    # spring and come down ahead of it on landing.
    feet = {}
    for leg in LEGS:
        y0 = STANCE[leg]
        if leg[0] == 'B':
            push = ramp(t, .34, .46)                                 # the hind feet trail as it leaves
            feet[leg] = (y0 + fwd * (1 - push) + .1 * coil, .18 * air)
        else:
            feet[leg] = (y0 + fwd - .18 * air, .45 * air)
    return body(p, feet)
sampled('Chimera_Attack', 36, attack)

# ---- Pain: recoil onto the haunches, a hiss ---------------------------------------------------
def pain(t):
    hit = ramp(t, 0, .12) * (1 - ramp(t, .12, .55)); over = ramp(t, .4, .6) * (1 - ramp(t, .6, 1))
    p = with_(CROUCH, root=(.12 * hit - .04 * over, -.2 - .06 * hit), pitch=.14 - .22 * hit + .08 * over,
              torso=.95 - .5 * hit + .15 * over, head=.05 - .5 * hit + .2 * over, look=-.3 * hit, cock=.2 * hit,
              petals=mix(BUD, .25, hit) + .4 * over, roll=.1 * hit, arm=-.35 - .5 * hit, elbow=-.45 - .5 * hit,
              tail_lift=.15 + .3 * hit)
    return body(p, planted(p))
sampled('Chimera_Pain', 21, pain)

# ---- Death: from the crouch, a rear in agony, the forelegs buckle, it rolls -------------------
def death(t):
    agony = ramp(t, 0, .18) * (1 - ramp(t, .2, .4)); buckle = ramp(t, .25, .5); roll = ramp(t, .45, .78); limp = ramp(t, .7, 1)
    p = with_(CROUCH, root=(.05 * buckle, -.2 + .15 * agony - .25 * buckle - .15 * roll),
              pitch=.14 - .45 * agony + .4 * buckle, torso=.95 - .7 * agony + .1 * buckle + .2 * limp,
              head=.05 - .7 * agony + .3 * buckle + .2 * limp, petals=mix(BUD, 1.25, agony) * (1 - buckle) + 1.0 * buckle,
              arm=-.35 - 1.2 * agony + .6 * buckle, elbow=-.45 - .5 * agony - .3 * limp, tail_lift=.15 - .5 * agony + .4 * roll)
    feet = {leg: (STANCE[leg] - (.25 * agony if leg[0] == 'F' else 0), (.35 * agony if leg[0] == 'F' else 0)) for leg in LEGS}
    pose = body(p, feet)
    # Once it goes over, the legs give up the floor: fold them and let them fall with the roll.
    for leg in LEGS:
        fold = buckle if leg[0] == 'F' else roll
        for bone, k in (('Thigh', .6), ('Shin', -1.0), ('Foot', .5)):
            q = pose[bone + leg]['rot']
            pose[bone + leg] = {'rot': (mix(q[0], q[0] + k, fold), 0, .25 * roll * (1 if leg.endswith('L') else -1))}
    pose['Root']['rot'] = (0, 1.4 * roll, 0)
    pose['Body']['rot'] = (pose['Body']['rot'][0], 0, 0)
    pose['Torso']['rot'] = (pose['Torso']['rot'][0], 0, .35 * roll)
    pose['Head']['rot'] = (pose['Head']['rot'][0], 0, .5 * roll)
    return pose
sampled('Chimera_Death', 54, death, step=2)

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

if '--preview' in sys.argv:
    # side-view stills of every clip for checking poses before a bake: OUT is a folder
    import os
    os.makedirs(out, exist_ok=True)
    cam = bpy.data.objects.new('cam', bpy.data.cameras.new('cam')); bpy.context.scene.collection.objects.link(cam)
    cam.data.type = 'ORTHO'; cam.data.ortho_scale = 4.6
    cam.location = (6, .2, 1.3); cam.rotation_euler = (math.radians(90), 0, math.radians(90))
    sc = bpy.context.scene; sc.camera = cam
    sc.render.engine = 'BLENDER_WORKBENCH'; sc.display.shading.color_type = 'TEXTURE'; sc.display.shading.light = 'STUDIO'
    sc.render.resolution_x, sc.render.resolution_y = 300, 300
    floor = bpy.data.meshes.new('floor'); floor.from_pydata([(-2, -3, 0), (2, -3, 0), (2, 4, 0), (-2, 4, 0)], [], [(0, 1, 2, 3)])
    bpy.context.scene.collection.objects.link(bpy.data.objects.new('floor', floor))
    for track in ad.nla_tracks:
        track.mute = True
    for name, (action, first, last) in clips.items():
        rig.animation_data.action = action
        for i, q in enumerate((0, .25, .45, .58, .75)):
            sc.frame_set(int(first + q * (last - first)))
            sc.render.filepath = os.path.join(out, '%s_%d.png' % (name, i))
            bpy.ops.render.render(write_still=True)
    print('CHIMERA preview', out)
else:
    bpy.ops.export_scene.fbx(filepath=out, use_selection=False, object_types={'ARMATURE', 'MESH'},
                             add_leaf_bones=False, bake_anim=True, bake_anim_use_all_actions=False,
                             bake_anim_use_nla_strips=True, bake_anim_force_startend_keying=True,
                             path_mode='COPY', embed_textures=False)
    print('CHIMERA exported', out, 'clips', list(clips))
