# Leon's player body (docs/Systems/player-body.md): a stylised white cat avatar
# (CC BY 4.0, see docs/Assets/THIRD_PARTY.md) made into a Claire-convention body.
#   blender -b --python tools/player/build_leon.py -- SOURCE.fbx src/assets/library/player/Leon.fbx TEXTURE_DIR [MAX_TRIANGLES]
# SOURCE.fbx is the avatar's file (three meshes: body and head, stockings, bow;
# a 133-bone avatar rig, already in a T-pose, facing -Y). This script
#  - bakes the drawn pose into the meshes and applies every scale and turn,
#  - names the bones Claire's way (Hips, Spine1-3, Neck, Head, Shoulder/Arm/
#    ForeArm/Hand, LegUp/LegLow/Foot/Toe, five fingers a hand) and drops every
#    other bone (ears, tail, hair, eyes, jaw, twist and helper bones), their
#    weights going to the nearest kept ancestor: the tail and ears follow the
#    hips and the head. The rig has one chest bone: it is split into Spine1-3
#    (the weights blend with height),
#  - joins the meshes and decimates them to MAX_TRIANGLES (6,000 by default),
#    keeping triangles for the head (eyes, nose, ears) and the bow,
#  - keeps at most four weights a vertex, names the materials LeonFur, LeonInner,
#    LeonBlack, LeonMetal and writes one 8x8 solid-colour PNG for each
#    (the engine finds a body's texture by material name),
#  - writes the FBX with +Z ahead and +X his left, its arrays undeflated.
import bpy, bmesh, sys, os, zlib, struct
argv=sys.argv[sys.argv.index('--')+1:]
src,out,texdir=argv[0],argv[1],argv[2]
LIMIT=int(argv[3]) if len(argv)>3 else 6000
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
view=bpy.context.view_layer
arm=[o for o in bpy.data.objects if o.type=='ARMATURE'][0]
meshes=[o for o in bpy.data.objects if o.type=='MESH']
for o in list(bpy.data.objects):
    if o not in meshes and o is not arm: bpy.data.objects.remove(o,do_unlink=True)
def select(*objs):
    bpy.ops.object.select_all(action='DESELECT')
    for o in objs: o.select_set(True)
    view.objects.active=objs[0]
# 1. Bake the drawn pose into each mesh, then make it the armature's rest.
for me in meshes:
    select(me)
    bpy.ops.object.modifier_copy(modifier=me.modifiers[0].name)
    bpy.ops.object.modifier_apply(modifier=me.modifiers[0].name)
    bpy.ops.object.parent_clear(type='CLEAR_KEEP_TRANSFORM')
    bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
select(arm)
bpy.ops.object.mode_set(mode='POSE'); bpy.ops.pose.select_all(action='SELECT'); bpy.ops.pose.armature_apply(selected=False); bpy.ops.object.mode_set(mode='OBJECT')
if arm.animation_data: arm.animation_data.action=None
for a in list(bpy.data.actions): bpy.data.actions.remove(a)
select(arm); bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
for pb in arm.pose.bones: pb.matrix_basis.identity()
# 1b. The body's skin under the stockings is never seen, and a coarser skin would poke through them:
# every body face whose outward ray meets the stockings in a hand's breadth goes.
from mathutils.bvhtree import BVHTree
body=next(m for m in meshes if m.name=='boykisser'); socks=next(m for m in meshes if m.name=='Stockings')
sb=bmesh.new(); sb.from_mesh(socks.data); sb.transform(socks.matrix_world); tree=BVHTree.FromBMesh(sb)
bb=bmesh.new(); bb.from_mesh(body.data); bb.transform(body.matrix_world); bb.faces.ensure_lookup_table()
covered=[]
for f in bb.faces:
    c=f.calc_center_median(); n=f.normal
    hit=tree.ray_cast(c+n*1e-4,n,0.12)
    if hit[0] is not None: covered.append(f)
