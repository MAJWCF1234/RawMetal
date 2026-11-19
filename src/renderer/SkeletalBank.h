#pragma once
// RMS1: a creature animation bank kept as bones, not baked vertices.
//
// RawMetalCreatureBake --skeletal-takes writes it from the same takes as
// --takes; on load it is skinned back into the RMA3 bank --takes would have
// written (the same corners, the same skinned-vertex order, each sample
// grounded on its lowest point, loops closed over their last fifth, int16
// millimetres), so everything that samples a bank is unchanged. Per sample it
// holds one transform for each skin cluster of the model's mesh (the cluster's
// geometry-to-world: a rotation, a translation, and the scale it keeps), so a
// body of 2,600 vertices on 40 clips packs in a fraction of its vertex bank.
//
// Layout (little endian):
//   "RMS1" u32 corners, u32 samples (all clips), u32 clips, u32 unique, u32 clusters
//   u16 remap[corners]               corner -> skinned vertex, as RMA3
//   u32 node[unique], u32 vertex[unique]   each skinned vertex: its node (typed id) and mesh vertex
//   u32 count[clips], u8 loop[clips]
//   f32 scale[clusters][3], f32 pivot[clusters][3]
//   i32 first[clips][clusters][3]    each clip's first pivot position (1/10000 m)
//   u32 changes, then that many i16 values (zigzag), their low bytes then their high bytes:
//     per clip, per cluster, the quaternion x,y,z,w planes (count each, 1/16384, each
//     the change from the sample before, the first from 0), then the pivot's world
//     position x,y,z planes (count-1 changes after its first). The pivot is the
//     cluster's bone in mesh space, so it moves as far as the bone does (the mesh
//     origin, swung on a hand, moves metres in a sample).
#include "../ThirdParty/ufbx/ufbx.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>
namespace retro::skeletal {
constexpr double kQuat=16384.,kMetre=10000.;
inline bool isBank(const std::vector<unsigned char>& data){return data.size()>=24&&!std::memcmp(data.data(),"RMS1",4);}
// The corners of every mesh node in node order, triangulated as the importer
// and the bake do, and their skinned vertices (one per node and mesh vertex).
struct Topology{std::vector<std::pair<uint32_t,uint32_t>> corners,unique;std::vector<uint16_t> remap;};
inline bool topology(const ufbx_scene* scene,Topology& t){
 t={};
 for(auto n:scene->nodes)if(auto mesh=n->mesh){std::vector<uint32_t> ids(mesh->max_face_triangles*3);for(auto face:mesh->faces){auto count=ufbx_triangulate_face(ids.data(),ids.size(),mesh,face);for(size_t i=0;i<count*3;++i)t.corners.push_back({n->typed_id,ids[i]});}}
 std::map<std::pair<uint32_t,uint32_t>,uint16_t> lookup;
 for(auto [node,index]:t.corners){auto key=std::make_pair(node,scene->nodes[node]->mesh->vertex_indices[index]);auto it=lookup.find(key);
  if(it==lookup.end()){if(t.unique.size()>=65535)return false;uint16_t id=uint16_t(t.unique.size());lookup[key]=id;t.unique.push_back(key);t.remap.push_back(id);}else t.remap.push_back(it->second);}
 return true;
}
// One cluster's transform for one sample, as stored (before quantizing).
struct Bone{double t[3],q[4];};
// The cluster's geometry-to-world from its rotation, scale, and where its pivot
// (mesh space) lands in the world.
inline ufbx_matrix matrix(const double at[3],const double q[4],const float s[3],const float pivot[3]){
 ufbx_transform x{};double n=std::sqrt(q[0]*q[0]+q[1]*q[1]+q[2]*q[2]+q[3]*q[3]);if(n<1e-9)n=1;
 x.rotation={q[0]/n,q[1]/n,q[2]/n,q[3]/n};x.scale={s[0],s[1],s[2]};ufbx_matrix m=ufbx_transform_to_matrix(&x);
 const ufbx_vec3 turned=ufbx_transform_position(&m,{pivot[0],pivot[1],pivot[2]});m.cols[3]={at[0]-turned.x,at[1]-turned.y,at[2]-turned.z};return m;
}
namespace detail {
struct Reader{const std::vector<unsigned char>& d;size_t at=0;bool ok=true;
 template<class T> T get(){T v{};if(at+sizeof(T)>d.size()){ok=false;return v;}std::memcpy(&v,d.data()+at,sizeof(T));at+=sizeof(T);return v;}};
}
// The RMA3 bank this RMS1 bank stands for, skinned on scene (the model it was
// baked from, loaded right-handed Y up in metres). Empty, with error set, when
// the bank does not fit the model.
inline std::vector<unsigned char> expand(const ufbx_scene* scene,const std::vector<unsigned char>& bank,std::string& error){
 detail::Reader r{bank};r.at=4;
 const uint32_t corners=r.get<uint32_t>(),total=r.get<uint32_t>(),clips=r.get<uint32_t>(),unique=r.get<uint32_t>(),clusters=r.get<uint32_t>();
 if(!r.ok||!clips||clips>64||!unique||unique>65535||!clusters||clusters>1024){error="skeletal bank header";return {};}
 Topology topo;if(!topology(scene,topo)||topo.corners.size()!=corners||topo.unique.size()!=unique){error="skeletal bank topology";return {};}
 std::vector<uint16_t> remap(corners);for(auto& v:remap)v=r.get<uint16_t>();
 if(!r.ok||remap!=topo.remap){error="skeletal bank corner order";return {};}
 std::vector<uint32_t> node(unique),vertex(unique);for(auto& v:node)v=r.get<uint32_t>();for(auto& v:vertex)v=r.get<uint32_t>();
 std::vector<uint32_t> count(clips);std::vector<uint8_t> loop(clips);for(auto& v:count)v=r.get<uint32_t>();for(auto& v:loop)v=r.get<uint8_t>();
 std::vector<std::array<float,3>> scale(clusters),pivot(clusters);for(auto& s:scale)for(auto& v:s)v=r.get<float>();for(auto& p:pivot)for(auto& v:p)v=r.get<float>();
 if(!r.ok){error="skeletal bank tables";return {};}
 uint32_t sum=0;for(auto c:count){if(c<2||c>512){error="skeletal bank sample count";return {};}sum+=c;}
 if(sum!=total){error="skeletal bank sample total";return {};}
 // Each skinned vertex: its bind position and its clusters' weights, normalized as ufbx skins.
 struct Influence{uint32_t cluster;float weight;};
 std::vector<ufbx_vec3> bind(unique);std::vector<uint32_t> first(unique+1,0);std::vector<Influence> influences;
 for(uint32_t i=0;i<unique;++i){
  if(node[i]!=topo.unique[i].first||vertex[i]!=topo.unique[i].second||node[i]>=scene->nodes.count){error="skeletal bank vertex table";return {};}
  const ufbx_mesh* mesh=scene->nodes[node[i]]->mesh;if(!mesh||mesh->skin_deformers.count!=1||vertex[i]>=mesh->vertices.count){error="skeletal bank needs one skin per mesh";return {};}
  const ufbx_skin_deformer* skin=mesh->skin_deformers[0];if(skin->clusters.count!=clusters){error="skeletal bank cluster count";return {};}
  bind[i]=mesh->vertices[vertex[i]];first[i]=uint32_t(influences.size());
  const auto sv=skin->vertices[vertex[i]];double weight=0;
  for(uint32_t k=0;k<sv.num_weights;++k){const auto w=skin->weights[sv.weight_begin+k];if(!skin->clusters[w.cluster_index]->bone_node)continue;influences.push_back({w.cluster_index,float(w.weight)});weight+=w.weight;}
  if(weight<=0){error="skeletal bank vertex without weights";return {};}
  for(size_t k=first[i];k<influences.size();++k)influences[k].weight=float(influences[k].weight/weight);
 }
 first[unique]=uint32_t(influences.size());
 std::vector<unsigned char> out;out.reserve(20+size_t(corners)*2+size_t(clips)*4+size_t(total)*unique*6);
 auto put=[&](const void* p,size_t n){const auto* b=static_cast<const unsigned char*>(p);out.insert(out.end(),b,b+n);};
 put("RMA3",4);for(uint32_t v:{corners,total,clips,unique})put(&v,4);put(remap.data(),remap.size()*2);put(count.data(),count.size()*4);
 // The pivots' first positions, then every change as one stream of 16-bit
 // values (zigzagged, low bytes then high bytes, which packs tighter).
 std::vector<int32_t> firstAt(size_t(clips)*clusters*3);for(auto& v:firstAt)v=r.get<int32_t>();
 const uint32_t changes=r.get<uint32_t>();if(!r.ok||r.at+size_t(changes)*2>bank.size()){error="skeletal bank truncated";return {};}
 std::vector<int16_t> stream(changes);
 for(uint32_t i=0;i<changes;++i){const uint16_t z=uint16_t(bank[r.at+i]|(bank[r.at+changes+i]<<8));stream[i]=int16_t((z>>1)^(0u-(z&1u)));}
 r.at+=size_t(changes)*2;size_t next=0;
 std::vector<ufbx_matrix> world(clusters);std::vector<std::vector<ufbx_vec3>> frames;
 for(uint32_t c=0;c<clips;++c){const uint32_t n=count[c];
  // The clip's planes, undone from changes into values.
  if(next+size_t(clusters)*(4*n+3*(n-1))>stream.size()){error="skeletal bank truncated";return {};}
  std::vector<int16_t> q(size_t(clusters)*4*n);std::vector<int32_t> t(size_t(clusters)*3*n);
  for(uint32_t k=0;k<clusters;++k){
   for(int a=0;a<4;++a){int16_t v=0;for(uint32_t s=0;s<n;++s){v=int16_t(v+stream[next++]);q[(size_t(k)*4+a)*n+s]=v;}}
   for(int a=0;a<3;++a){int32_t v=firstAt[(size_t(c)*clusters+k)*3+size_t(a)];t[(size_t(k)*3+a)*n]=v;for(uint32_t s=1;s<n;++s){v+=stream[next++];t[(size_t(k)*3+a)*n+s]=v;}}}
  frames.assign(n,std::vector<ufbx_vec3>(unique));
  for(uint32_t s=0;s<n;++s){
   for(uint32_t k=0;k<clusters;++k){double tq[4],tt[3];for(int a=0;a<4;++a)tq[a]=q[(size_t(k)*4+a)*n+s]/kQuat;for(int a=0;a<3;++a)tt[a]=t[(size_t(k)*3+a)*n+s]/kMetre;world[k]=matrix(tt,tq,scale[k].data(),pivot[k].data());}
   double floor=1e9;auto& frame=frames[s];
   for(uint32_t i=0;i<unique;++i){ufbx_matrix m{};for(uint32_t k=first[i];k<first[i+1];++k){const auto& g=world[influences[k].cluster];const double w=influences[k].weight;
     for(int e=0;e<12;++e)m.v[e]+=g.v[e]*w;}
    frame[i]=ufbx_transform_position(&m,bind[i]);floor=std::min(floor,double(frame[i].y));}
   for(auto& p:frame)p.y-=floor;}
  if(loop[c]){const auto head=frames.front(),tail=frames.back();const double last=double(n-1);
   for(uint32_t f=0;f<n;++f){const double w=std::clamp((double(f)-last*.8)/(last*.2),0.,1.);
    for(uint32_t i=0;i<unique;++i){auto& p=frames[f][i];p.x+=w*(head[i].x-tail[i].x);p.y+=w*(head[i].y-tail[i].y);p.z+=w*(head[i].z-tail[i].z);}}}
  for(const auto& frame:frames)for(const auto& p:frame)for(double v:{p.x,p.y,p.z}){if(std::fabs(v)>32.7){error="skeletal bank vertex out of range";return {};}const int16_t m=int16_t(std::lround(v*1000));put(&m,2);}
 }
 if(r.at!=bank.size()||next!=stream.size()){error="skeletal bank trailing bytes";return {};}
 return out;
}
}
