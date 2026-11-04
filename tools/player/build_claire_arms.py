# Claire's first-person arms: her own arms, sliced from Claire.fbx and fitted
# onto the viewmodel rig (arms_rig.fbx) so every viewmodel action, grip IK and
# VR hand drives them unchanged.
#   blender -b --python tools/player/build_claire_arms.py -- arms_rig.fbx Claire.fbx OUT.fbx
# Each of her arm bones maps onto its rig counterpart by a similarity
# transform (head onto head, tail onto tail, the hand's index-to-pinky line
# keeping the twist); her vertices move by those transforms blended by her
# own skin weights, then take the rig's bone names as their groups.
import bpy,sys
from mathutils import Matrix,Vector
rig_path,claire_path,out_path=sys.argv[sys.argv.index('--')+1:][:3]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=rig_path)
rig=bpy.data.objects['ArmsRig'];old=bpy.data.objects['ArmsMesh']
before=set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=claire_path)
claire_objects=[o for o in bpy.data.objects if o not in before]
skeleton=next(o for o in claire_objects if o.type=='ARMATURE')
# Her bone -> the rig's bone; finger joints 1-3 follow the same pattern.
mapping={}
for side,s in (('R','R'),('L','L')):
    mapping['Arm_'+side]='upper_arm.'+s;mapping['ForeArm_'+side]='forearm.'+s;mapping['Hand_'+side]='hand.'+s
    for finger,target in (('Thumb','thumb.0'),('Index','f_index.0'),('Middle','f_middle.0'),('Ring','f_ring.0'),('Pinky','f_pinky.0')):
        for j in (1,2,3):mapping[f'{finger}{j}_{side}']=f'{target}{j}.{s}'
arm_chain=set(mapping)
cw=skeleton.matrix_world
def ch(name):b=skeleton.data.bones[name];return cw@b.head_local,cw@b.tail_local
def rg(name):b=rig.data.bones[name];return rig.matrix_world@b.head_local,rig.matrix_world@b.tail_local
def frame(direction,side):
    x=direction.normalized();y=(side-x*side.dot(x)).normalized();return Matrix((x,y,x.cross(y))).transposed()
transforms={}
reach=lambda f,a,b:(f(b)[0]-f(a)[0]).length
for side in ('R','L'):
    arm=(rg(f'hand.{side}')[0]-rg(f'upper_arm.{side}')[0]).length/((ch(f'Hand_{side}')[0]-ch(f'Arm_{side}')[0]).length)
    palm=(rg(f'f_pinky.01.{side}')[0]-rg(f'f_index.01.{side}')[0]).length/((ch(f'Pinky1_{side}')[0]-ch(f'Index1_{side}')[0]).length)
    # Twist reference: index knuckle to pinky knuckle, on both rigs.
    cs=ch(f'Pinky1_{side}')[0]-ch(f'Index1_{side}')[0];rs=rg(f'f_pinky.01.{side}')[0]-rg(f'f_index.01.{side}')[0]
    for her,its in mapping.items():
        if not her.endswith(side):continue
        (ch0,ch1),(rg0,rg1)=ch(her),rg(its)
        if her.startswith('Hand'):ch1=ch(f'Middle1_{side}')[0];rg1=rg(f'f_middle.01.{side}')[0]
        across=arm if her.startswith(('Arm','ForeArm')) else palm
        fc,fr=frame(ch1-ch0,cs),frame(rg1-rg0,rs);along=(rg1-rg0).length/max(1e-6,(ch1-ch0).length)
        transforms[her]=(ch0,fr@Matrix.Diagonal((along,across,across))@fc.transposed(),rg0)
# Join her meshes, keep faces whose every corner belongs to an arm.
meshes=[o for o in claire_objects if o.type=='MESH']
for o in meshes:
    for m in o.modifiers:o.modifiers.remove(m)
bpy.ops.object.select_all(action='DESELECT')
for o in meshes:o.select_set(True)
bpy.context.view_layer.objects.active=meshes[0];bpy.ops.object.join()
arms=bpy.context.view_layer.objects.active;arms.parent=None
arms.data.transform(arms.matrix_world);arms.matrix_world=Matrix()
names={g.index:g.name for g in arms.vertex_groups}
def weights(v):return {names[g.group]:g.weight for g in v.groups if g.weight>0}
keep=set()
for v in arms.data.vertices:
    w=weights(v);total=sum(w.values()) or 1
    if sum(x for b,x in w.items() if b in arm_chain)/total>.5:keep.add(v.index)