print("COVERED",len(covered),"of",len(bb.faces))
bmesh.ops.delete(bb,geom=covered,context='FACES')
bb.transform(body.matrix_world.inverted()); bb.to_mesh(body.data); bb.free(); sb.free()
body.data.update()
# 2. The bone map: source name -> Claire's. Everything else merges into its nearest kept ancestor.
keep={'Hips':'Hips','spine':'Hips','Neck':'Neck','Head':'Head','Chest':'Spine1'}
for s,c in (('L','L'),('R','R')):
    keep.update({'Shoulder_'+s:'Shoulder_'+c,'Arm_'+s:'Arm_'+c,'Elbow_'+s:'ForeArm_'+c,'Wrist_'+s:'Hand_'+c,
                 'Leg_'+s:'LegUp_'+c,'Knee_'+s:'LegLow_'+c,'Foot_'+s:'Foot_'+c})
    for f,n in (('Thumb','Thumb'),('Index','Index'),('Middle','Middle'),('Ring','Ring'),('Little','Pinky')):
        keep.update({f+'_'+s:n+'1_'+c,f+'_'+s+'.001':n+'2_'+c,f+'_'+s+'.002':n+'3_'+c})
# Weightless leaf bones kept for the bake: the toe (the foot's tip) and each finger's end.
leaf={'Foot_L_end':'Toe_L','Foot_R_end':'Toe_R'}
for s in 'LR':
    for f,n in (('Thumb','Thumb'),('Index','Index'),('Middle','Middle'),('Ring','Ring'),('Little','Pinky')): leaf[f+'_'+s+'.002_end']=n+'3_'+s+'_end'
bones=arm.data.bones
def owner(b):
    while b is not None:
        if b.name in keep: return keep[b.name]
        b=b.parent
    return 'Hips'
owners={b.name:owner(b) for b in bones}
chest=bones['Chest']; z_s1=chest.head_local.z; z_s3=bones['Shoulder_L'].head_local.z; z_s2=(z_s1+z_s3)/2
# The paws' finger weights sit on the tip joints and the rig has no weights on its shoulder (clavicle) bones, which
# ClaireOutside needs (a finger's three joints, a shoulder ring): the weights of each finger are spread over its
# three joints by their place along the finger, and a share of the chest and upper arm near the shoulder goes
# to the shoulder bone.
fingers=[]
for s in 'LR':
    for f,n in (('Thumb','Thumb'),('Index','Index'),('Middle','Middle'),('Ring','Ring'),('Little','Pinky')):
        hs=[bones[f+'_'+s+x].head_local.copy() for x in ('','.001','.002','.002_end')]
        d=(hs[3]-hs[0]).normalized()
        srcs={f+'_'+s+x for x in ('','.001','.002')}
        xs=[]
        for me in meshes:
            gn={g.index:g.name for g in me.vertex_groups}
            for v in me.data.vertices:
                if sum(g.weight for g in v.groups if gn[g.group] in srcs)>0.2:
                    xs.append((v.co-hs[0]).dot(d))
        # the paw's finger geometry lies out near the tips: the joints sit at the 30th, 50th and 75th percentile of its extent
        xs.sort(); q=lambda t: xs[int(t*(len(xs)-1))]
        fingers.append(([n+str(i+1)+'_'+s for i in range(3)],hs[0],d,[q(.3),q(.5),q(.75)]))
shoulder_x={'L':bones['Shoulder_L'].head_local.x,'R':bones['Shoulder_R'].head_local.x}
def spread(w,co):
    for names,h0,d,at in fingers:
        total=sum(w.pop(n,0) for n in names)
        if total<=0: continue
        x=(co-h0).dot(d); parts={}
        if x<=at[0]: parts[names[0]]=1
        elif x>=at[2]: parts[names[2]]=1
        else:
            i=0 if x<at[1] else 1; t=(x-at[i])/(at[i+1]-at[i]); parts[names[i]]=1-t; parts[names[i+1]]=t
        for k,v in parts.items(): w[k]=w.get(k,0)+total*v
def shoulder(w,co):
    side='L' if co.x>0 else 'R'; ax=abs(co.x); c=abs(shoulder_x[side])+0.03
    bump=max(0.0,1-abs(ax-c)/0.06)*0.8
    if bump<=0: return
    take=0
    for n in ('Spine3','Arm_'+side):
        if n in w: t=w[n]*bump; w[n]-=t; take+=t
    if take>0: w['Shoulder_'+side]=w.get('Shoulder_'+side,0)+take
