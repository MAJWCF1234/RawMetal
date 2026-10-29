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

// Mixamo takes onto a Mixamo-named rig (the transformed worker):
//   bake --takes target.fbx out.bin TAKE.fbx:FIRST:LAST[:drift] ...
// one take per clip, five or more in the runtime's order (idle, move, strike,
// pain, death, then any extra). FIRST and LAST are the take's 30 fps frames
// (1 is its first); each clip is sampled at every frame of that window and
// written as RMA3. Each take's own static pose is its rest. The hips keep
// their rise and fall, and with :drift their travel across the floor too (a
// lunge that carries the body forward); the idle and move loops are
// crossfaded over their last fifth so they close on their first frame.
static int bakeTakes(int argc,char**argv){
 ufbx_load_opts options{};options.target_axes=ufbx_axes_right_handed_y_up;options.target_unit_meters=1;options.evaluate_skinning=true;
 auto target=ufbx_load_file(argv[2],&options,nullptr);if(!target)return 2;
 const char* bones[]={"Hips","Spine","Spine1","Spine2","Neck","Head","LeftUpLeg","RightUpLeg","LeftLeg","RightLeg","LeftFoot","RightFoot","LeftToeBase","RightToeBase","LeftShoulder","RightShoulder","LeftArm","RightArm","LeftForeArm","RightForeArm","LeftHand","RightHand"};
 auto inverse=[](ufbx_quat q){return ufbx_quat{-q.x,-q.y,-q.z,q.w};};
 auto rotation=[](ufbx_node*n){return ufbx_matrix_to_transform(&n->node_to_world).rotation;};
 auto difference=[](ufbx_node*a,ufbx_node*b){return ufbx_vec3{b->node_to_world.m03-a->node_to_world.m03,b->node_to_world.m13-a->node_to_world.m13,b->node_to_world.m23-a->node_to_world.m23};};
 auto unit=[](ufbx_vec3 v){double n=std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);return ufbx_vec3{v.x/n,v.y/n,v.z/n};};
 auto arc=[&](ufbx_vec3 a,ufbx_vec3 b){a=unit(a);b=unit(b);ufbx_quat q{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x,1+a.x*b.x+a.y*b.y+a.z*b.z};double length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);if(length<1e-6){auto axis=unit(std::fabs(a.x)<.8?ufbx_vec3{0,a.z,-a.y}:ufbx_vec3{-a.z,0,a.x});return ufbx_quat{axis.x,axis.y,axis.z,0};}return ufbx_quat{q.x/length,q.y/length,q.z/length,q.w/length};};
 ufbx_evaluate_opts eo{};eo.evaluate_skinning=true;
 std::vector<std::pair<uint32_t,uint32_t>> corners;
 for(auto n:target->nodes)if(auto mesh=n->mesh){std::vector<uint32_t> ids(mesh->max_face_triangles*3);for(auto face:mesh->faces){auto count=ufbx_triangulate_face(ids.data(),ids.size(),mesh,face);for(size_t i=0;i<count*3;++i)corners.push_back({n->typed_id,ids[i]});}}
 std::vector<std::pair<uint32_t,uint32_t>> unique;std::vector<uint16_t> remap;std::map<std::pair<uint32_t,uint32_t>,uint16_t> lookup;
 for(auto [node,index]:corners){auto key=std::make_pair(node,target->nodes[node]->mesh->vertex_indices[index]);auto it=lookup.find(key);
  if(it==lookup.end()){if(unique.size()>=65535)return 3;uint16_t id=uint16_t(unique.size());lookup[key]=id;unique.push_back({node,index});remap.push_back(id);}else remap.push_back(it->second);
 }
 struct Take{std::string file;int first=1,last=1;bool drift=false;};std::vector<Take> takes;
 for(int i=4;i<argc;++i){std::string spec=argv[i];Take t;if(spec.ends_with(":drift")){t.drift=true;spec.resize(spec.size()-6);}
  auto b=spec.rfind(':'),a=b==std::string::npos?b:spec.rfind(':',b-1);if(a==std::string::npos||a<2)return 1; // a drive letter's colon is never the field separator
  t.file=spec.substr(0,a);t.first=std::stoi(spec.substr(a+1,b-a-1));t.last=std::stoi(spec.substr(b+1));if(t.first<1||t.last<=t.first)return 1;takes.push_back(t);}
 std::vector<uint32_t> samples;for(auto&t:takes)samples.push_back(uint32_t(std::clamp(t.last-t.first+1,8,240)));
 std::ofstream out(argv[3],std::ios::binary);out.write("RMA3",4);uint32_t vertices=uint32_t(corners.size()),frames=0,clipCount=uint32_t(takes.size()),count=uint32_t(unique.size());
 for(auto s:samples)frames+=s;
 for(auto value:{vertices,frames,clipCount,count})out.write(reinterpret_cast<char*>(&value),4);out.write(reinterpret_cast<char*>(remap.data()),remap.size()*2);out.write(reinterpret_cast<char*>(samples.data()),samples.size()*4);
 for(size_t c=0;c<takes.size();++c){const auto&take=takes[c];
  auto source=ufbx_load_file(take.file.c_str(),&options,nullptr);if(!source||source->anim_stacks.count==0)return 4;auto clip=source->anim_stacks[0];
  std::vector<int> mapping(target->nodes.count,-1);
  for(auto name:bones){auto label=std::string("mixamorig:")+name;auto a=ufbx_find_node(target,label.c_str()),b=ufbx_find_node(source,label.c_str());if(!a||!b)return 3;mapping[a->typed_id]=int(b->typed_id);}
  auto tr=ufbx_find_node(target,"mixamorig:RightArm"),tl=ufbx_find_node(target,"mixamorig:LeftArm"),sr=ufbx_find_node(source,"mixamorig:RightArm"),sl=ufbx_find_node(source,"mixamorig:LeftArm");
  double yaw=std::atan2(sr->node_to_world.m23-sl->node_to_world.m23,sr->node_to_world.m03-sl->node_to_world.m03)-std::atan2(tr->node_to_world.m23-tl->node_to_world.m23,tr->node_to_world.m03-tl->node_to_world.m03);
  ufbx_quat align{0,std::sin(yaw*.5),0,std::cos(yaw*.5)};
  auto sourceHips=ufbx_find_node(source,"mixamorig:Hips");ufbx_vec3 start{};
  double minimumArmAlignment=1;std::vector<std::vector<ufbx_vec3>> clipFrames;
  for(unsigned sample=0;sample<samples[c];++sample){double frame=take.first-1+double(sample)*(take.last-take.first)/(samples[c]-1);
   auto pose=ufbx_evaluate_scene(source,clip->anim,clip->time_begin+frame/30.,&eo,nullptr);if(!pose)return 6;
   if(sample==0)start=pose->nodes[sourceHips->typed_id]->node_to_world.cols[3];
   std::vector<ufbx_quat> world(target->nodes.count);std::vector<bool> ready(target->nodes.count);std::vector<ufbx_transform_override> changes;
   std::function<ufbx_quat(ufbx_node*)> solve=[&](ufbx_node*n)->ufbx_quat{
    if(!n)return {0,0,0,1};if(ready[n->typed_id])return world[n->typed_id];auto parent=solve(n->parent);auto tr=n->local_transform;int mapped=mapping[n->typed_id];
    auto global=ufbx_quat_mul(parent,tr.rotation);
    if(mapped>=0){auto delta=ufbx_quat_mul(rotation(pose->nodes[mapped]),inverse(rotation(source->nodes[mapped])));delta=ufbx_quat_mul(align,ufbx_quat_mul(delta,inverse(align)));global=ufbx_quat_mul(delta,rotation(n));
     std::string label=n->name.data;const char* child=label.ends_with("LeftArm")?"LeftForeArm":label.ends_with("RightArm")?"RightForeArm":label.ends_with("LeftForeArm")?"LeftHand":label.ends_with("RightForeArm")?"RightHand":nullptr;
     if(child){auto end=ufbx_find_node(target,(std::string("mixamorig:")+child).c_str());auto desired=ufbx_quat_rotate_vec3(align,difference(pose->nodes[mapped],pose->nodes[mapping[end->typed_id]]));global=ufbx_quat_mul(arc(difference(n,end),desired),rotation(n));}
     if(label.ends_with("LeftHand")||label.ends_with("RightHand"))global=ufbx_quat_mul(parent,ufbx_quat_mul(inverse(rotation(n->parent)),rotation(n)));
     tr.rotation=ufbx_quat_mul(inverse(parent),global);
     if(label=="mixamorig:Hips"){double scale=n->node_to_world.m13/source->nodes[mapped]->node_to_world.m13;auto at=pose->nodes[mapped]->node_to_world.cols[3];
      ufbx_vec3 move{take.drift?(at.x-start.x)*scale:0.,(at.y-source->nodes[mapped]->node_to_world.m13)*scale,take.drift?(at.z-start.z)*scale:0.};move=ufbx_quat_rotate_vec3(align,move);
      auto inv=ufbx_matrix_invert(&n->parent->node_to_world);auto delta=ufbx_transform_direction(&inv,move);tr.translation.x+=delta.x;tr.translation.y+=delta.y;tr.translation.z+=delta.z;}
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
   // Grounded: each frame's lowest point stands on the floor.
   std::vector<ufbx_vec3> positions;double floor=1e9;
   for(auto [node,index]:unique){auto n=skinned->nodes[node];auto p=ufbx_get_vertex_vec3(&n->mesh->skinned_position,index);if(n->mesh->skinned_is_local)p=ufbx_transform_position(&n->geometry_to_world,p);positions.push_back(p);floor=std::min(floor,double(p.y));}
   for(auto&p:positions)p.y-=floor;
   clipFrames.push_back(std::move(positions));
   ufbx_free_scene(skinned);ufbx_free_anim(anim);ufbx_free_scene(pose);
  }
  if(c<2){const auto first=clipFrames.front(),last=clipFrames.back();const double n=double(clipFrames.size()-1);
   for(size_t f=0;f<clipFrames.size();++f){const double w=std::clamp((double(f)-n*.8)/(n*.2),0.,1.);
    for(size_t i=0;i<first.size();++i){auto& p=clipFrames[f][i];p.x+=w*(first[i].x-last[i].x);p.y+=w*(first[i].y-last[i].y);p.z+=w*(first[i].z-last[i].z);}}}
  for(const auto&frame:clipFrames)for(const auto&p:frame)for(double v:{p.x,p.y,p.z}){if(std::fabs(v)>32.7)return 8;int16_t q=int16_t(std::lround(v*1000));out.write(reinterpret_cast<char*>(&q),2);}
  std::cout<<take.file<<" "<<take.first<<"-"<<take.last<<": "<<samples[c]<<" skeletal samples, minimum arm direction dot "<<minimumArmAlignment<<'\n';if(minimumArmAlignment<.985)return 10;
  ufbx_free_scene(source);
 }
 std::cout<<vertices/3<<" triangles; "<<count<<" skinned vertices; "<<clipCount<<" clips\n";ufbx_free_scene(target);return out?0:9;
}
// Offline skeletal retargeting. Bake skinned vertices in importer triangle order;
// the runtime interpolates samples without evaluating an FBX scene per monster.
int main(int argc,char**argv){
 if(argc>=9&&std::string(argv[1])=="--takes")return bakeTakes(argc,argv);
 if(argc==2){ufbx_load_opts o{};auto s=ufbx_load_file(argv[1],&o,nullptr);if(!s)return 2;for(auto a:s->anim_stacks)std::cout<<a->name.data<<'\n';for(auto n:s->nodes)if(n->bone)std::cout<<"BONE "<<n->name.data<<'\n';ufbx_free_scene(s);return 0;}
 if(argc!=4&&argc!=5)return 1;
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
 std::vector<std::string> clips{"Idle_Loop","Walk_Loop","Punch_Cross","Hit_Chest","Death01"};
 if(argc==5)clips.push_back("Crouch_Idle_Loop");
 std::vector<std::pair<uint32_t,uint32_t>> unique;std::vector<uint16_t> remap;std::map<std::pair<uint32_t,uint32_t>,uint16_t> lookup;
 for(auto [node,index]:corners){auto key=std::make_pair(node,target->nodes[node]->mesh->vertex_indices[index]);auto it=lookup.find(key);
  if(it==lookup.end()){uint16_t id=uint16_t(unique.size());lookup[key]=id;unique.push_back({node,index});remap.push_back(id);}else remap.push_back(it->second);
 }
 std::ofstream out(argv[3],std::ios::binary);out.write("RMA2",4);uint32_t vertices=uint32_t(corners.size()),frames=16,clipsCount=uint32_t(clips.size());
 for(auto value:{vertices,frames,clipsCount})out.write(reinterpret_cast<char*>(&value),4);
 uint32_t count=uint32_t(unique.size());out.write(reinterpret_cast<char*>(&count),4);out.write(reinterpret_cast<char*>(remap.data()),remap.size()*2);
 std::ofstream anchors;if(argc==5)anchors.open(std::string(argv[3])+".anchors",std::ios::binary);
 for(auto& name:clips){auto clip=stack(name.c_str());if(!clip)return 5;double minimumArmAlignment=1;
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
   if(anchors)for(const char* joint:{"mixamorig:RightHand","mixamorig:RightForeArm"}){auto n=ufbx_find_node(skinned,joint);float xyz[]={float(n->node_to_world.m03),float(n->node_to_world.m13-floor),float(n->node_to_world.m23)};anchors.write(reinterpret_cast<char*>(xyz),sizeof(xyz));}
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
