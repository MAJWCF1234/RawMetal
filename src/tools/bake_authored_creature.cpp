#include "../ThirdParty/ufbx/ufbx.h"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <numeric>
// Bake the supplied rig's own clips, retaining runtime importer topology.
//   bake in.fbx out.bin                      the mutated human's authored clips
//   bake in.fbx out.bin idle move strike pain death [anchor]
// names the five clips (matched as anim-stack name suffixes) and writes RMA3:
// each clip sampled 30 times a second of its take, the idle and move loops
// crossfaded over their last fifth so they close on their first frame. With
// an anchor bone (e.g. mixamorig:Hips) the pain and death clips are held over
// the spot they start on: the bone's horizontal drift is taken out of every frame.
int main(int argc,char**argv){
 if(argc!=3&&argc!=8&&argc!=9)return 1;
 ufbx_load_opts opts{};opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1;opts.evaluate_skinning=true;
 auto scene=ufbx_load_file(argv[1],&opts,nullptr);if(!scene)return 2;
 for(auto s:scene->anim_stacks)std::cout<<s->name.data<<" "<<s->time_begin<<" "<<s->time_end<<"\n";
 std::vector<std::pair<uint32_t,uint32_t>> unique;std::vector<uint16_t> remap;std::map<std::pair<uint32_t,uint32_t>,uint16_t> lookup;
 for(auto n:scene->nodes)if(auto mesh=n->mesh){std::vector<uint32_t> ids(mesh->max_face_triangles*3);for(auto face:mesh->faces){auto count=ufbx_triangulate_face(ids.data(),ids.size(),mesh,face);for(size_t i=0;i<count*3;++i){auto key=std::make_pair(n->typed_id,mesh->vertex_indices[ids[i]]);auto it=lookup.find(key);if(it==lookup.end()){if(unique.size()>=65535)return 3;auto id=uint16_t(unique.size());lookup[key]=id;unique.push_back({n->typed_id,ids[i]});remap.push_back(id);}else remap.push_back(it->second);}}}
 const bool authored=argc==3;
 const char* defaults[]={"Idle_Watchful","Walk_Nervous","Attack_Lunge","WallSlam_Recover","WallSlam_Recover"};
 std::vector<std::string> clips;for(int c=0;c<5;++c)clips.push_back(authored?defaults[c]:argv[3+c]);
 double bindFloor=1e9;for(auto [node,index]:unique){auto n=scene->nodes[node];auto p=ufbx_transform_position(&n->geometry_to_world,ufbx_get_vertex_vec3(&n->mesh->vertex_position,index));bindFloor=std::min(bindFloor,p.y);}
 const ufbx_node* anchor=nullptr;if(argc==9){for(auto n:scene->nodes)if(std::string(n->name.data)==argv[8])anchor=n;if(!anchor)return 8;}
 std::vector<ufbx_anim_stack*> stacks;std::vector<uint32_t> samples;
 for(int c=0;c<5;++c){ufbx_anim_stack* clip=nullptr;for(auto s:scene->anim_stacks)if(std::string(s->name.data).ends_with(clips[size_t(c)]))clip=s;if(!clip)return 4;
  stacks.push_back(clip);samples.push_back(authored?24u:uint32_t(std::clamp(std::lround((clip->time_end-clip->time_begin)*30)+1,8l,240l)));}
 std::ofstream out(argv[2],std::ios::binary);out.write(authored?"RMA2":"RMA3",4);uint32_t vertices=uint32_t(remap.size()),frames=authored?24u:std::accumulate(samples.begin(),samples.end(),0u),clipCount=5,count=uint32_t(unique.size());
 for(auto value:{vertices,frames,clipCount,count})out.write(reinterpret_cast<char*>(&value),4);out.write(reinterpret_cast<char*>(remap.data()),remap.size()*2);
 if(!authored)out.write(reinterpret_cast<char*>(samples.data()),samples.size()*4);
 for(int c=0;c<5;++c){auto clip=stacks[size_t(c)];const unsigned frames=samples[size_t(c)];
  ufbx_vec3 start{};bool haveStart=false;std::vector<std::vector<ufbx_vec3>> clipFrames;
  for(unsigned f=0;f<frames;++f){double phase=double(f)/(frames-1);ufbx_evaluate_opts eo{};eo.evaluate_skinning=true;auto pose=ufbx_evaluate_scene(scene,clip->anim,clip->time_begin+phase*(clip->time_end-clip->time_begin),&eo,nullptr);if(!pose)return 5;
   ufbx_vec3 drift{};
   if(anchor&&c>=3){auto at=pose->nodes[anchor->typed_id]->node_to_world.cols[3];if(!haveStart){start=at;haveStart=true;}drift={at.x-start.x,0,at.z-start.z};}
   std::vector<ufbx_vec3> points;double minY=1e9;for(auto [node,index]:unique){auto n=pose->nodes[node];auto p=ufbx_get_vertex_vec3(&n->mesh->skinned_position,index);if(n->mesh->skinned_is_local)p=ufbx_transform_position(&n->geometry_to_world,p);
    p.x-=drift.x;p.z-=drift.z;
    // The mutated human has no death clip. Lay its authored recovery pose onto
    // its back, then ground its bounds rather than letting a standing corpse linger.
    if(c==4&&authored){double a=std::min(1.,phase*1.5)*1.57079632679;double y=p.y,z=p.z;p.y=y*std::cos(a)-z*std::sin(a);p.z=y*std::sin(a)+z*std::cos(a);}minY=std::min(minY,p.y);points.push_back(p);}
   if(c==4)for(auto& p:points)p.y-=minY;
   clipFrames.push_back(std::move(points));ufbx_free_scene(pose);
  }
  // Named clips stand on the bind mesh's floor, the floor the runtime bounds
  // come from: each clip's first frame is set down there and its own rise and
  // fall kept. (Retargeted takes and a rig's own idle can disagree by metres.)
  if(!authored){double first=1e9;for(const auto& p:clipFrames.front())first=std::min(first,p.y);const double lift=bindFloor-(c==4?0.:first);
   for(auto& frame:clipFrames)for(auto& p:frame)p.y+=lift;
   if(c<2){const auto first=clipFrames.front(),last=clipFrames.back();const double n=double(clipFrames.size()-1);
    for(size_t f=0;f<clipFrames.size();++f){const double w=std::clamp((double(f)-n*.8)/(n*.2),0.,1.);
     for(size_t i=0;i<first.size();++i){auto& p=clipFrames[f][i];p.x+=w*(first[i].x-last[i].x);p.y+=w*(first[i].y-last[i].y);p.z+=w*(first[i].z-last[i].z);}}}}
  for(const auto& frame:clipFrames)for(const auto& p:frame)for(double v:{p.x,p.y,p.z}){if(std::fabs(v)>32.7)return 6;int16_t q=int16_t(std::lround(v*1000));out.write(reinterpret_cast<char*>(&q),2);}
 }
 std::cout<<vertices/3<<" triangles; "<<count<<" skinned vertices; five clips\n";ufbx_free_scene(scene);return out?0:7;
}