# 3. Merge the weights (per mesh), splitting the chest bone across Spine1-3 by height.
for me in meshes:
    names={g.index:g.name for g in me.vertex_groups}
    rows=[]
    for v in me.data.vertices:
        w={}
        for g in v.groups:
            n=owners[names[g.group]]
            if n=='Spine1':
                z=v.co.z
                if z<=z_s1: parts={'Spine1':1}
                elif z>=z_s3: parts={'Spine3':1}
                elif z<z_s2: t=(z-z_s1)/(z_s2-z_s1); parts={'Spine1':1-t,'Spine2':t}
                else: t=(z-z_s2)/(z_s3-z_s2); parts={'Spine2':1-t,'Spine3':t}
                for k,x in parts.items(): w[k]=w.get(k,0)+g.weight*x
            else: w[n]=w.get(n,0)+g.weight
        spread(w,v.co); shoulder(w,v.co)
        rows.append(w)
    for g in list(me.vertex_groups): me.vertex_groups.remove(g)
    made={}
    for i,w in enumerate(rows):
        for n,x in w.items():
            if x<=1e-5: continue
            if n not in made: made[n]=me.vertex_groups.new(name=n)
            made[n].add([i],x,'REPLACE')
    if not rows or not any(rows): print("no weights on",me.name)
# 4. Join, then decimate.
select(*meshes); view.objects.active=[m for m in meshes if m.name=='boykisser'][0]
bpy.ops.object.join()
me=view.objects.active; me.name='Leon'; me.data.name='Leon'
for m in list(me.modifiers): me.modifiers.remove(m)
def tris(): return sum(len(p.vertices)-2 for p in me.data.polygons)
print("JOINED",tris())
# Head vertices (above the neck bone) keep more; the bow sits on the neck.
neck_z=bones['Neck'].head_local.z
vg=me.vertex_groups.new(name='keep'); hold=me.vertex_groups.new(name='hold')
BAND=(float(os.environ.get('LEON_BAND_LO','0.55')),float(os.environ.get('LEON_BAND_HI','0.9')))   # the thighs' tops, where the stockings end
for v in me.data.vertices:
    head=v.co.z>=neck_z-0.02
    vg.add([v.index],1.0 if head else 0.0,'REPLACE')
    hold.add([v.index],1.0 if head or BAND[0]<=v.co.z<=BAND[1] else 0.0,'REPLACE')
head_tris=lambda: sum(len(p.vertices)-2 for p in me.data.polygons if all(me.data.vertices[i].co.z>=neck_z-0.02 for i in p.vertices))
# Two passes with the weights of 'keep' (1 on the head): the body is cut while the head is held, then the head
# while the body is held. A decimate ratio is of the whole mesh's triangles.
HEAD=int(os.environ.get('LEON_HEAD','3400')); BODY=LIMIT-HEAD
def cut(invert,target,group):
    mod=me.modifiers.new('d','DECIMATE');mod.decimate_type='COLLAPSE';mod.use_collapse_triangulate=True
    mod.vertex_group=group;mod.invert_vertex_group=invert
    mod.ratio=min(1.0,target/tris())
    bpy.ops.object.modifier_apply(modifier=mod.name)
h0=head_tris(); print('HEAD before',h0,'of',tris())
cut(True,h0+BODY+int(os.environ.get('LEON_BANDTRIS','0')),'hold')
print('BODY cut',tris(),'head',head_tris())
cut(False,tris()-head_tris()+HEAD,'keep')
print("DECIMATED",tris(),"head",head_tris())
me.vertex_groups.remove(me.vertex_groups['keep']); me.vertex_groups.remove(me.vertex_groups['hold'])
# 5. Four weights a vertex, normalized.
bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL',limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL',lock_active=False)
# 6. Restructure the skeleton: Hips is the root, the chest bone becomes Spine1 and gains Spine2-3.
select(arm)
bpy.ops.object.mode_set(mode='EDIT')
eb=arm.data.edit_bones
for n in list(eb.keys()):
    if n not in keep and n not in leaf: eb.remove(eb[n])   # 'spine' and 'Hips' are both in keep; spine goes below
