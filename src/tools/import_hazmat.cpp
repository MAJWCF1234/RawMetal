#include "../ThirdParty/ufbx/ufbx.h"
#include <fstream>
#include <vector>
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <iostream>
int main(int argc,char**argv){
 if(argc!=3)return 1;ufbx_load_opts opts{};opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1;opts.evaluate_skinning=true;
 auto scene=ufbx_load_file(argv[1],&opts,nullptr);if(!scene)return 2;
 const char* names[]={"Hips","Spine2","Head","LeftArm","LeftForeArm","LeftHand","RightArm","RightForeArm","RightHand","LeftUpLeg","LeftLeg","LeftFoot","RightUpLeg","RightLeg","RightFoot"};
 std::array<ufbx_node*,15> nodes{};for(int i=0;i<15;++i){nodes[i]=ufbx_find_node(scene,(std::string("mixamorig:")+names[i]).c_str());if(!nodes[i])return 3;}
 ufbx_node* node=nullptr;for(auto n:scene->nodes)if(n->mesh)node=n;if(!node||!node->mesh->skin_deformers.count)return 4;auto mesh=node->mesh;
 auto position=[&](uint32_t index){auto p=ufbx_get_vertex_vec3(&mesh->skinned_position,index);return mesh->skinned_is_local?ufbx_transform_position(&node->geometry_to_world,p):p;};
 double low=1e9,high=-1e9;for(uint32_t i=0;i<mesh->num_indices;++i){auto p=position(i);low=std::min(low,double(p.y));high=std::max(high,double(p.y));}double scale=1.7/(high-low),cx=nodes[0]->node_to_world.m03,cz=nodes[0]->node_to_world.m23;
 auto convert=[&](ufbx_vec3 p){return std::array<float,3>{float((p.x-cx)*scale),float((p.z-cz)*scale),float((p.y-low)*scale)};};
 std::ofstream out(argv[2],std::ios::binary);auto write=[&](auto v){out.write(reinterpret_cast<char*>(&v),sizeof(v));};out.write("RMR1",4);write(uint32_t(mesh->num_triangles*3));
 for(auto n:nodes)for(auto v:convert({n->node_to_world.m03,n->node_to_world.m13,n->node_to_world.m23}))write(v);
 auto skin=mesh->skin_deformers[0];std::vector<int> clusterMap;
 for(auto cluster:skin->clusters){auto bone=cluster->bone_node;int mapped=-1;while(bone&&mapped<0){for(int i=0;i<15;++i)if(bone==nodes[i])mapped=i;bone=bone->parent;}clusterMap.push_back(std::max(0,mapped));}
 std::vector<uint32_t> indices(mesh->max_face_triangles*3);
 for(size_t f=0;f<mesh->faces.count;++f){auto count=ufbx_triangulate_face(indices.data(),indices.size(),mesh,mesh->faces[f]);
  for(size_t c=0;c<count*3;++c){auto index=indices[c];auto p=convert(position(index));auto uv=ufbx_get_vertex_vec2(&mesh->vertex_uv,index);
   for(float v:p)write(int16_t(std::round(v*1000)));write(uint16_t(std::round(std::clamp(double(uv.x),0.,1.)*65535)));write(uint16_t(std::round(std::clamp(double(uv.y),0.,1.)*65535)));
   std::array<double,15> weights{};auto vertex=skin->vertices[mesh->vertex_indices[index]];for(uint32_t j=0;j<vertex.num_weights;++j){auto weight=skin->weights[vertex.weight_begin+j];weights[clusterMap[weight.cluster_index]]+=weight.weight;}
   int a=int(std::max_element(weights.begin(),weights.end())-weights.begin());double weightA=weights[a];weights[a]=0;int b=int(std::max_element(weights.begin(),weights.end())-weights.begin());double total=weightA+weights[b];
   write(uint8_t(a));write(uint8_t(b));write(uint8_t(total>0?std::round(weightA/total*255):255));write(uint8_t(mesh->face_material.count?mesh->face_material[f]:0));
  }
 }
 std::cout<<mesh->num_triangles<<" triangles, 15 joints, authored two-weight skin, original material slots\n";ufbx_free_scene(scene);return out?0:5;
}
