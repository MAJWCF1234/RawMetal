#include "../ThirdParty/ufbx/ufbx.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
// List source nodes, or export one node's original triangles and UVs as OBJ.
int main(int argc,char**argv){
 if(argc!=2&&argc!=4&&argc!=5)return 1;
 ufbx_load_opts options{};options.target_axes=ufbx_axes_right_handed_y_up;options.target_unit_meters=1;options.evaluate_skinning=true;
 ufbx_error error{};auto scene=ufbx_load_file(argv[1],&options,&error);if(!scene)return 2;
 bool exported=false,assembly=argc>=4&&std::strcmp(argv[2],"--assembly")==0;
 std::ofstream out;if(argc>=4){out.open(argv[3]);if(!out)return 3;out<<"# RawMetal imported world-space triangles and UVs\n"<<std::setprecision(9);}unsigned vertex=1;
 if(argc==2){
  for(auto stack:scene->anim_stacks)std::cout<<"ANIMATION "<<stack->name.data<<" "<<stack->time_begin<<".."<<stack->time_end<<'\n';
  for(auto node:scene->nodes)if(node->bone)std::cout<<"BONE "<<node->name.data<<'\n';
 }
 for(auto node:scene->nodes)if(auto mesh=node->mesh){
  std::cout<<node->name.data<<" | "<<mesh->num_triangles<<" triangles";
  for(auto material:node->materials)std::cout<<" | "<<material->name.data;
  std::cout<<'\n';
  if(argc<4||(!assembly&&std::strcmp(argv[2],node->name.data)))continue;
  out<<"o "<<node->name.data<<'\n';
  std::vector<uint32_t> indices(mesh->max_face_triangles*3);
  for(size_t f=0;f<mesh->faces.count;++f){auto face=mesh->faces[f];auto triangles=ufbx_triangulate_face(indices.data(),indices.size(),mesh,face);
   if(mesh->face_material.count>f&&mesh->face_material[f]<node->materials.count)out<<"usemtl "<<node->materials[mesh->face_material[f]]->name.data<<'\n';
   for(uint32_t triangle=0;triangle<triangles;++triangle){
    for(int corner=0;corner<3;++corner){auto index=indices[triangle*3+corner];auto p=ufbx_get_vertex_vec3(&mesh->vertex_position,index);p=ufbx_transform_position(&node->geometry_to_world,p);auto uv=ufbx_get_vertex_vec2(&mesh->vertex_uv,index);
     if(argc==5&&std::strcmp(argv[4],"--skin-rest")==0){p=ufbx_get_vertex_vec3(&mesh->skinned_position,index);if(mesh->skinned_is_local)p=ufbx_transform_position(&node->geometry_to_world,p);}
     if(argc==5&&std::strcmp(argv[4],"--lay-flat")==0){double y=p.y;p.y=-p.z;p.z=y;}
     out<<"v "<<float(p.x)<<' '<<float(p.y)<<' '<<float(p.z)<<"\nvt "<<float(uv.x)<<' '<<float(uv.y)<<'\n';
    }
    out<<"f "<<vertex<<'/'<<vertex<<' '<<vertex+1<<'/'<<vertex+1<<' '<<vertex+2<<'/'<<vertex+2<<'\n';vertex+=3;
   }
  }
  exported=true;
 }
 ufbx_free_scene(scene);return argc>=4&&!exported?4:0;
}
