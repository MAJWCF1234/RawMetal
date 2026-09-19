#include "../ThirdParty/ufbx/ufbx.h"
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>
// List source nodes, or export one node's original triangles and UVs as OBJ.
int main(int argc,char**argv){
 if(argc!=2&&argc!=4&&argc!=5)return 1;
 ufbx_load_opts options{};options.target_axes=ufbx_axes_right_handed_y_up;options.target_unit_meters=1;
 ufbx_error error{};auto scene=ufbx_load_file(argv[1],&options,&error);if(!scene)return 2;
 bool exported=false;
 for(auto node:scene->nodes)if(auto mesh=node->mesh){
  std::cout<<node->name.data<<" | "<<mesh->num_triangles<<" triangles";
  for(auto material:node->materials)std::cout<<" | "<<material->name.data;
  std::cout<<'\n';
  if(argc<4||std::strcmp(argv[2],node->name.data))continue;
  std::ofstream out(argv[3]);if(!out)return 3;out<<"# RawMetal: original asset node "<<node->name.data<<'\n'<<std::setprecision(9);
  std::vector<uint32_t> indices(mesh->max_face_triangles*3);unsigned vertex=1;
  for(auto face:mesh->faces){auto triangles=ufbx_triangulate_face(indices.data(),indices.size(),mesh,face);
   for(uint32_t triangle=0;triangle<triangles;++triangle){
    for(int corner=0;corner<3;++corner){auto index=indices[triangle*3+corner];auto p=ufbx_get_vertex_vec3(&mesh->vertex_position,index);p=ufbx_transform_position(&node->geometry_to_world,p);auto uv=ufbx_get_vertex_vec2(&mesh->vertex_uv,index);
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
