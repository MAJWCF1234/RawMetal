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
#    (the weights blend with height), and the torso's weight is then graded up Hips, Spine1, Spine2, Spine3 by
#    height (leon_grade.py: the avatar's hips-to-chest weights jumped over a hand's breadth and the waist snapped),
#  - joins the meshes and decimates them to MAX_TRIANGLES (11,800 by default, about 12,700 in the end): flat faces
#    are merged first (planar, never on the arms), then the plain body (torso, legs, stockings) is collapsed while the
#    head (eyes, nose, ears, fur, the bow), the paws (hands, fingers, feet: found by their bone weights and place), the
#    arms (kept whole: their rings are what bends at the elbow) and the thighs' tops are held, then the head and the
#    paws each on their own to LEON_HEAD / LEON_PAWS (5,600 and 3,000 triangles by default), all with the mesh's
#    mirror symmetry kept,
#  - moves the upper arm's joint out of the chest to where the skin turns from chest to arm (LEON_SHOULDER_X),
#  - keeps at most four weights a vertex, names the materials LeonFur, LeonInner,
#    LeonBlack, LeonMetal and writes one 8x8 solid-colour PNG for each
#    (the engine finds a body's texture by material name),
#  - writes the FBX with +Z ahead and +X his left, its arrays undeflated.
import bpy, bmesh, sys, os, zlib, struct, math
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import leon_grade
argv=sys.argv[sys.argv.index('--')+1:]
src,out,texdir=argv[0],argv[1],argv[2]
LIMIT=int(argv[3]) if len(argv)>3 else 11800
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
JOINTS=[bones['Hips'].head_local.z,z_s1,z_s2,z_s3]; HIPS_Y=bones['Hips'].head_local.y   # the spine's joints, for leon_grade
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
        spread(w,v.co); shoulder(w,v.co); leon_grade.grade(w,v.co,JOINTS,HIPS_Y)
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
# The head (above the neck bone; the bow sits on the neck), the paws (hands, fingers, feet, toes) and the thighs'
# tops (where the stockings end) are protected: the plain body (torso, arms, legs, stockings) is collapsed first
# while they are held, then the head and the paws are cut gently on their own. A decimate ratio is of the whole
# mesh's triangles, so every pass's target is the mesh's total.
neck_z=bones['Neck'].head_local.z
gname={g.index:g.name for g in me.vertex_groups}
HAND_G={n+str(i)+'_'+s for n in ('Thumb','Index','Middle','Ring','Pinky') for i in (1,2,3) for s in 'LR'}|{'Hand_L','Hand_R'}
FOOT_G={'Foot_L','Foot_R','Toe_L','Toe_R'}
ARM_G={'Arm_L','Arm_R','ForeArm_L','ForeArm_R'}
vin=lambda v,names: sum(g.weight for g in v.groups if gname[g.group] in names)
BAND=(float(os.environ.get('LEON_BAND_LO','0.55')),float(os.environ.get('LEON_BAND_HI','0.9')))
HAND_X=float(os.environ.get('LEON_HAND_X','0.62')); FOOT_Z=float(os.environ.get('LEON_FOOT_Z','0.14'))   # the paws' reach in the T-pose
ARM_X=float(os.environ.get('LEON_ARM_X','0.14'))   # the arms leave the torso here in the T-pose
region={}   # vertex index -> 'head' | 'paw' | 'arm' | 'band' | 'body'
for v in me.data.vertices:
    if (vin(v,HAND_G)>=0.5 and abs(v.co.x)>=HAND_X) or (vin(v,FOOT_G)>=0.5 and v.co.z<=FOOT_Z): r='paw'
    # The arms are a few rings of 16 (the source's): collapsed with the plain body they lost their forearm rings and
    # their faces ran from the upper arm to the paw, flat sheets ("wings") once the elbow bent. They are kept whole.
    elif vin(v,ARM_G)>=0.5 and abs(v.co.x)>=ARM_X and v.co.z<neck_z-0.02: r='arm'
    elif v.co.z>=neck_z-0.02: r='head'
    elif BAND[0]<=v.co.z<=BAND[1]: r='band'
    else: r='body'
    region[v.index]=r
if os.environ.get('LEON_DIAG'):
    bmd=bmesh.new(); bmd.from_mesh(me.data); bmd.verts.ensure_lookup_table(); seen=set(); import collections; st_=collections.defaultdict(list)
    for v0 in bmd.verts:
        if v0.index in seen: continue
        stack=[v0]; seen.add(v0.index); comp=[v0]
        while stack:
            x=stack.pop()
            for e in x.link_edges:
                w=e.other_vert(x)
                if w.index not in seen: seen.add(w.index); stack.append(w); comp.append(w)
        fs={f for x in comp for f in x.link_faces}; rr=collections.Counter(region[x.index] for x in comp).most_common(1)[0][0]
        st_[rr].append(sum(len(f.verts)-2 for f in fs))
    for r,l in st_.items(): print('ISLANDS',r,len(l),sorted(l,reverse=True)[:14],'small(<=40)',sum(1 for t in l if t<=40),sum(t for t in l if t<=40))
    for r in ('paw','arm','head','band','body'):
        pts=[me.data.vertices[i].co for i,k in region.items() if k==r]
        print('DIAG',r,len(pts),[round(min(p[a] for p in pts),2) for a in range(3)],[round(max(p[a] for p in pts),2) for a in range(3)])
