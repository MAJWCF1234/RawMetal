#include "SoftwareRenderer.h"
#include "GpuRenderer.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <string>

namespace retro {

// Painterly Horizon LOD, capture stage. Each repeated cutout model (trees,
// grass, bushes, ferns) is rendered once on the CPU from 8 azimuths at 3
// elevations into one atlas. The atlas keeps the model's own colours (scene
// lighting is applied at draw time), a little baked volume (front of the
// canopy lighter than its core) and is simplified into broad painted colour
// masses before the GPU mip chain takes over with distance. Every instance
// shares it; scene.vert turns the quad to the eye and picks the views.
namespace {
constexpr float ImpostorElevations[]={.0698f,.5585f,1.0472f}; // 4, 32, 60 degrees; scene.vert assumes this spacing
Point3 cross3(Point3 a,Point3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
float channel(std::uint32_t c,int shift){return float((c>>shift)&255u);}
std::uint32_t pack(float r,float g,float b,std::uint32_t alpha){
 auto c=[](float v){return std::uint32_t(std::clamp(v,0.f,255.f)+.5f);};
 return (alpha<<24)|(c(r)<<16)|(c(g)<<8)|c(b);
}
}

const SoftwareRenderer::Impostor* SoftwareRenderer::impostorFor(int model){
 auto found=m_impostors.find(model);
 if(found!=m_impostors.end())return found->second.get();
 auto& slot=m_impostors[model];
 if(model<FirstLibraryModel||model>=FacilityModelCount)return nullptr;
 const auto& mesh=m_facilityMeshes[size_t(model)];
 if(mesh.triangles.empty())return nullptr;
 // Only cutout foliage benefits: opaque library models are already cheaper
 // as their own few hundred triangles than as four texture fetches a pixel.
 // Foliage means most triangles carry a cutout texture (a bus with glass does not).
 std::unordered_map<int,bool> cutoutPart;size_t cutoutTriangles=0;
 for(const auto& face:mesh.triangles){
  auto part=cutoutPart.find(face.part);
  if(part==cutoutPart.end()){const auto& texture=facilityTexture(model,face.part);size_t clear=0;
   for(auto p:texture.pixels)clear+=(p>>24)<128u;
   part=cutoutPart.emplace(face.part,!texture.pixels.empty()&&clear*20>texture.pixels.size()).first;}
  cutoutTriangles+=part->second;
 }
 if(cutoutTriangles*2<mesh.triangles.size())return nullptr;

 auto impostor=std::make_unique<Impostor>();
 constexpr int Cell=ImpostorCell,Columns=ImpostorColumns,Rows=ImpostorRows;
 impostor->center=(mesh.minimum+mesh.maximum)*.5f;
 auto span=mesh.maximum-mesh.minimum;
 impostor->radius=.5f*std::sqrt(span.x*span.x+span.y*span.y+span.z*span.z);
 auto& atlas=impostor->atlas;
 atlas.width=Columns*Cell;atlas.height=Rows*Cell;atlas.pixels.assign(size_t(atlas.width)*atlas.height,0u);
 std::vector<float> depth(atlas.pixels.size(),-1e30f);
 // Per texel: the surface normal in the model frame (X, Z, Y) and its depth
 // along the view, so the shader can light the shape and reproject views.
 std::vector<Point3> normals(atlas.pixels.size(),Point3{0,0,1});
 const float radius=impostor->radius,toPixels=Cell/(2*radius);
 for(int row=0;row<Rows;++row)for(int column=0;column<Columns;++column){
  // Mesh frame as (X, Z, Y): the facility transform maps mesh X and Z to the
  // ground plane and Y up, so this matches scene.vert's model-space eye vector.
  float elevation=ImpostorElevations[row],azimuth=column*6.2831853f/Columns;
  Point3 d{std::cos(elevation)*std::cos(azimuth),std::cos(elevation)*std::sin(azimuth),std::sin(elevation)};
  Point3 right{-d.y,d.x,0};float rl=std::sqrt(right.x*right.x+right.y*right.y);right=right*(1.f/rl);
  Point3 up=cross3(right,d*-1.f);
  int ox=column*Cell,oy=row*Cell;
  for(const auto& face:mesh.triangles){
   const auto& texture=facilityTexture(model,face.part);
   std::array<Point3,3> s{};
   for(int k=0;k<3;++k){auto q=face.v[k].p-impostor->center;Point3 w{q.x,q.z,q.y};
    s[size_t(k)]={Cell*.5f+(w.x*right.x+w.y*right.y+w.z*right.z)*toPixels,Cell*.5f-(w.x*up.x+w.y*up.y+w.z*up.z)*toPixels,w.x*d.x+w.y*d.y+w.z*d.z};}
   float area=(s[1].x-s[0].x)*(s[2].y-s[0].y)-(s[2].x-s[0].x)*(s[1].y-s[0].y);
   if(std::fabs(area)<1e-6f)continue;
   // Broad shading baked from the face's tilt only: tops a touch lighter.
   auto e1=face.v[1].p-face.v[0].p,e2=face.v[2].p-face.v[0].p;auto n=cross3(e1,e2);
   float nl=std::sqrt(n.x*n.x+n.y*n.y+n.z*n.z),tilt=nl>0?std::fabs(n.y)/nl:0.f,shade=.9f+.1f*tilt;
   Point3 faceNormal=nl>0?Point3{n.x/nl,n.z/nl,n.y/nl}:Point3{0,0,1};
   // Two-sided cards: the recorded normal faces the camera that saw it.
   if(faceNormal.x*d.x+faceNormal.y*d.y+faceNormal.z*d.z<0)faceNormal=faceNormal*-1.f;
   int x0=std::max(0,int(std::floor(std::min({s[0].x,s[1].x,s[2].x})))),x1=std::min(Cell-1,int(std::ceil(std::max({s[0].x,s[1].x,s[2].x}))));
   int y0=std::max(0,int(std::floor(std::min({s[0].y,s[1].y,s[2].y})))),y1=std::min(Cell-1,int(std::ceil(std::max({s[0].y,s[1].y,s[2].y}))));
   for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){
    float px=x+.5f,py=y+.5f;
    float w0=((s[1].x-px)*(s[2].y-py)-(s[2].x-px)*(s[1].y-py))/area,w1=((s[2].x-px)*(s[0].y-py)-(s[0].x-px)*(s[2].y-py))/area,w2=1-w0-w1;
    if(w0<0||w1<0||w2<0)continue;
    float z=w0*s[0].z+w1*s[1].z+w2*s[2].z;size_t index=size_t(oy+y)*atlas.width+size_t(ox+x);
    if(z<=depth[index])continue;
    float u=w0*face.v[0].u+w1*face.v[1].u+w2*face.v[2].u,v=w0*face.v[0].v+w1*face.v[1].v+w2*face.v[2].v;
    auto texel=sample(texture,u,v,0);
    if((texel>>24)<128u)continue;
    depth[index]=z;normals[index]=faceNormal;
    atlas.pixels[index]=pack(channel(texel,16)*shade,channel(texel,8)*shade,channel(texel,0)*shade,255u);
   }
  }
  // Volume: the near side of the mass reads lighter than its core.
  for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x){size_t index=size_t(oy+y)*atlas.width+size_t(ox+x);auto c=atlas.pixels[index];
   if(!(c>>24))continue;float t=std::clamp((depth[index]+radius)/(2*radius),0.f,1.f),k=.78f+.3f*t;
   atlas.pixels[index]=pack(channel(c,16)*k,channel(c,8)*k,channel(c,0)*k,255u);}
  // Painted masses: a small Kuwahara filter keeps silhouettes and merges
  // leaf-scale noise into flat strokes of the quietest neighbouring colour.
  std::vector<std::uint32_t> cell(size_t(Cell)*Cell);
  for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x)cell[size_t(y)*Cell+size_t(x)]=atlas.pixels[size_t(oy+y)*atlas.width+size_t(ox+x)];
  for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x){auto c=cell[size_t(y)*Cell+size_t(x)];if(!(c>>24))continue;
   float best=1e30f,br=0,bg=0,bb=0;
   for(int qy=-1;qy<=0;++qy)for(int qx=-1;qx<=0;++qx){float r=0,g=0,b=0,rr=0,gg=0,bb2=0;int count=0;
    for(int j=0;j<3;++j)for(int i=0;i<3;++i){int sx=x+qx*2+i,sy=y+qy*2+j;if(sx<0||sy<0||sx>=Cell||sy>=Cell)continue;auto p=cell[size_t(sy)*Cell+size_t(sx)];if(!(p>>24))continue;
     float cr=channel(p,16),cg=channel(p,8),cb=channel(p,0);r+=cr;g+=cg;b+=cb;rr+=cr*cr;gg+=cg*cg;bb2+=cb*cb;++count;}
    if(count<3)continue;float inv=1.f/count,variance=rr*inv-r*r*inv*inv+gg*inv-g*g*inv*inv+bb2*inv-b*b*inv*inv;
    if(variance<best){best=variance;br=r*inv;bg=g*inv;bb=b*inv;}}
   if(best<1e30f)atlas.pixels[size_t(oy+y)*atlas.width+size_t(ox+x)]=pack(br,bg,bb,255u);}
  // Dilate colour into the clear texels so mips never pull in black fringes.
  for(int pass=0;pass<6;++pass){std::vector<std::uint32_t> next(size_t(Cell)*Cell);
   for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x)next[size_t(y)*Cell+size_t(x)]=atlas.pixels[size_t(oy+y)*atlas.width+size_t(ox+x)];
   for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x){auto& target=next[size_t(y)*Cell+size_t(x)];if(target&0xffffffu)continue;
    float r=0,g=0,b=0;int count=0;
    for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i){int sx=x+i,sy=y+j;if(sx<0||sy<0||sx>=Cell||sy>=Cell)continue;auto p=atlas.pixels[size_t(oy+sy)*atlas.width+size_t(ox+sx)];if(!(p&0xffffffu))continue;r+=channel(p,16);g+=channel(p,8);b+=channel(p,0);++count;}
    if(count)target=pack(r/count,g/count,b/count,0u)&0x00ffffffu;}
   for(int y=0;y<Cell;++y)for(int x=0;x<Cell;++x)atlas.pixels[size_t(oy+y)*atlas.width+size_t(ox+x)]=next[size_t(y)*Cell+size_t(x)];}
 }
 atlas.normalLevels.push_back(std::move(normals));
 atlas.relief.resize(atlas.pixels.size());
 for(size_t i=0;i<atlas.pixels.size();++i)atlas.relief[i]=depth[i]>-1e29f?std::uint8_t(std::clamp((depth[i]/radius*.5f+.5f)*255.f,1.f,255.f)):std::uint8_t(128);
 if(m_gpu)m_gpu->prepare(atlas);
 slot=std::move(impostor);
 return slot.get();
}

void SoftwareRenderer::saveImpostors(const std::string& prefix)const{
 for(const auto& [model,impostor]:m_impostors){if(!impostor)continue;const auto& atlas=impostor->atlas;
  std::ofstream out(prefix+"-"+std::to_string(model)+".ppm",std::ios::binary);out<<"P6\n"<<atlas.width<<' '<<atlas.height<<"\n255\n";
  // Clear texels show as magenta so coverage is visible.
  for(auto p:atlas.pixels){bool clear=(p>>24)<128u;char rgb[]={char(clear?255:p>>16),char(clear?0:p>>8),char(clear?255:p)};out.write(rgb,3);}}
}

}
