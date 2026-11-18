#pragma once
// Underground (CAVE records): tunnels and caverns carved into the campaign's
// terrain, filled back, pushed and pulled, smoothed and floored. The terrain is
// a density field (32nds: +32 rock, -32 air) that Surface Nets skins and the
// collision walks (World::buildTerrain, terrainSurfaceBelow/Above), so a cave is
// only a change to that field: the walls, the floor, the roof, the mouth where a
// tunnel breaks the hillside and the collision all come from the same samples.
//
//   CAVE|op|x1|y1|z1|x2|y2|z2|radius[|rough[|strength]]   (world metres, campaign-wide)
//
//   carve    a tunnel from (x1,y1,z1) to (x2,y2,z2), `radius` round, rough-walled,
//            its floor flat 0.6 radius under the line (a point when both ends meet:
//            a cavern);
//   fill     rock back into the same shape;
//   move     the rock and air round (x1,y1,z1) pulled to (x2,y2,z2), falling off
//            over `radius` (the Move brush: the wall follows the hand);
//   smooth   the field round (x1,y1,z1) evened out over `radius`;
//   flatten  a level floor at z1 round (x1,y1): rock below, air above, in `radius`.
//
// `rough` (0-1, default 0.35) roughens the walls with seeded 3D noise; `strength`
// (0-1, default 1) is how far a move, smooth or flatten goes. Strokes apply in the
// order written, each to the field the ones before it left, over world-metre
// samples: buildCaveVolume works them out once for the whole campaign, so every
// chunk samples one continuous underground and the Campaign Builder the same one.
//
// Header-only and free of the World (tools/Level Editor/editor reads it too).
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace retro {
enum CaveOp:int {CaveCarve,CaveFill,CaveMove,CaveSmooth,CaveFlatten,CaveOps};
inline const char* const caveOpWords[CaveOps]={"carve","fill","move","smooth","flatten"};
inline int caveOpFromWord(const std::string& w){for(int i=0;i<CaveOps;++i)if(w==caveOpWords[i])return i;return -1;}
struct CaveStroke {
 int op=CaveCarve;float x1=0,y1=0,z1=0,x2=0,y2=0,z2=0,radius=2,rough=.35f,strength=1;
 bool operator==(const CaveStroke&)const=default;
};
inline constexpr int CaveMaxStrokes=4096;
inline constexpr float CaveMinRadius=.5f,CaveMaxRadius=8.f;
// The samples a cave may change: never the bottom two rows (-13 and -12), so the
// field always closes under the deepest floor, and nothing over the terrain's top.
inline constexpr int CaveLowestZ=-11,CaveHighestZ=20,CaveFieldBottom=-13;

// The field after every stroke, over the box the strokes reach (world metres,
// one sample a metre, like the engine's terrain samples). `touched` marks the
// samples a stroke changed: they are drawn as cave rock.
struct CaveVolume {
 int x0=0,y0=0,z0=0,nx=0,ny=0,nz=0;
 std::vector<std::int8_t> density;
 std::vector<std::uint8_t> touched;
 bool contains(int x,int y,int z)const{return x>=x0&&y>=y0&&z>=z0&&x<x0+nx&&y<y0+ny&&z<z0+nz;}
 std::size_t at(int x,int y,int z)const{return std::size_t((std::size_t(z-z0)*std::size_t(ny)+std::size_t(y-y0))*std::size_t(nx)+std::size_t(x-x0));}
};

// Seeded 3D value noise (-1..1), two octaves, a few metres across.
inline float caveLattice(int x,int y,int z){
 std::uint32_t h=std::uint32_t(x)*0x8DA6B343u^std::uint32_t(y)*0xD8163841u^std::uint32_t(z)*0xCB1AB31Fu^0x5F356495u;
 h^=h>>15;h*=0x2C1B3C6Du;h^=h>>12;h*=0x297A2D39u;h^=h>>15;return float(h&0xffffff)/float(0xffffff)*2.f-1.f;
}
inline float caveValueNoise(float x,float y,float z){
 const int ix=int(std::floor(x)),iy=int(std::floor(y)),iz=int(std::floor(z));
 auto fade=[](float t){return t*t*t*(t*(t*6-15)+10);};const float u=fade(x-ix),v=fade(y-iy),w=fade(z-iz);
 auto lerp=[](float a,float b,float t){return a+(b-a)*t;};
 auto plane=[&](int k){return lerp(lerp(caveLattice(ix,iy,k),caveLattice(ix+1,iy,k),u),lerp(caveLattice(ix,iy+1,k),caveLattice(ix+1,iy+1,k),u),v);};
 return lerp(plane(iz),plane(iz+1),w);
}
inline float caveNoise(float x,float y,float z){return caveValueNoise(x*.38f,y*.38f,z*.38f)*.7f+caveValueNoise(x*.91f+17.3f,y*.91f-4.1f,z*.91f+9.7f)*.3f;}

// The world box (inclusive whole metres) a stroke can change or read.
struct CaveBounds {int x0,y0,z0,x1,y1,z1;};
inline CaveBounds caveStrokeBounds(const CaveStroke& s){
 const float r=s.radius*(1+std::max(0.f,s.rough))+1.5f;
 float lx=std::min(s.x1,s.x2),hx=std::max(s.x1,s.x2),ly=std::min(s.y1,s.y2),hy=std::max(s.y1,s.y2),lz=std::min(s.z1,s.z2),hz=std::max(s.z1,s.z2);
 if(s.op==CaveSmooth||s.op==CaveFlatten){lx=hx=s.x1;ly=hy=s.y1;lz=hz=s.z1;}
 return {int(std::floor(lx-r)),int(std::floor(ly-r)),std::max(CaveFieldBottom,int(std::floor(lz-r))),int(std::ceil(hx+r)),int(std::ceil(hy+r)),std::min(CaveHighestZ,int(std::ceil(hz+r)))};
}
inline bool caveBoundsMeet(const CaveBounds& b,float x0,float y0,float x1,float y1){return b.x1>=x0&&b.x0<=x1&&b.y1>=y0&&b.y0<=y1;}

// Signed distance (metres, negative inside the air) of a carve: the rough tube
// round the line, cut level 0.6 radius under it.
inline float caveCarveDistance(const CaveStroke& s,float px,float py,float pz){
 const float dx=s.x2-s.x1,dy=s.y2-s.y1,dz=s.z2-s.z1,len2=dx*dx+dy*dy+dz*dz;
 float t=len2>1e-6f?std::clamp(((px-s.x1)*dx+(py-s.y1)*dy+(pz-s.z1)*dz)/len2,0.f,1.f):0.f;
 const float cx=s.x1+dx*t,cy=s.y1+dy*t,cz=s.z1+dz*t;
 const float n=caveNoise(px,py,pz);
 const float tube=std::sqrt((px-cx)*(px-cx)+(py-cy)*(py-cy)+(pz-cz)*(pz-cz))-s.radius*(1+s.rough*.45f*n);
 const float floor=(cz-s.radius*.6f+s.rough*.12f*n)-pz;
 return std::max(tube,floor);
}
inline float caveSmoothStep(float t){t=std::clamp(t,0.f,1.f);return t*t*(3-2*t);}

// Works the strokes out over `base`, the terrain's own density at a world sample
// (32nds). Returns null when there are no strokes.
inline std::shared_ptr<const CaveVolume> buildCaveVolume(const std::vector<CaveStroke>& strokes,const std::function<int(int,int,int)>& base){
 if(strokes.empty())return nullptr;
 CaveBounds all=caveStrokeBounds(strokes.front());
 for(const auto& s:strokes){const auto b=caveStrokeBounds(s);all={std::min(all.x0,b.x0),std::min(all.y0,b.y0),std::min(all.z0,b.z0),std::max(all.x1,b.x1),std::max(all.y1,b.y1),std::max(all.z1,b.z1)};}
 auto v=std::make_shared<CaveVolume>();
 v->x0=all.x0-1;v->y0=all.y0-1;v->z0=CaveFieldBottom;v->nx=all.x1-all.x0+3;v->ny=all.y1-all.y0+3;v->nz=std::max(1,std::min(CaveHighestZ,all.z1+1)-CaveFieldBottom+1);
 if(std::size_t(v->nx)*std::size_t(v->ny)*std::size_t(v->nz)>24u*1024u*1024u)return nullptr; // far too big: leave the land as it is
 const std::size_t count=std::size_t(v->nx)*v->ny*v->nz;
 std::vector<float> field(count);v->touched.assign(count,0);
 for(int z=0;z<v->nz;++z)for(int y=0;y<v->ny;++y)for(int x=0;x<v->nx;++x)field[v->at(v->x0+x,v->y0+y,v->z0+z)]=float(base(v->x0+x,v->y0+y,v->z0+z))/32.f;
 auto inField=[&](int x,int y,int z){return v->contains(x,y,z);};
 std::vector<float> before;
 for(const auto& s:strokes){
  const auto b=caveStrokeBounds(s);
  const int zLo=std::max(b.z0,CaveLowestZ),zHi=std::min(b.z1,CaveHighestZ);
  const bool reads=s.op==CaveMove||s.op==CaveSmooth;
  if(reads)before=field;
  auto from=[&](int x,int y,int z){x=std::clamp(x,v->x0,v->x0+v->nx-1);y=std::clamp(y,v->y0,v->y0+v->ny-1);z=std::clamp(z,v->z0,v->z0+v->nz-1);return before[v->at(x,y,z)];};
  const float strength=std::clamp(s.strength,0.f,1.f);
  for(int z=zLo;z<=zHi;++z)for(int y=b.y0;y<=b.y1;++y)for(int x=b.x0;x<=b.x1;++x){
   if(!inField(x,y,z))continue;
   float& d=field[v->at(x,y,z)];const float old=d;
   const float px=float(x),py=float(y),pz=float(z);
   switch(s.op){
    case CaveCarve:d=std::min(d,std::clamp(caveCarveDistance(s,px,py,pz),-1.f,1.f));break;
    case CaveFill:d=std::max(d,std::clamp(-caveCarveDistance(s,px,py,pz),-1.f,1.f));break;
    case CaveMove:{
     const float mx=s.x2-s.x1,my=s.y2-s.y1,mz=s.z2-s.z1;
     const float dist=std::sqrt((px-s.x2)*(px-s.x2)+(py-s.y2)*(py-s.y2)+(pz-s.z2)*(pz-s.z2));
     const float w=caveSmoothStep(1-dist/std::max(.5f,s.radius))*strength;if(w<=0)break;
     // Reads the field before this stroke: what stood w of the pull back along it.
     auto gb=[&](float qx,float qy,float qz){const int ix=int(std::floor(qx)),iy=int(std::floor(qy)),iz=int(std::floor(qz));const float tx=qx-ix,ty=qy-iy,tz=qz-iz;
      auto l=[](float a,float c,float t){return a+(c-a)*t;};
      return l(l(l(from(ix,iy,iz),from(ix+1,iy,iz),tx),l(from(ix,iy+1,iz),from(ix+1,iy+1,iz),tx),ty),l(l(from(ix,iy,iz+1),from(ix+1,iy,iz+1),tx),l(from(ix,iy+1,iz+1),from(ix+1,iy+1,iz+1),tx),ty),tz);};
     d=gb(px-mx*w,py-my*w,pz-mz*w);break;}
    case CaveSmooth:{
     const float dist=std::sqrt((px-s.x1)*(px-s.x1)+(py-s.y1)*(py-s.y1)+(pz-s.z1)*(pz-s.z1));
     const float w=caveSmoothStep(1-dist/std::max(.5f,s.radius))*strength*.85f;if(w<=0)break;
     float sum=0;for(int k=-1;k<=1;++k)for(int j=-1;j<=1;++j)for(int i=-1;i<=1;++i)sum+=from(x+i,y+j,z+k);
     d=old+(sum/27.f-old)*w;break;}
    case CaveFlatten:{
     const float dist=std::sqrt((px-s.x1)*(px-s.x1)+(py-s.y1)*(py-s.y1)+(pz-s.z1)*(pz-s.z1)*.25f);
     const float w=caveSmoothStep(1-dist/std::max(.5f,s.radius))*strength;if(w<=0)break;
     d=old+(std::clamp(s.z1-pz,-1.f,1.f)-old)*w;break;}
    default:break;
   }
   if(std::abs(d-old)>1e-4f)v->touched[v->at(x,y,z)]=1;
  }
 }
 v->density.resize(count);
 for(std::size_t i=0;i<count;++i)v->density[i]=std::int8_t(std::lround(std::clamp(field[i],-1.f,1.f)*32.f));
 return v;
}

// CAVE fields as text and back (the record after its tag).
inline std::string caveNumber(float v){char b[32];std::snprintf(b,sizeof b,"%.2f",double(v));std::string s=b;while(!s.empty()&&s.back()=='0')s.pop_back();if(!s.empty()&&s.back()=='.')s.pop_back();return s=="-0"||s.empty()?"0":s;}
inline std::vector<std::string> caveFields(const CaveStroke& s){
 std::vector<std::string> f{"CAVE",caveOpWords[std::clamp(s.op,0,CaveOps-1)],caveNumber(s.x1),caveNumber(s.y1),caveNumber(s.z1),caveNumber(s.x2),caveNumber(s.y2),caveNumber(s.z2),caveNumber(s.radius)};
 if(s.rough!=.35f||s.strength!=1.f)f.push_back(caveNumber(s.rough));
 if(s.strength!=1.f)f.push_back(caveNumber(s.strength));
 return f;
}
}