hips=eb['Hips']
for n in ('Leg_L','Leg_R'): eb[n].parent=hips
eb['spine'].use_connect=False
hips.parent=None
eb.remove(eb['spine'])
s1=eb['Chest']; head_n=eb['Neck'].head.copy(); sh=eb['Shoulder_L'].head.copy()
zs=[z_s1,z_s2,z_s3]
def at(z,src):
    p=src.head.copy(); p.z=z; return p
s2=eb.new('Spine2'); s3=eb.new('Spine3')
base=s1.head.copy()
s2.head=at(z_s2,s1); s3.head=at(z_s3,s1)
s2.head.x=s3.head.x=base.x
s1.tail=s2.head; s2.tail=s3.head; s3.tail=eb['Neck'].head
s2.parent=s1; s3.parent=s2
for n in ('Neck','Shoulder_L','Shoulder_R'): eb[n].parent=s3
for n in ('Neck','Shoulder_L','Shoulder_R'): eb[n].use_connect=False
for b in eb:
    b.roll=b.roll
bpy.ops.object.mode_set(mode='OBJECT')
for b in list(arm.data.bones):
    n=keep.get(b.name) or leaf.get(b.name)
    if n and b.name!=n: b.name=n   # vertex groups were already renamed to the new names
# Chest is now Spine1; the 'Hips' bone name stays. (names in keep that map to themselves need nothing)
arm.name='Armature'; arm.data.name='Armature'
select(me); me.parent=arm
for m in list(me.modifiers):
    if m.type=='ARMATURE': me.modifiers.remove(m)
am=me.modifiers.new('Armature','ARMATURE'); am.object=arm
# 7. Materials: rename, one 8x8 PNG each from the base colour.
def srgb(c):
    c=max(0.0,min(1.0,c)); return c*12.92 if c<=0.0031308 else 1.055*(c**(1/2.4))-0.055
def png(path,rgb):
    raw=b''.join(b'\x00'+bytes(rgb)*8 for _ in range(8))
    def chunk(t,d): c=struct.pack('>I',len(d))+t+d; return c+struct.pack('>I',zlib.crc32(t+d)&0xffffffff)
    open(path,'wb').write(b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',8,8,8,2,0,0,0))+chunk(b'IDAT',zlib.compress(raw,9))+chunk(b'IEND',b''))
names={'Material':'LeonFur','fur 2':'LeonInner','Glossy Black Plastic':'LeonBlack','Scifi Metal Plate':'LeonMetal'}
used={p.material_index for p in me.data.polygons}
for i,m in enumerate(me.data.materials):
    if m.name not in names: raise SystemExit('unexpected material '+m.name)
    bsdf=next(n for n in m.node_tree.nodes if n.type=='BSDF_PRINCIPLED')
    col=bsdf.inputs['Base Color'].default_value
    m.name=names[m.name]
    rgb=[int(round(srgb(col[k])*255)) for k in range(3)]
    png(os.path.join(texdir,m.name+'.png'),rgb)
    print("MATERIAL",m.name,rgb,"used" if i in used else "UNUSED")
# 8. Report and write.
print("BONES",len(arm.data.bones),sorted(b.name for b in arm.data.bones))
print("TRIS",tris(),"VERTS",len(me.data.vertices),"GROUPS",len(me.vertex_groups))
ws=[me.matrix_world@v.co for v in me.data.vertices]
print("BOUNDS",[round(min(p[i] for p in ws),3) for i in range(3)],[round(max(p[i] for p in ws),3) for i in range(3)])
bpy.ops.object.select_all(action='SELECT')
import io_scene_fbx.encode_bin as fbx_bin
def plain_array(self,data,prop_type,length):
    self.props_type.append(prop_type);self.props.append(struct.pack('<3I',length,0,len(data))+data)
fbx_bin.FBXElem._add_array_helper=plain_array
bpy.ops.export_scene.fbx(filepath=out,use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,path_mode='STRIP',embed_textures=False,
    use_armature_deform_only=False,armature_nodetype='NULL',apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',mesh_smooth_type='FACE')
print("WROTE",out,os.path.getsize(out))
