# The Limitator's player body (docs/Systems/player-body.md): the purchased
# O:/retro official/Limitator.zip FBX made into a Claire-convention body.
#   blender -b --python tools/player/build_limitator.py -- Limitator.fbx src/assets/library/player/Limitator.fbx [TEXTURE_DIR]
# (run where the pack's two PNGs sit beside its FBX; TEXTURE_DIR receives the
# halved LimitatorBody.png and LimitatorHead.png)
# The pack's mesh is bound in a pose its armature does not rest in: this bakes
# the T-pose it is drawn in into the mesh and makes that the rest, applies the
# armature's 0.01 scale and quarter turn, names the bones Claire's way (so
# RawMetalCreatureBake, ClaireOutside and the renderer read him as they read
# her), keeps at most four weights a vertex, and drops the pack's animation
# and its rotation-target empties. +X is his left, +Z ahead, 2.7 m tall.
import bpy, sys, os
argv=sys.argv[sys.argv.index('--')+1:]
src,out=argv[0],argv[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm=[o for o in bpy.data.objects if o.type=='ARMATURE'][0]
me=[o for o in bpy.data.objects if o.type=='MESH'][0]
for o in list(bpy.data.objects):
    if o not in (arm,me): bpy.data.objects.remove(o,do_unlink=True)
view=bpy.context.view_layer
# 1. Bake the drawn T-pose into the mesh, then make it the armature's rest.
bpy.ops.object.select_all(action='DESELECT'); view.objects.active=me; me.select_set(True)
mod=me.modifiers[0]
bpy.ops.object.modifier_copy(modifier=mod.name)
bpy.ops.object.modifier_apply(modifier=me.modifiers[0].name)
view.objects.active=arm; arm.select_set(True); me.select_set(False)
bpy.ops.object.mode_set(mode='POSE'); bpy.ops.pose.select_all(action='SELECT'); bpy.ops.pose.armature_apply(selected=False); bpy.ops.object.mode_set(mode='OBJECT')
if arm.animation_data: arm.animation_data.action=None
for a in list(bpy.data.actions): bpy.data.actions.remove(a)
# 2. Unit transforms: apply the armature's rotation and scale, and the mesh's.
bpy.ops.object.select_all(action='DESELECT'); arm.select_set(True); me.select_set(True); view.objects.active=arm
bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
for pb in arm.pose.bones: pb.matrix_basis.identity()
# 3. Claire's bone names (Mixamo's fourth finger joint is her finger's _end).
ren={'Hips':'Hips','Spine':'Spine1','Spine1':'Spine2','Spine2':'Spine3','Neck':'Neck','Head':'Head','HeadTop_End':'Head_end'}
for s,c in (('Left','L'),('Right','R')):
    ren.update({s+'Shoulder':'Shoulder_'+c,s+'Arm':'Arm_'+c,s+'ForeArm':'ForeArm_'+c,s+'Hand':'Hand_'+c,s+'UpLeg':'LegUp_'+c,s+'Leg':'LegLow_'+c,s+'Foot':'Foot_'+c,s+'ToeBase':'Toe_'+c,s+'Toe_End':'Toe_'+c+'_end'})
    for f in ('Thumb','Index','Middle','Ring','Pinky'):
        for j in (1,2,3): ren[s+'Hand'+f+str(j)]=f+str(j)+'_'+c
        ren[s+'Hand'+f+'4']=f+'3_'+c+'_end'
bpy.ops.object.select_all(action='DESELECT'); view.objects.active=arm; arm.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
eb=arm.data.edit_bones
for b in list(eb):
    n=b.name.replace('mixamorig:','')
    if n not in ren: eb.remove(b)
bpy.ops.object.mode_set(mode='OBJECT')
for b in arm.data.bones:
    n=b.name.replace('mixamorig:','')
    b.name=ren[n]   # vertex groups follow a bone rename
arm.name='Armature'; arm.data.name='Armature'; me.name='Limitator'; me.data.name='Limitator'
# 4. At most four weights a vertex, normalized.
bpy.ops.object.select_all(action='DESELECT'); view.objects.active=me; me.select_set(True)
bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL',limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL',lock_active=False)
# One image per material; the renderer picks his two textures by these names.
for m in me.data.materials:
    m.name=m.name.replace('R.001','')
    for n in m.node_tree.nodes:
        if n.type=='TEX_IMAGE' and n.image and n.image.name.endswith('.001'):
            base=bpy.data.images.get(n.image.name[:-4])
            if base: n.image=base
# 5. His textures are mirror images left and right, and each face keeps to one
# half: every face is folded onto the left half and the textures keep only
# that half (512x512 each), the right side wearing the left's paint.
uv=me.data.uv_layers.active.data
for p in me.data.polygons:
    us=[uv[li].uv[0] for li in p.loop_indices]
    if min(us)<0.5-1e-4 and max(us)>0.5+1e-4: raise SystemExit('a face straddles the mirror seam')
    right=min(us)>=0.5-1e-4
    for li in p.loop_indices:
        u=uv[li].uv[0]; uv[li].uv[0]=(1-u if right else u)*2
if len(argv)>2:
    import numpy as np
    names={'LimitatorBody':'LimitatorBody.png','LimitatorHead':'LimitatorHead.png'}
    for m in me.data.materials:
        image=next(n.image for n in m.node_tree.nodes if n.type=='TEX_IMAGE' and n.image)
        w,h=image.size;px=np.empty(w*h*4,dtype=np.float32);image.pixels.foreach_get(px);px=px.reshape(h,w,4)[:,:w//2].copy()
        half=bpy.data.images.new(m.name+'_half',w//2,h,alpha=True);half.pixels.foreach_set(px.ravel())
        half.filepath_raw=os.path.join(argv[2],names[m.name]);half.file_format='PNG';half.save()
        print("TEXTURE",half.filepath_raw,w//2,h)
print("BONES",len(arm.data.bones),"TRIS",sum(len(p.vertices)-2 for p in me.data.polygons))
ws=[me.matrix_world@v.co for v in me.data.vertices]
print("BOUNDS",[round(min(p[i] for p in ws),3) for i in range(3)],[round(max(p[i] for p in ws),3) for i in range(3)])
bpy.ops.object.select_all(action='SELECT')
# Arrays written plain, not deflated: the asset packer's LZMS packs the plain
# file smaller (177 KB against 212 KB for the deflated one).
import io_scene_fbx.encode_bin as fbx_bin
from struct import pack
def plain_array(self,data,prop_type,length):
    self.props_type.append(prop_type);self.props.append(pack('<3I',length,0,len(data))+data)
fbx_bin.FBXElem._add_array_helper=plain_array
bpy.ops.export_scene.fbx(filepath=out,use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,path_mode='STRIP',embed_textures=False,
    use_armature_deform_only=False,armature_nodetype='NULL',apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',mesh_smooth_type='FACE')
print("WROTE",out,os.path.getsize(out))
