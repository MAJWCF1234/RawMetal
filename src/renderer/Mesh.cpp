#include "Mesh.h"
#include "../core/PackedResource.h"
#include <windows.h>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <sstream>
#include <map>
namespace retro {
Mesh::Mesh(int id,const char* nodeFilter):m_materialParts(id>=163&&id<200),m_nodeFilter(nodeFilter){
 auto resource=loadResource(id);
 ufbx_load_opts opts{};opts.evaluate_skinning=true;opts.target_axes=ufbx_axes_right_handed_y_up;opts.target_unit_meters=1;
 if(!resource.empty()&&resource[0]=='#')opts.file_format=UFBX_FILE_FORMAT_OBJ;
 ufbx_error error{};m_scene=ufbx_load_memory(resource.data(),resource.size(),&opts,&error);
 if(!m_scene)throw std::runtime_error("FBX mesh decode failed");
 for(auto material:m_scene->materials)materialNames.emplace_back(material->name.data);
 extract(m_scene);
 // This supplied vent lies in the XZ ceiling plane, with its grille toward -Y.
 // Rotate it into a wall panel: grille toward +Z, horizontal texture louvers.
 if(id==166)for(auto&face:triangles)for(auto&v:face.v){auto p=v.p;v.p={-p.z,p.x,-p.y};}
 minimum={1e9f,1e9f,1e9f};maximum={-1e9f,-1e9f,-1e9f};
 for(auto&t:triangles)for(auto&v:t.v){minimum.x=std::min(minimum.x,v.p.x);minimum.y=std::min(minimum.y,v.p.y);minimum.z=std::min(minimum.z,v.p.z);maximum.x=std::max(maximum.x,v.p.x);maximum.y=std::max(maximum.y,v.p.y);maximum.z=std::max(maximum.z,v.p.z);}
 std::ostringstream info;info<<id<<": "<<triangles.size()<<" triangles; bounds "<<minimum.x<<","<<minimum.y<<","<<minimum.z<<" to "<<maximum.x<<","<<maximum.y<<","<<maximum.z<<"\n";
 for(auto node:m_scene->nodes)if(node->bone){++bones;info<<" bone "<<node->name.data<<" at "<<node->node_to_world.m03<<","<<node->node_to_world.m13<<","<<node->node_to_world.m23<<"\n";}
 description=info.str();if(triangles.empty())throw std::runtime_error("Mesh has no triangles");
 for(auto n:m_scene->nodes)if(n->mesh){description+=" mesh "+std::string(n->name.data)+" triangles "+std::to_string(n->mesh->num_triangles)+"\n";for(auto mat:n->materials)description+=" material "+std::string(mat->name.data)+"\n";}
 description+="Animation stacks: "+std::to_string(m_scene->anim_stacks.count)+"\n";
 for(auto stack:m_scene->anim_stacks)description+=std::string(stack->name.data)+"\n";
 if(id==111){
  // Retain the authored finger poses from the supplied left/right grab actions.
  for(auto stack:m_scene->anim_stacks){std::string name=stack->name.data;bool left=name.find("grab.L")!=std::string::npos,right=name.find("grab.R")!=std::string::npos;if(!left&&!right)continue;
   ufbx_evaluate_opts eo{};auto frame=ufbx_evaluate_scene(m_scene,stack->anim,stack->time_end,&eo,nullptr);
   if(frame){for(auto n:frame->nodes){std::string bone=n->name.data;
    if((bone.find("f_")==0||bone.find("thumb.")==0)&&bone.find(left?".L":".R")!=std::string::npos&&bone.find("_end")==std::string::npos)m_fingerGrip.push_back({n->typed_id,n->local_transform});
   }ufbx_free_scene(frame);}
  }
  description+="Authored grab finger transforms: "+std::to_string(m_fingerGrip.size())+"\n";
  m_bindScene=m_scene;ufbx_retain_scene(m_bindScene);grip();extract(m_scene);
  description+="Arm opening rings: "+std::to_string(openRings.size())+"\n";
 }
}
Mesh::~Mesh(){ufbx_free_scene(m_scene);ufbx_free_scene(m_bindScene);}
Point3 Mesh::bonePosition(const char* name)const{auto n=ufbx_find_node(m_scene,name);if(!n)return {};return {float(n->node_to_world.m03),float(n->node_to_world.m13),float(n->node_to_world.m23)};}
void Mesh::grip(Point3 right,Point3 left,float swing,float pitch,float yaw){
 auto add=[](ufbx_vec3 a,ufbx_vec3 b){return ufbx_vec3{a.x+b.x,a.y+b.y,a.z+b.z};};
 auto sub=[](ufbx_vec3 a,ufbx_vec3 b){return ufbx_vec3{a.x-b.x,a.y-b.y,a.z-b.z};};
 auto mul=[](ufbx_vec3 a,double s){return ufbx_vec3{a.x*s,a.y*s,a.z*s};};
 auto dot=[](ufbx_vec3 a,ufbx_vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;};
 auto unit=[&](ufbx_vec3 a){return mul(a,1.0/std::sqrt(std::max(1e-12,dot(a,a))));};
 auto position=[](ufbx_node*n){return ufbx_vec3{n->node_to_world.m03,n->node_to_world.m13,n->node_to_world.m23};};
 auto arc=[&](ufbx_vec3 a,ufbx_vec3 b){a=unit(a);b=unit(b);ufbx_quat q{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x,1+dot(a,b)};double s=1/std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);return ufbx_quat{q.x*s,q.y*s,q.z*s,q.w*s};};
 auto rotated=[&](ufbx_node*n,ufbx_quat delta){auto global=ufbx_matrix_to_transform(&n->node_to_world).rotation;auto parent=ufbx_matrix_to_transform(&n->parent->node_to_world).rotation;parent={-parent.x,-parent.y,-parent.z,parent.w};auto tr=n->local_transform;tr.rotation=ufbx_quat_mul(parent,ufbx_quat_mul(delta,global));return tr;};
 // Two-bone IK establishes the grip using the actual upper-arm and forearm bones.
 auto cross=[](ufbx_vec3 a,ufbx_vec3 b){return ufbx_vec3{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};};
 ufbx_quat handRot[2];for(int s=0;s<2;++s){
  std::string suffix=s?".L":".R";auto hand=ufbx_find_node(m_scene,("hand"+suffix).c_str());
  auto forward=unit(sub(position(ufbx_find_node(m_scene,("f_middle.01"+suffix).c_str())),position(hand)));
  auto across=sub(position(ufbx_find_node(m_scene,("f_index.01"+suffix).c_str())),position(ufbx_find_node(m_scene,("f_pinky.01"+suffix).c_str())));
  auto palm=unit(s?cross(forward,across):cross(across,forward));
  // Support palm faces up across the fore-end; trigger palm faces the stock.
  ufbx_vec3 desiredForward=unit(s?ufbx_vec3{-.95,0,.312}:ufbx_vec3{0,.65,.76});
  ufbx_vec3 desiredPalm=s?ufbx_vec3{0,1,0}:ufbx_vec3{1,0,0};
  ufbx_vec3 source[]={forward,palm,cross(forward,palm)},target[]={desiredForward,desiredPalm,cross(desiredForward,desiredPalm)};
  ufbx_matrix rotation{};
  for(int row=0;row<3;++row)for(int col=0;col<3;++col)for(int axis=0;axis<3;++axis)rotation.cols[col].v[row]+=target[axis].v[row]*source[axis].v[col];
  handRot[s]=ufbx_quat_mul(ufbx_matrix_to_transform(&rotation).rotation,ufbx_matrix_to_transform(&hand->node_to_world).rotation);
 }
 for(int step=0;step<3;++step){std::vector<ufbx_transform_override> changes;
  for(int side=0;side<2;++side){std::string suffix=side?".L":".R";
   auto upper=ufbx_find_node(m_scene,("upper_arm"+suffix).c_str()),elbow=ufbx_find_node(m_scene,("forearm"+suffix).c_str()),hand=ufbx_find_node(m_scene,("hand"+suffix).c_str());
   if(!upper||!elbow||!hand)continue;
   auto target=side?left:right;
   ufbx_vec3 S=position(upper),E=position(elbow),H=position(hand),T{target.x,target.y,target.z};
   if(step==0){auto axis=unit(sub(T,S));double l1=std::sqrt(dot(sub(E,S),sub(E,S))),l2=std::sqrt(dot(sub(H,E),sub(H,E))),d=std::sqrt(dot(sub(T,S),sub(T,S)));d=std::clamp(d,std::fabs(l1-l2)+.001,l1+l2-.001);
    double along=(l1*l1-l2*l2+d*d)/(2*d),height=std::sqrt(std::max(0.,l1*l1-along*along));
    ufbx_vec3 bend={side?1.:-1.,-.8+double(swing)*.6,-.2+double(swing)};bend=unit(sub(bend,mul(axis,dot(bend,axis))));auto targetElbow=add(S,add(mul(axis,along),mul(bend,height)));
    changes.push_back({upper->typed_id,rotated(upper,arc(sub(E,S),sub(targetElbow,S)))});
   }else if(step==1)changes.push_back({elbow->typed_id,rotated(elbow,arc(sub(H,E),sub(T,E)))});
   else {auto parent=ufbx_matrix_to_transform(&hand->parent->node_to_world).rotation;parent={-parent.x,-parent.y,-parent.z,parent.w};auto tr=hand->local_transform;double angle=yaw;ufbx_quat turn{0,std::sin(angle*.5),0,std::cos(angle*.5)},tilt{std::sin(pitch*.5),0,0,std::cos(pitch*.5)};tr.rotation=ufbx_quat_mul(parent,ufbx_quat_mul(turn,ufbx_quat_mul(tilt,handRot[side])));changes.push_back({hand->typed_id,tr});}
  }
  for(auto n:m_scene->nodes)if(n->bone&&std::none_of(changes.begin(),changes.end(),[&](auto&c){return c.node_id==n->typed_id;}))changes.push_back({n->typed_id,n->local_transform});
  ufbx_anim_opts ao{};ao.transform_overrides={changes.data(),changes.size()};auto anim=ufbx_create_anim(m_scene,&ao,nullptr);ufbx_evaluate_opts eo{};eo.evaluate_skinning=step==2;auto posed=ufbx_evaluate_scene(m_scene,anim,0,&eo,nullptr);ufbx_free_anim(anim);
  if(!posed)throw std::runtime_error("Arm IK evaluation failed");ufbx_free_scene(m_scene);m_scene=posed;
 }
}
void Mesh::poseAttached(Point3 right,Point3 left,float elbowSwing,float pitch,float yaw,float phase,float recoil){
 if(!m_bindScene)return;
 ufbx_free_scene(m_scene);m_scene=m_bindScene;ufbx_retain_scene(m_scene);
 grip(right,left,elbowSwing,pitch,yaw);pose(phase,recoil);
}
bool Mesh::poseAction(const char* action,float phase){
 if(!m_bindScene)return false;
 for(auto stack:m_bindScene->anim_stacks){std::string name=stack->name.data;
  if(name.ends_with(action)){ufbx_evaluate_opts opts{};opts.evaluate_skinning=true;
   auto frame=ufbx_evaluate_scene(m_bindScene,stack->anim,stack->time_begin+(stack->time_end-stack->time_begin)*std::clamp(phase,0.f,1.f),&opts,nullptr);
   if(!frame)return false;ufbx_free_scene(m_scene);m_scene=frame;extract(frame);return true;
  }
 }return false;
}
void Mesh::extract(ufbx_scene* scene){
 // Animation changes positions, not topology, UVs, materials or boundary
 // connectivity. Triangulate and discover shoulder rings only once per asset.
 if(m_topologyReady){
  auto position=[&](uint32_t nodeId,uint32_t corner){auto node=scene->nodes[nodeId];auto p=ufbx_get_vertex_vec3(&node->mesh->skinned_position,corner);if(node->mesh->skinned_is_local)p=ufbx_transform_position(&node->geometry_to_world,p);return Point3{float(p.x),float(p.y),float(p.z)};};
  triangles.resize(m_cachedTriangles.size());for(size_t i=0;i<triangles.size();++i){auto&source=m_cachedTriangles[i];triangles[i]=source.prototype;for(int c=0;c<3;++c)triangles[i].v[c].p=position(source.node,source.corners[c]);}
  openRings.resize(m_cachedRings.size());for(size_t i=0;i<openRings.size();++i){auto&source=m_cachedRings[i];auto&ring=openRings[i];ring.resize(source.corners.size());for(size_t j=0;j<ring.size();++j)ring[j]={position(source.node,source.corners[j]),0,0};}
  return;
 }
 triangles.clear();
 openRings.clear();
 for(auto node:scene->nodes){auto mesh=node->mesh;if(!mesh||(!m_nodeFilter.empty()&&m_nodeFilter!=node->name.data))continue;
  // Detect actual open mesh boundaries, independent of UV seam duplication.
  if(std::string(node->name.data)=="ArmsMesh"){
   struct Edge{uint32_t a,b,faceA,faceB;int count;};std::map<std::pair<uint32_t,uint32_t>,Edge> edges;
   for(auto face:mesh->faces)for(uint32_t i=0;i<face.num_indices;++i){uint32_t ia=face.index_begin+i,ib=face.index_begin+(i+1)%face.num_indices;
    uint32_t a=mesh->vertex_indices.data[ia],b=mesh->vertex_indices.data[ib];auto key=std::minmax(a,b);auto&edge=edges[{key.first,key.second}];edge={a,b,ia,ib,edge.count+1};
   }
   std::map<uint32_t,uint32_t> next,corner;
   for(auto&entry:edges){auto&e=entry.second;if(e.count==1){next[e.a]=e.b;corner[e.a]=e.faceA;corner[e.b]=e.faceB;}}
   while(!next.empty()){std::vector<MeshVertex> ring;CachedRing cached{node->typed_id,{}};auto start=next.begin()->first,current=start;bool closed=false;
    for(size_t i=0;i<mesh->num_vertices;++i){auto it=next.find(current);if(it==next.end())break;
     auto ix=corner[current];auto p=ufbx_get_vertex_vec3(&mesh->skinned_position,ix);if(mesh->skinned_is_local)p=ufbx_transform_position(&node->geometry_to_world,p);
     ring.push_back({{float(p.x),float(p.y),float(p.z)},0,0});cached.corners.push_back(ix);current=it->second;next.erase(it);if(current==start){closed=true;break;}
    }
    if(closed&&ring.size()>=3){openRings.push_back(ring);m_cachedRings.push_back(std::move(cached));}
   }
  }
  std::vector<uint32_t> indices(mesh->max_face_triangles*3);
  for(size_t faceIndex=0;faceIndex<mesh->faces.count;++faceIndex){auto face=mesh->faces[faceIndex];auto count=ufbx_triangulate_face(indices.data(),indices.size(),mesh,face);
   for(uint32_t t=0;t<count;++t){MeshTriangle triangle;std::string partName=node->name.data;triangle.part=partName.find("Wing")!=std::string::npos?1:partName=="Bolt"?2:partName=="ShotgunBullet"?3:0;
    if(m_materialParts&&mesh->face_material.count>faceIndex){auto materialIndex=mesh->face_material[faceIndex];if(materialIndex<node->materials.count){std::string materialName=node->materials[materialIndex]->name.data;auto found=std::find(materialNames.begin(),materialNames.end(),materialName);triangle.part=int(found-materialNames.begin());}}
    for(int c=0;c<3;++c){uint32_t ix=indices[t*3+c];auto p=ufbx_get_vertex_vec3(&mesh->skinned_position,ix);
     if(mesh->skinned_is_local)p=ufbx_transform_position(&node->geometry_to_world,p);
     ufbx_vec2 uv{};if(mesh->vertex_uv.exists)uv=ufbx_get_vertex_vec2(&mesh->vertex_uv,ix);
     triangle.v[c]={{float(p.x),float(p.y),float(p.z)},float(uv.x),1.f-float(uv.y)};
    }triangles.push_back(triangle);m_cachedTriangles.push_back({node->typed_id,{indices[t*3],indices[t*3+1],indices[t*3+2]},triangle});
   }
  }
 }
 m_topologyReady=true;
}
void Mesh::pose(float phase,float recoil){
 if(!bones)return;
 std::vector<ufbx_transform_override> overrides;
 for(auto node:m_scene->nodes){if(!node->bone)continue;
  std::string name=node->name.data;
  bool finger=name.find("f_")==0&&name.find("_end")==std::string::npos;
  auto tr=node->local_transform;
  auto authored=std::find_if(m_fingerGrip.begin(),m_fingerGrip.end(),[&](auto&t){return t.node_id==node->typed_id;});
  if(authored!=m_fingerGrip.end()){
   // Grab endpoints are closed fists. Blend each joint toward a firearm grasp,
   // preserving a relaxed support palm and a distinct trigger finger.
   bool support=name.find(".L")!=std::string::npos;
   float curl=support?.55f:.60f;
   if(support&&name.find(".02.")!=std::string::npos)curl=.85f;
   if(support&&name.find(".03.")!=std::string::npos)curl=.90f;
   if(name.find("f_index")==0&&!support)curl=.20f;
   if(name.find("thumb.")==0)curl=support?.60f:.56f;
   if(!support&&name.find(".03.")!=std::string::npos)curl*=.7f;
   tr.rotation=ufbx_quat_slerp(tr.rotation,authored->transform.rotation,curl);
  }
  const double angle=finger?((authored==m_fingerGrip.end()?.8:0)+std::sin(phase)*.008+recoil*.015):0;
  ufbx_quat q{std::sin(angle*.5),0,0,std::cos(angle*.5)};
  auto r=tr.rotation;
  tr.rotation={q.w*r.x+q.x*r.w,q.w*r.y-q.x*r.z,q.w*r.z+q.x*r.y,q.w*r.w-q.x*r.x};
  overrides.push_back({node->typed_id,tr});
 }
 ufbx_anim_opts animOpts{};animOpts.transform_overrides={overrides.data(),overrides.size()};
 auto anim=ufbx_create_anim(m_scene,&animOpts,nullptr);if(!anim)throw std::runtime_error("Rig animation creation failed");
 ufbx_evaluate_opts opts{};opts.evaluate_skinning=true;
 auto evaluated=ufbx_evaluate_scene(m_scene,anim,0,&opts,nullptr);
 if(evaluated){extract(evaluated);ufbx_free_scene(evaluated);}ufbx_free_anim(anim);
}
}
