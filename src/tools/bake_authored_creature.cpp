#include "../ThirdParty/ufbx/ufbx.h"
#include <iostream>
#include <fstream>
#include <map>
#include <vector>
#include <string>
#include <cstdint>
#include <cmath>
#include <algorithm>
// Bake the supplied rig's own clips, retaining runtime importer topology.
int main(int argc,char**argv){
 if(argc!=3)return 1;
 ufbx_load_opts opts{};opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1;opts.evaluate_skinning=true;
 auto scene=ufbx_load_file(argv[1],&opts,nullptr);if(!scene)return 2;
 for(auto s:scene->anim_stacks)std::cout<<s->name.data<<" "<<s->time_begin<<" "<<s->time_end<<"\n";
 std::vector<std::pair<uint32_t,uint32_t>> unique;std::vector<uint16_t> remap;std::map<std::pair<uint32_t,uint32_t>,uint16_t> lookup;
 for(auto n:scene->nodes)if(auto mesh=n->mesh){std::vector<uint32_t> ids(mesh->max_face_triangles*3);for(auto face:mesh->faces){auto count=ufbx_triangulate_face(ids.data(),ids.size(),mesh,face);for(size_t i=0;i<count*3;++i){auto key=std::make_pair(n->typed_id,mesh->vertex_indices[ids[i]]);auto it=lookup.find(key);if(it==lookup.end()){if(unique.size()>=65535)return 3;auto id=uint16_t(unique.size());lookup[key]=id;unique.push_back({n->typed_id,ids[i]});remap.push_back(id);}else remap.push_back(it->second);}}}
 const char* clips[]={"Idle_Watchful","Walk_Nervous","Attack_Lunge","WallSlam_Recover","WallSlam_Recover"};
 std::ofstream out(argv[2],std::ios::binary);out.write("RMA2",4);uint32_t vertices=uint32_t(remap.size()),frames=24,clipCount=5,count=uint32_t(unique.size());
 for(auto value:{vertices,frames,clipCount,count})out.write(reinterpret_cast<char*>(&value),4);out.write(reinterpret_cast<char*>(remap.data()),remap.size()*2);
 for(int c=0;c<5;++c){ufbx_anim_stack* clip=nullptr;for(auto s:scene->anim_stacks)if(std::string(s->name.data).ends_with(clips[c]))clip=s;if(!clip)return 4;
  for(unsigned f=0;f<frames;++f){double phase=double(f)/(frames-1);ufbx_evaluate_opts eo{};eo.evaluate_skinning=true;auto pose=ufbx_evaluate_scene(scene,clip->anim,clip->time_begin+phase*(clip->time_end-clip->time_begin),&eo,nullptr);if(!pose)return 5;
   std::vector<ufbx_vec3> points;double minY=1e9;for(auto [node,index]:unique){auto n=pose->nodes[node];auto p=ufbx_get_vertex_vec3(&n->mesh->skinned_position,index);if(n->mesh->skinned_is_local)p=ufbx_transform_position(&n->geometry_to_world,p);
    // No death clip was supplied. Lay the authored recovery pose onto its back,
    // then ground its bounds rather than letting a standing corpse linger.
    if(c==4){double a=std::min(1.,phase*1.5)*1.57079632679;double y=p.y,z=p.z;p.y=y*std::cos(a)-z*std::sin(a);p.z=y*std::sin(a)+z*std::cos(a);}minY=std::min(minY,p.y);points.push_back(p);}
   for(auto p:points){if(c==4)p.y-=minY;for(double v:{p.x,p.y,p.z}){if(std::fabs(v)>32.7)return 6;int16_t q=int16_t(std::lround(v*1000));out.write(reinterpret_cast<char*>(&q),2);}}ufbx_free_scene(pose);
  }
 }
 std::cout<<vertices/3<<" triangles; "<<count<<" skinned vertices; five clips\n";ufbx_free_scene(scene);return out?0:7;
}
