# Grades the torso weights of a built Leon.fbx (leon_grade.py) without the avatar's source file:
#   blender -b --python tools/player/regrade_leon.py -- IN.fbx OUT.fbx
# build_leon.py applies the same grade while it merges the weights; this applies it to the merged weights of the
# finished model (the same function), limits a vertex to four weights again and writes the FBX as build_leon.py does.
import bpy, sys, os, struct
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import leon_grade
src,out=sys.argv[sys.argv.index('--')+1:][:2]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
arm=[o for o in bpy.data.objects if o.type=='ARMATURE'][0]
me=[o for o in bpy.data.objects if o.type=='MESH'][0]
bones=arm.data.bones
joint=lambda n:(arm.matrix_world@bones[n].head_local)
JOINTS=[joint(n).z for n in leon_grade.SPINE]; HIPS_Y=joint('Hips').y
print("JOINTS",[round(z,3) for z in JOINTS],"HIPS_Y",round(HIPS_Y,3))
names={g.index:g.name for g in me.vertex_groups}
changed=0
for v in me.data.vertices:
    w={names[g.group]:g.weight for g in v.groups}; before=dict(w)
    leon_grade.grade(w,me.matrix_world@v.co,JOINTS,HIPS_Y)
    if w==before: continue
    changed+=1
    for n in leon_grade.SPINE:
        g=me.vertex_groups.get(n)
        if g is None:
            if n in w: g=me.vertex_groups.new(name=n)
            else: continue
        if n in w: g.add([v.index],w[n],'REPLACE')
        elif n in before: g.remove([v.index])
print("REGRADED",changed,"of",len(me.data.vertices))
bpy.ops.object.select_all(action='DESELECT'); me.select_set(True); bpy.context.view_layer.objects.active=me
bpy.ops.object.vertex_group_limit_total(group_select_mode='ALL',limit=4)
bpy.ops.object.vertex_group_normalize_all(group_select_mode='ALL',lock_active=False)
bpy.ops.object.select_all(action='SELECT')
import io_scene_fbx.encode_bin as fbx_bin
def plain_array(self,data,prop_type,length):
    self.props_type.append(prop_type);self.props.append(struct.pack('<3I',length,0,len(data))+data)
fbx_bin.FBXElem._add_array_helper=plain_array
bpy.ops.export_scene.fbx(filepath=out,use_selection=True,object_types={'ARMATURE','MESH'},add_leaf_bones=False,bake_anim=False,path_mode='STRIP',embed_textures=False,
    use_armature_deform_only=False,armature_nodetype='NULL',apply_unit_scale=True,apply_scale_options='FBX_SCALE_NONE',mesh_smooth_type='FACE')
print("WROTE",out,os.path.getsize(out))
