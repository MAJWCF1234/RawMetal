"""Blender -b --python tools/BakeMedicTreatment.py

Retarget Quaternius UAL2 Chest_Open onto the supplied unrigged Medic, then
append a seventh compact vertex clip without altering the six existing clips.
"""
import bpy, math, struct, shutil, json
from pathlib import Path
from mathutils import Vector, Quaternion
root=Path(__file__).resolve().parents[1]
source=root/'source-assets/medic-motion'
source.mkdir(parents=True,exist_ok=True)
donor=source/'UAL2_Standard.fbx'
if not donor.exists():
    shutil.copyfile(Path(r'O:\retro official\Animations\Universal Animation Library 2[Standard]\Unity\UAL2_Standard.fbx'),donor)
bpy.ops.import_scene.fbx(filepath=str(donor))
rig=next(o for o in bpy.data.objects if o.type=='ARMATURE')
action=next(a for a in bpy.data.actions if a.name.endswith('|Chest_Open'))
rig.animation_data.action=action
if action.slots:rig.animation_data.action_slot=action.slots[0]
def engine(p):return Vector((p.x,p.z,-p.y))
def joint(name,posed=False):
    return engine((rig.pose.bones[name].head if posed else rig.data.bones[name].head_local))
def direction(name,posed=False):
    b=rig.pose.bones[name] if posed else rig.data.bones[name]
    return engine((b.tail-b.head) if posed else (b.tail_local-b.head_local)).normalized()
path=root/'src/assets/actors/cleanup/medic-animation.bin'
data=path.read_bytes()
corners,samples,clips,count=struct.unpack_from('<4I',data,4)
assert clips in (6,7)
offset=20+corners*2
base=data[:offset+6*samples*count*6]
points=[Vector(tuple(v*.001 for v in struct.unpack_from('<3h',data,offset+i*6))) for i in range(count)]
indices=struct.unpack_from(f'<{corners}H',data,20)
calibration=(root/'src/assets/actors/cleanup/medic-arms.bin').read_bytes()
joints=struct.unpack_from('<18f',calibration,4)
weights=[None]*count
for corner,index in enumerate(indices):weights[index]=tuple(v/255 for v in calibration[104+corner*4:107+corner*4])
identity=Quaternion((1,0,0,0))
pelvis=Vector((0,.78,0));chest=Vector((0,1.1,0))
restPelvis=joint('pelvis')
frames=[];anchors=[]
for sample in range(samples):
    phase=sample/(samples-1)
    start,end=action.frame_range
    frame=start+(end-start)*phase
    bpy.context.scene.frame_set(int(frame),subframe=frame-int(frame))
    blend=min(1,phase/.18,(1-phase)/.14)
    blend=max(0,blend);blend=blend*blend*(3-2*blend)
    rootDelta=(joint('pelvis',True)-restPelvis)*.9
    torsoRotation=direction('spine_02').rotation_difference(direction('spine_02',True))
    rootRotation=direction('pelvis').rotation_difference(direction('pelvis',True))
    def torso(p):return pelvis+rootDelta+torsoRotation@(p-pelvis)
    arms=[];legs=[]
    for side,suffix in enumerate(('r','l')):
        S,E,H=(Vector(joints[side*9+i:side*9+i+3]) for i in (0,3,6))
        targetS=torso(S)
        upper=(E-S).normalized().rotation_difference(direction('upperarm_'+suffix,True))
        lower=(H-E).normalized().rotation_difference(direction('lowerarm_'+suffix,True))
        targetE=targetS+upper@(E-S);targetH=targetE+lower@(H-E)
        arms.append((S,E,H,targetS,targetE,targetH,upper,lower))
        sign=-1 if side==0 else 1
        hip=Vector((sign*.12,.78,0));knee=Vector((sign*.12,.40,0));ankle=Vector((sign*.12,.08,0))
        targetHip=pelvis+rootDelta+rootRotation@(hip-pelvis)
        thigh=(knee-hip).normalized().rotation_difference(direction('thigh_'+suffix,True))
        calf=(ankle-knee).normalized().rotation_difference(direction('calf_'+suffix,True))
        targetKnee=targetHip+thigh@(knee-hip);targetAnkle=targetKnee+calf@(ankle-knee)
        foot=direction('foot_'+suffix).rotation_difference(direction('foot_'+suffix,True))
        legs.append((hip,knee,ankle,targetHip,targetKnee,targetAnkle,thigh,calf,foot))
    posed=[]
    for index,p in enumerate(points):
        side=0 if p.x<0 else 1
        target=torso(p)
        if p.y<.79:
            hip,knee,ankle,H,K,A,thigh,calf,foot=legs[side]
            upper=H+thigh@(p-hip);lower=K+calf@(p-knee)
            bend=max(0,min(1,(.46-p.y)/.12));target=upper.lerp(lower,bend)
            if p.y<.13:target=target.lerp(A+foot@(p-ankle),max(0,min(1,(.13-p.y)/.06)))
        influence,lowerWeight,palmWeight=weights[index]
        if influence>0:
            S,E,H,tS,tE,tH,upper,lower=arms[side]
            arm=(tS+upper@(p-S)).lerp(tE+lower@(p-E),lowerWeight)
            arm=arm.lerp(tH+lower@(p-H),palmWeight)
            target=target.lerp(arm,influence)
        posed.append(p.lerp(target,blend))
    ground=min(p.y for p in posed)
    frames.append([Vector((p.x,p.y-ground,p.z)) for p in posed])
    anchors.append([Vector((p.x,p.y-ground,p.z)) for p in (arms[0][2].lerp(arms[0][5],blend),arms[1][2].lerp(arms[1][5],blend))])
result=bytearray(base);struct.pack_into('<I',result,12,7)
for frame in frames:
    for p in frame:result.extend(struct.pack('<3h',*(round(v*1000) for v in p)))
path.write_bytes(result)
anchorData=bytearray()
for clip in range(7):
    for sample in range(samples):
        for p in anchors[sample] if clip==6 else (Vector(joints[6:9]),Vector(joints[15:18])):
            anchorData.extend(struct.pack('<3f',*p))
(root/'src/assets/actors/cleanup/medic-treatment-anchors.bin').write_bytes(anchorData)
evidence={'source':str(donor.relative_to(root)),'action':action.name,'frames':list(action.frame_range),'samples':samples,'vertices':count,'runtime_clip':6,'method':'anatomical limb retarget with existing calibrated sleeve/palm weights, source joint rotations and grounded vertex bake'}
(root/'diagnostics/medic-treatment-bake.json').write_text(json.dumps(evidence,indent=2))
print(json.dumps(evidence))
