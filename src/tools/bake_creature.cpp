#include "../ThirdParty/ufbx/ufbx.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <map>

// Offline skeletal retargeting. Bake skinned vertices in importer triangle order;
// the runtime interpolates samples without evaluating an FBX scene per monster.
int main(int argc,char**argv){
 if(argc!=4)return 1;
 ufbx_load_opts options{};options.target_axes=ufbx_axes_right_handed_y_up;options.target_unit_meters=1;options.evaluate_skinning=true;
 auto target=ufbx_load_file(argv[1],&options,nullptr),source=ufbx_load_file(argv[2],&options,nullptr);if(!target||!source)return 2;
 const char* pairs[][2]={{"Hips","pelvis"},{"Spine","spine_01"},{"Spine1","spine_02"},{"Spine2","spine_03"},{"Neck","neck_01"},{"Head","Head"},{"LeftUpLeg","thigh_l"},{"RightUpLeg","thigh_r"},{"LeftLeg","calf_l"},{"RightLeg","calf_r"},{"LeftFoot","foot_l"},{"RightFoot","foot_r"},{"LeftToeBase","ball_l"},{"RightToeBase","ball_r"},{"LeftShoulder","clavicle_l"},{"RightShoulder","clavicle_r"},{"LeftArm","upperarm_l"},{"RightArm","upperarm_r"},{"LeftForeArm","lowerarm_l"},{"RightForeArm","lowerarm_r"},{"LeftHand","hand_l"},{"RightHand","hand_r"}};
 std::vector<int> mapping(target->nodes.count,-1);
 for(auto&p:pairs){auto a=ufbx_find_node(target,(std::string("mixamorig:")+p[0]).c_str()),b=ufbx_find_node(source,p[1]);if(!a||!b)return 3;mapping[a->typed_id]=int(b->typed_id);}
 auto inverse=[](ufbx_quat q){return ufbx_quat{-q.x,-q.y,-q.z,q.w};};
 auto rotation=[](ufbx_node*n){return ufbx_matrix_to_transform(&n->node_to_world).rotation;};
 auto difference=[](ufbx_node*a,ufbx_node*b){return ufbx_vec3{b->node_to_world.m03-a->node_to_world.m03,b->node_to_world.m13-a->node_to_world.m13,b->node_to_world.m23-a->node_to_world.m23};};
 auto unit=[](ufbx_vec3 v){double n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return ufbx_vec3{v.x/n,v.y/n,v.z/n};};
 auto arc=[&](ufbx_vec3 a,ufbx_vec3 b){a=unit(a);b=unit(b);ufbx_quat q{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x,1+a.x*b.x+a.y*b.y+a.z*b.z};double length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);if(length<1e-6){auto axis=unit(std::fabs(a.x)<.8?ufbx_vec3{0,a.z,-a.y}:ufbx_vec3{-a.z,0,a.x});return ufbx_quat{axis.x,axis.y,axis.z,0};}return ufbx_quat{q.x/length,q.y/length,q.z/length,q.w/length};};
 auto stack=[&](const char* name){for(auto a:source->anim_stacks){std::string label=a->name.data;auto separator=label.rfind('|');if(label.substr(separator==std::string::npos?0:separator+1)==name)return a;}return (ufbx_anim_stack*)nullptr;};
 ufbx_evaluate_opts eo{};eo.evaluate_skinning=true;
 auto rest=ufbx_evaluate_scene(source,stack("A_TPose")->anim,0,&eo,nullptr);if(!rest)return 4;
 auto tr=ufbx_find_node(target,"mixamorig:RightArm"),tl=ufbx_find_node(target,"mixamorig:LeftArm"),sr=ufbx_find_node(rest,"upperarm_r"),sl=ufbx_find_node(rest,"upperarm_l");
 double yaw=std::atan2(sr->node_to_world.m23-sl->node_to_world.m23,sr->node_to_world.m03-sl->node_to_world.m03)-std::atan2(tr->node_to_world.m23-tl->node_to_world.m23,tr->node_to_world.m03-tl->node_to_world.m03);
 ufbx_quat align{0,std::sin(yaw*.5),0,std::cos(yaw*.5)};
 std::vector<std::pair<uint32_t,uint32_t>> corners;
 for(auto n:target->nodes)if(auto mesh=n->mesh){std::vector<uint32_t> ids(mesh->max_face_triangles*3);for(auto face:mesh->faces){auto count=ufbx_triangulate_face(ids.data(),ids.size(),mesh,face);for(size_t i=0;i<count*3;++i)corners.push_back({n->typed_id,ids[i]});}}
 const char* clips[]={"Idle_Loop","Walk_Loop","Punch_Cross","Hit_Chest","Death01"};
 std::vector<std::pair<uint32_t,uint32_t>> unique;std::vector<uint16_t> remap;std::map<std::pair<uint32_t,uint32_t>,uint16_t> lookup;
 for(auto [node,index]:corners){auto key=std::make_pair(node,target->nodes[node]->mesh->vertex_indices[index]);auto it=lookup.find(key);
  if(it==lookup.end()){uint16_t id=uint16_t(unique.size());lookup[key]=id;unique.push_back({node,index});remap.push_back(id);}else remap.push_back(it->second);
 }
 std::ofstream out(argv[3],std::ios::binary);out.write("RMA2",4);uint32_t vertices=uint32_t(corners.size()),frames=16,clipsCount=5;
 for(auto value:{vertices,frames,clipsCount})out.write(reinterpret_cast<char*>(&value),4);
 uint32_t count=uint32_t(unique.size());out.write(reinterpret_cast<char*>(&count),4);out.write(reinterpret_cast<char*>(remap.data()),remap.size()*2);
 for(auto name:clips){auto clip=stack(name);if(!clip)return 5;double minimumArmAlignment=1;
  for(unsigned sample=0;sample<frames;++sample){double phase=double(sample)/(frames-1);auto pose=ufbx_evaluate_scene(source,clip->anim,clip->time_begin+phase*(clip->time_end-clip->time_begin),&eo,nullptr);if(!pose)return 6;
   std::vector<ufbx_quat> world(target->nodes.count);std::vector<bool> ready(target->nodes.count);std::vector<ufbx_transform_override> changes;
   std::function<ufbx_quat(ufbx_node*)> solve=[&](ufbx_node*n)->ufbx_quat{
    if(!n)return {0,0,0,1};if(ready[n->typed_id])return world[n->typed_id];auto parent=solve(n->parent);auto tr=n->local_transform;int mapped=mapping[n->typed_id];
    auto global=ufbx_quat_mul(parent,tr.rotation);
    if(mapped>=0){auto delta=ufbx_quat_mul(rotation(pose->nodes[mapped]),inverse(rotation(rest->nodes[mapped])));delta=ufbx_quat_mul(align,ufbx_quat_mul(delta,inverse(align)));global=ufbx_quat_mul(delta,rotation(n));
     // Bone rolls differ between these rigs. Match the actual shoulder/elbow/
     // wrist directions rather than assuming their rotational bases agree.
     std::string label=n->name.data;const char* child=label.ends_with("LeftArm")?"LeftForeArm":label.ends_with("RightArm")?"RightForeArm":label.ends_with("LeftForeArm")?"LeftHand":label.ends_with("RightForeArm")?"RightHand":nullptr;
     if(child){auto end=ufbx_find_node(target,(std::string("mixamorig:")+child).c_str());auto desired=ufbx_quat_rotate_vec3(align,difference(pose->nodes[mapped],pose->nodes[mapping[end->typed_id]]));global=ufbx_quat_mul(arc(difference(n,end),desired),rotation(n));}
     if(label.ends_with("LeftHand")||label.ends_with("RightHand"))global=ufbx_quat_mul(parent,ufbx_quat_mul(inverse(rotation(n->parent)),rotation(n)));
     tr.rotation=ufbx_quat_mul(inverse(parent),global);
     if(std::string(n->name.data)=="mixamorig:Hips"){double scale=n->node_to_world.m13/rest->nodes[mapped]->node_to_world.m13;auto inv=ufbx_matrix_invert(&n->parent->node_to_world);auto delta=ufbx_transform_direction(&inv,{0,(pose->nodes[mapped]->node_to_world.m13-rest->nodes[mapped]->node_to_world.m13)*scale,0});tr.translation.x+=delta.x;tr.translation.y+=delta.y;tr.translation.z+=delta.z;}
     changes.push_back({n->typed_id,tr});
    }
    ready[n->typed_id]=true;return world[n->typed_id]=global;
   };
   for(auto n:target->nodes)solve(n);
   ufbx_anim_opts ao{};ao.ignore_connections=true;ao.transform_overrides={changes.data(),changes.size()};auto anim=ufbx_create_anim(target,&ao,nullptr);
   auto skinned=ufbx_evaluate_scene(target,anim,0,&eo,nullptr);if(!skinned)return 7;
   for(const char* side:{"Left","Right"})for(const char* part:{"Arm","ForeArm"}){
    auto joint=ufbx_find_node(target,(std::string("mixamorig:")+side+part).c_str()),end=ufbx_find_node(target,(std::string("mixamorig:")+side+(std::string(part)=="Arm"?"ForeArm":"Hand")).c_str());
    auto actual=unit(difference(skinned->nodes[joint->typed_id],skinned->nodes[end->typed_id]));auto desired=unit(ufbx_quat_rotate_vec3(align,difference(pose->nodes[mapping[joint->typed_id]],pose->nodes[mapping[end->typed_id]])));
    minimumArmAlignment=std::min(minimumArmAlignment,actual.x*desired.x+actual.y*desired.y+actual.z*desired.z);
   }
   std::vector<ufbx_vec3> positions;double floor=1e9;
   for(auto [node,index]:unique){auto n=skinned->nodes[node];auto p=ufbx_get_vertex_vec3(&n->mesh->skinned_position,index);if(n->mesh->skinned_is_local)p=ufbx_transform_position(&n->geometry_to_world,p);positions.push_back(p);floor=std::min(floor,double(p.y));}
   // These are grounded clips: correct proportion-related floor penetration.
   for(auto p:positions){p.y-=floor;
    for(double value:{p.x,p.y,p.z}){if(std::fabs(value)>32)return 8;int16_t q=int16_t(std::round(value*1000));out.write(reinterpret_cast<char*>(&q),2);}
   }
   if(std::string(name)=="Punch_Cross"){auto hand=ufbx_find_node(skinned,"mixamorig:RightHand"),hips=ufbx_find_node(skinned,"mixamorig:Hips");std::cout<<"swing phase "<<phase<<" hand reach "<<hand->node_to_world.m23-hips->node_to_world.m23<<'\n';}
   ufbx_free_scene(skinned);ufbx_free_anim(anim);ufbx_free_scene(pose);
  }
  std::cout<<name<<": "<<frames<<" skeletal samples, "<<vertices<<" corners, minimum arm direction dot "<<minimumArmAlignment<<'\n';if(minimumArmAlignment<.985)return 10;
 }
 ufbx_free_scene(rest);ufbx_free_scene(source);ufbx_free_scene(target);return out?0:9;
}