def group_of(name,regions):
    g=me.vertex_groups.new(name=name)
    for i,r in region.items(): g.add([i],1.0 if r in regions else 0.0,'REPLACE')
for r in ('head','paw','arm','band'): group_of('r_'+r,{r})
def count(r_in):
    # triangles whose corners all carry the region's mark (the marks follow the vertices through the collapses)
    idx=[me.vertex_groups['r_'+r].index for r in r_in]
    w=[ {g.group for g in v.groups if g.group in idx and g.weight>0.5} for v in me.data.vertices]
    return sum(len(p.vertices)-2 for p in me.data.polygons if all(w[i] for i in p.vertices))
# 4a. Planar: faces that already lie in one plane (the same material) merge, so the budget goes where the shape is.
PLANAR=float(os.environ.get('LEON_PLANAR','1.0'))
if PLANAR>0:
    # never on the arms: a straight tube's sides are flat along its length, so its rings dissolved and the faces left
    # spanned the elbow
    bm=bmesh.new(); bm.from_mesh(me.data); bm.verts.ensure_lookup_table()
    free=lambda v: region[v.index]!='arm'
    bmesh.ops.dissolve_limit(bm,angle_limit=math.radians(PLANAR),use_dissolve_boundaries=False,verts=[v for v in bm.verts if free(v)],
                             edges=[e for e in bm.edges if all(free(v) for v in e.verts)],delimit={'MATERIAL'})
    bmesh.ops.triangulate(bm,faces=bm.faces[:],quad_method='BEAUTY',ngon_method='BEAUTY')
    bm.to_mesh(me.data); bm.free(); me.data.update()
    print('PLANAR',PLANAR,'->',tris())
HEAD=int(os.environ.get('LEON_HEAD','5600')); PAWS=int(os.environ.get('LEON_PAWS','3000'))
def cut(invert,target,group):
    mod=me.modifiers.new('d','DECIMATE');mod.decimate_type='COLLAPSE';mod.use_collapse_triangulate=True
    mod.vertex_group=group;mod.invert_vertex_group=invert;mod.vertex_group_factor=float(os.environ.get('LEON_HOLD','1000'))   # held is held
    mod.ratio=min(1.0,target/tris())
    if os.environ.get('LEON_SYM','1')=='1': mod.use_symmetry=True; mod.symmetry_axis='X'
    bpy.ops.object.modifier_apply(modifier=mod.name)
def regions_tris():
    d={k:count({k}) for k in ('head','paw','arm','band')}; d['body']=tris()-sum(d.values()); return d
print('REGIONS before',regions_tris(),'of',tris())
group_of('hold',{'head','paw','arm','band'}); group_of('keep',{'head'}); group_of('pawg',{'paw'})
h=regions_tris()
# the plain body is cut so that the whole mesh would hold LIMIT with the head and paws at their targets
body_target=h['head']+h['paw']+h['arm']+h['band']+max(0,LIMIT-HEAD-PAWS-h['arm']-h['band'])
cut(True,min(tris(),body_target),'hold')
print('BODY cut',tris(),regions_tris())
cut(False,tris()-count({'head'})+HEAD,'keep')
print('HEAD cut',tris(),regions_tris())
cut(False,tris()-count({'paw'})+PAWS,'pawg')
print("DECIMATED",tris(),regions_tris())
for n in ('keep','hold','pawg','r_head','r_paw','r_arm','r_band'): me.vertex_groups.remove(me.vertex_groups[n])
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
# The avatar's arm joint sits inside the chest (0.125 m out) while its skin turns from chest to arm at 0.15-0.18 m. The
# takes set the arms' directions, so a pivot that far in hung his long arms from a narrow yoke: in Claire's rifle-ready
# idle and walk his hands met in front of his crotch and the forearms crossed (hands 4-6% of his height apart, the
# Limitator's 10-12%). The upper arm's joint goes out to where the skin turns (LEON_SHOULDER_X).
SHOULDER_X=float(os.environ.get('LEON_SHOULDER_X','0.17'))
for s,k in (('L',1),('R',-1)):
    a=eb['Arm_'+s]; a.use_connect=False; a.head.x=k*SHOULDER_X; eb['Shoulder_'+s].tail=a.head.copy()
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