import bmesh
bm=bmesh.new();bm.from_mesh(arms.data)
# Torso clothing (vest straps, jacket) stays with the torso.
torso={i for i,m in enumerate(arms.data.materials) if m.name in ('VEST','JACKET')}
bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.material_index in torso or not all(v.index in keep for v in f.verts)],context='FACES')
bmesh.ops.delete(bm,geom=[v for v in bm.verts if not v.link_faces],context='VERTS')
shoulders=[ch(f'Arm_{side}')[0] for side in ('R','L')]
loops=[];seen=set()
for e in bm.edges:
    if e.is_boundary and e not in seen:
        loop=[];stack=[e]
        while stack:
            x=stack.pop()
            if x in seen:continue
            seen.add(x);loop.append(x);stack+=[y for v in x.verts for y in v.link_edges if y.is_boundary and y not in seen]
        loops.append(loop)
def centre(loop):vs={v for e in loop for v in e.verts};return sum((v.co for v in vs),Vector())/len(vs)
keep_open=set()
for sh in shoulders:keep_open.add(min(range(len(loops)),key=lambda i:(centre(loops[i])-sh).length))
for i,loop in enumerate(loops):
    if i not in keep_open:bmesh.ops.holes_fill(bm,edges=loop,sides=0)
print('OPENINGS',len(loops),'kept',len(keep_open))
bm.to_mesh(arms.data);bm.free()
# Move each vertex by her bones' transforms, blended by her weights.
for v in arms.data.vertices:
    w={b:x for b,x in weights(v).items() if b in transforms};total=sum(w.values())
    if total<=0:raise SystemExit(f'vertex {v.index} has no arm weight')
    p=Vector((0,0,0))
    for b,x in w.items():c0,m,r0=transforms[b];p+=(m@(v.co-c0)+r0)*(x/total)
    v.co=p
# Her groups become the rig's, unmapped influences dropped.
data=[{mapping[b]:x for b,x in weights(v).items() if b in mapping} for v in arms.data.vertices]
# Her wrist is hard-weighted: one ring of triangles took every turn of the hand
# against the forearm and twisted thin like a balloon. Spread the hand's
# influence over the forearm's last rings (as the original arms were weighted)
# and give the wrist back some girth: hers is a third slimmer than the rig's.
def smooth(t,a,b):x=min(1,max(0,(t-a)/(b-a)));return x*x*(3-2*x)
for side in ('R','L'):
    e=rg(f'forearm.{side}')[0];w=rg(f'hand.{side}')[0];axis=w-e;L=axis.length;axis.normalize()
    fa,hd=f'forearm.{side}',f'hand.{side}'
    for v,wt in zip(arms.data.vertices,data):
        share=wt.get(fa,0)+wt.get(hd,0)
        if share<=0:continue                 # fingers and upper arm keep theirs
        d=v.co-e;t=d.dot(axis)/L
        if t<.4 or t>1.15:continue
        h=smooth(t,.45,1.06);wt.pop(fa,None);wt.pop(hd,None)
        if h<1:wt[fa]=share*(1-h)
        if h>0:wt[hd]=share*h
        radial=d-axis*d.dot(axis);bulge=1+.28*max(0,1-abs(t-.95)/.22)
        v.co=e+axis*d.dot(axis)+radial*bulge
for g in list(arms.vertex_groups):arms.vertex_groups.remove(g)
for v,w in zip(arms.data.vertices,data):
    total=sum(w.values())
    for b,x in w.items():
        g=arms.vertex_groups.get(b) or arms.vertex_groups.new(name=b);g.add([v.index],x/total,'REPLACE')
# Her ten material slots stay as authored (her arm skin is BOOTS); the
# renderer picks her textures by these names.
from collections import Counter
print('CLAIRE ARMS',len(arms.data.vertices),'vertices',len(arms.data.polygons),'faces',dict(Counter(arms.data.materials[p.material_index].name for p in arms.data.polygons)))
# Replace the old arms on the rig.
bpy.data.objects.remove(old,do_unlink=True)
for o in list(bpy.data.objects):
    if o!=rig and o!=arms:bpy.data.objects.remove(o,do_unlink=True)
arms.name='ArmsMesh';arms.data.name='ArmsMesh';arms.parent=rig
mod=arms.modifiers.new('Armature','ARMATURE');mod.object=rig
# The file's default pose is what the viewmodel starts from and frames by:
# the original's is the first frame of its relax action.
relax=next(x for x in bpy.data.actions if x.name.endswith('relax'));rig.animation_data.action=relax
if relax.slots:rig.animation_data.action_slot=relax.slots[0]
bpy.context.scene.frame_set(int(relax.frame_range[0]))
bpy.ops.object.select_all(action='DESELECT');rig.select_set(True);arms.select_set(True)
bpy.ops.export_scene.fbx(filepath=out_path,use_selection=True,add_leaf_bones=False,bake_anim=True,bake_anim_use_all_actions=True,bake_anim_use_nla_strips=False,bake_anim_force_startend_keying=True,bake_anim_simplify_factor=0,path_mode='STRIP')
