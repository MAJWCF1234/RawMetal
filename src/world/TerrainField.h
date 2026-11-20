#pragma once
// Campaign-authored outdoor terrain (TERRAIN, TERRAIN_FLAT, TERRAIN_SCULPT and
// GROUND records): the shape and the height and cover it gives every world
// metre. A seeded fractal heightfield in world space, so every stitched chunk
// samples one continuous surface. Flats level pads, roads and building sites,
// blending back into the hills over `blend` metres; sculpt grids then raise or
// lower single metres. Heights are 1 m voxel tops.
//
// Header-only and free of the World so the Campaign Builder samples the very
// same ground the engine builds (tools/Level Editor/editor).
#include "CaveField.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

namespace retro {
struct TerrainFlat {float x1=0,y1=0,x2=0,y2=0,height=2,blend=4;int pad=0;};
// A footing's pad (Footing.h; never written as a record): the ground over world columns
// x1..x2, y1..y2 (whole metres, both ends included) is walked at exactly `height`, not at a
// voxel top. Level cuts and fills to it; Cut only lowers ground above it. Its skirt blends
// back into the hills over `blend` metres like a TERRAIN_FLAT's.
enum TerrainPad:int {TerrainPadNone,TerrainPadLevel,TerrainPadCut};
// Ground cover painted over authored terrain: one texture kind per rectangle
// (world metres), with a noisy border `edge` metres wide.
enum GroundKind:int {GroundShortGrass,GroundLeafyGrass,GroundClover,GroundWild,GroundDry,GroundLitter,GroundGravel,GroundMud,GroundKinds,GroundMeadow=GroundKinds};
struct GroundPaint {float x1=0,y1=0,x2=0,y2=0;int kind=GroundMeadow;float edge=1.5f;};
// TERRAIN_SCULPT|x|y|cols|rows|offsets: a grid of height offsets added to the
// ground after the flats, one per world metre cell from (x, y), row by row.
// The offset of cell (i, j) holds at its centre (x+i+.5, y+j+.5) and blends
// to its neighbours' between centres. Offsets are written in decimetres.
struct TerrainSculpt {int x=0,y=0,cols=0,rows=0;std::vector<float> offsets;};
struct TerrainShape {
    std::uint32_t seed=1;
    float base=2,amplitude=3,scale=36,roughness=.45f;
    std::vector<TerrainFlat> flats;
    std::vector<TerrainSculpt> sculpts;
    // Any GROUND record turns on painted cover: a noise meadow of grass, clover,
    // wild and dry ground everywhere, then the rectangles in order.
    std::vector<GroundPaint> ground;
    // HORIZON: unreachable land around the playable maps, rising `rise` metres
    // over `depth` metres and wooded every `spacing` metres with these models
    // (drawn only as Painterly Horizon LOD impostors).
    float horizonDepth=0,horizonSpacing=6,horizonRise=0;std::vector<int> horizonModels;
    // The distance the map is meant to be seen at, metres (0: the engine's own, Game::viewDistance).
    float viewDistance=0;
    // CAVE records (CaveField.h), in the order written, and the field they leave in the
    // land (buildTerrainCaves, once the flats, pads and sculpts are all in).
    std::vector<CaveStroke> caves;
    std::shared_ptr<const CaveVolume> caveVolume;
};
// The voxel tops terrain may take (World::TerrainMinZ + 1 to TerrainMaxZ - 2).
inline constexpr int TerrainFieldLowestTop=-11,TerrainFieldHighestTop=18;
inline constexpr int TerrainSculptMaxCells=4096;
inline constexpr float TerrainSculptMaxOffset=30.f;

// Smooth value noise: a hashed lattice, quintic-blended, summed over octaves.
inline float terrainLatticeValue(std::uint32_t seed,int x,int y){
 std::uint32_t h=seed*0x9E3779B1u^std::uint32_t(x)*0x85EBCA77u^std::uint32_t(y)*0xC2B2AE3Du;
 h^=h>>15;h*=0x2C1B3C6Du;h^=h>>12;h*=0x297A2D39u;h^=h>>15;return float(h&0xffffff)/float(0xffffff)*2.f-1.f;
}
inline float terrainValueNoise(std::uint32_t seed,float x,float y){
 int ix=int(std::floor(x)),iy=int(std::floor(y));float fx=x-ix,fy=y-iy;
 auto fade=[](float t){return t*t*t*(t*(t*6-15)+10);};float u=fade(fx),v=fade(fy);
 float a=terrainLatticeValue(seed,ix,iy),b=terrainLatticeValue(seed,ix+1,iy),c=terrainLatticeValue(seed,ix,iy+1),d=terrainLatticeValue(seed,ix+1,iy+1);
 return (a*(1-u)+b*u)*(1-v)+(c*(1-u)+d*u)*v;
}
// The sculpted offset of one world metre cell (0 where no grid covers it).
inline float terrainSculptCell(const TerrainShape& shape,int cx,int cy){
 for(auto it=shape.sculpts.rbegin();it!=shape.sculpts.rend();++it){const auto& s=*it;
  if(cx>=s.x&&cy>=s.y&&cx<s.x+s.cols&&cy<s.y+s.rows)return s.offsets[size_t((cy-s.y)*s.cols+(cx-s.x))];}
 return 0;
}
// Sculpted offset at any world point: the cell centres' offsets, bilinear.
inline float terrainSculptOffset(const TerrainShape& shape,float wx,float wy){
 if(shape.sculpts.empty())return 0;
 float px=wx-.5f,py=wy-.5f;int ix=int(std::floor(px)),iy=int(std::floor(py));float tx=px-ix,ty=py-iy;
 float a=terrainSculptCell(shape,ix,iy),b=terrainSculptCell(shape,ix+1,iy),c=terrainSculptCell(shape,ix,iy+1),d=terrainSculptCell(shape,ix+1,iy+1);
 return (a*(1-tx)+b*tx)*(1-ty)+(c*(1-tx)+d*tx)*ty;
}
// The noise and the flats, before any sculpting.
inline float authoredTerrainBaseHeight(const TerrainShape& shape,float wx,float wy){
 float sum=0,amp=1,freq=1/std::max(4.f,shape.scale),norm=0;
 for(int octave=0;octave<4;++octave){sum+=terrainValueNoise(shape.seed+std::uint32_t(octave)*101u,wx*freq,wy*freq)*amp;norm+=amp;amp*=shape.roughness;freq*=2.03f;}
 float h=shape.base+shape.amplitude*sum/norm;
 for(const auto& flat:shape.flats){
  float dx=std::max({flat.x1-wx,0.f,wx-flat.x2}),dy=std::max({flat.y1-wy,0.f,wy-flat.y2});
  float d=std::sqrt(dx*dx+dy*dy),t=flat.blend>0?std::clamp(1-d/flat.blend,0.f,1.f):(d<=0?1.f:0.f);
  t=t*t*(3-2*t);
  // A pad's voxels sit half a metre under the height walked on them (a voxel top k is walked at k + .5).
  const float target=flat.pad?flat.height-.5f:flat.height,blended=h+(target-h)*t;
  h=flat.pad==TerrainPadCut?std::min(h,blended):blended;
 }
 return h;
}
// The continuous (unquantized) surface height of an authored terrain shape.
inline float authoredTerrainHeight(const TerrainShape& shape,float wx,float wy){
 return authoredTerrainBaseHeight(shape,wx,wy)+terrainSculptOffset(shape,wx,wy);
}
// The voxel top of world metre cell (wx, wy): solid ground fills up to it.
inline int authoredTerrainTop(const TerrainShape& shape,int wx,int wy){
 return std::clamp(int(std::lround(authoredTerrainHeight(shape,wx+.5f,wy+.5f))),TerrainFieldLowestTop,TerrainFieldHighestTop);
}
// The pad covering world column (wx, wy), the last one written winning, or null.
inline const TerrainFlat* terrainPadAt(const TerrainShape& shape,int wx,int wy){
 for(auto it=shape.flats.rbegin();it!=shape.flats.rend();++it)
  if(it->pad&&wx>=it->x1-.001f&&wx<=it->x2+.001f&&wy>=it->y1-.001f&&wy<=it->y2+.001f)return &*it;
 return nullptr;
}
// The ground's density at voxel z of world column (wx, wy), in 32nds (the engine's
// sample, std::int8_t): +32 solid, -32 air. A voxel column is solid up to its top; a
// pad's column crosses zero exactly at the pad's height, so Surface Nets puts the
// ground there (and terrainSurfaceBelow walks it there) to the 32nd of a metre.
inline int terrainColumnDensity(const TerrainShape& shape,int wx,int wy,int z){
 const int top=authoredTerrainTop(shape,wx,wy);
 if(const TerrainFlat* pad=terrainPadAt(shape,wx,wy);pad&&(pad->pad==TerrainPadLevel||pad->height<float(top)+.5f)){
  const float h=std::clamp(pad->height,float(TerrainFieldLowestTop)-.5f,float(TerrainFieldHighestTop)+.5f);
  return int(std::lround(std::clamp(h-float(z),-1.f,1.f)*32.f));}
 return z<=top?32:-32;
}
// The land's density at a world sample with the underground carved into it.
inline int terrainDensityWithCaves(const TerrainShape& shape,int wx,int wy,int z){
 if(const auto& v=shape.caveVolume;v&&v->contains(wx,wy,z))return v->density[v->at(wx,wy,z)];
 return terrainColumnDensity(shape,wx,wy,z);
}
// Whether a CAVE stroke changed this sample (it is drawn as cave rock).
inline bool terrainCaveTouched(const TerrainShape& shape,int wx,int wy,int z){
 const auto& v=shape.caveVolume;return v&&v->contains(wx,wy,z)&&v->touched[v->at(wx,wy,z)];
}
// The floor of the first carved space under the land's top over world column (wx, wy), to the
// samples' linear crossing; the land's top where nothing under it is carved.
inline float terrainCaveFloor(const TerrainShape& shape,int wx,int wy){
 const int top=authoredTerrainTop(shape,wx,wy);
 if(!shape.caveVolume)return float(top)+.5f;
 bool rock=false;
 for(int z=std::min(top,CaveHighestZ);z>CaveFieldBottom;--z){
  if(terrainDensityWithCaves(shape,wx,wy,z)>0){rock=true;continue;}
  if(!rock)continue;
  for(int k=z-1;k>=CaveFieldBottom;--k){const int below=terrainDensityWithCaves(shape,wx,wy,k);
   if(below>0){const int above=terrainDensityWithCaves(shape,wx,wy,k+1);return float(k)+float(below)/float(below-above);}}
  return float(CaveFieldBottom);
 }
 return float(top)+.5f;
}
// Works the CAVE strokes into the finished land (after the flats, the footings' pads and the sculpts).
inline void buildTerrainCaves(TerrainShape& shape){
 shape.caveVolume=nullptr;
 shape.caveVolume=buildCaveVolume(shape.caves,[&](int x,int y,int z){return terrainColumnDensity(shape,x,y,z);});
}
// Where the walked surface lies over a point, as the engine's Surface Nets
// density gives it: the voxel tops at the four surrounding sample corners,
// blended bilinearly in x and y and linearly in z (World::terrainSurfaceBelow
// over the whole column). (lx, ly) are map metres; (ox, oy) the map origin.
inline float authoredTerrainSurface(const TerrainShape& shape,float ox,float oy,float lx,float ly){
 int x0=int(std::floor(lx)),y0=int(std::floor(ly));float tx=lx-x0,ty=ly-y0;
 const int cx[4]={int(ox)+x0,int(ox)+x0+1,int(ox)+x0,int(ox)+x0+1},cy[4]={int(oy)+y0,int(oy)+y0,int(oy)+y0+1,int(oy)+y0+1};
 int corner[4];bool padded=false;
 for(int i=0;i<4;++i){corner[i]=authoredTerrainTop(shape,cx[i],cy[i]);padded|=terrainPadAt(shape,cx[i],cy[i])!=nullptr;}
 float weight[4]={(1-tx)*(1-ty),tx*(1-ty),(1-tx)*ty,tx*ty};
 int high=*std::max_element(corner,corner+4),low=*std::min_element(corner,corner+4);
 if(!padded){
  auto density=[&](int z){float d=0;for(int i=0;i<4;++i)d+=weight[i]*(z<=corner[i]?1.f:-1.f);return d;};
  for(int z=high;z>=low;--z){float below=density(z),above=density(z+1);
   if(below>0&&above<=0)return float(z)+below/(below-above);}
  return float(low)+.5f;
 }
 // Pads: the engine's own densities, blended the same way.
 for(int i=0;i<4;++i)if(const TerrainFlat* pad=terrainPadAt(shape,cx[i],cy[i])){high=std::max(high,int(std::ceil(pad->height))+1);low=std::min(low,int(std::floor(pad->height))-1);}
 auto density=[&](int z){float d=0;for(int i=0;i<4;++i)d+=weight[i]*float(terrainColumnDensity(shape,cx[i],cy[i],z))/32.f;return d;};
 for(int z=high;z>=low;--z){float below=density(z),above=density(z+1);
  if(below>0&&above<=0)return float(z)+below/(below-above);}
 return float(low)+.5f;
}
inline int authoredGroundKind(const TerrainShape& shape,float wx,float wy){
 // Meadow: two noise fields pick grass, leafy grass, clover, wild and dry
 // patches a few metres across, PSX-style hard edges between triangles.
 float a=terrainValueNoise(shape.seed+7001u,wx*.11f,wy*.11f)+terrainValueNoise(shape.seed+7002u,wx*.31f,wy*.31f)*.35f;
 float b=terrainValueNoise(shape.seed+7003u,wx*.07f,wy*.07f)+terrainValueNoise(shape.seed+7004u,wx*.23f,wy*.23f)*.4f;
 // Green dominates; wild and dry ground are occasional clearings.
 int kind=a>.62f?GroundDry:a>.42f?GroundWild:b>.38f?GroundClover:b<-.2f?GroundLeafyGrass:GroundShortGrass;
 for(const auto& paint:shape.ground){
  float jitter=terrainValueNoise(shape.seed+7005u,wx*.6f,wy*.6f)*paint.edge;
  float dx=std::max(paint.x1-wx,wx-paint.x2),dy=std::max(paint.y1-wy,wy-paint.y2);
  if(std::max(dx,dy)<jitter)kind=paint.kind==GroundMeadow?kind:paint.kind;
 }
 return kind;
}
// Whether two shapes give one 24 m map (origin ox, oy) the same ground: its voxel tops and
// heights a few metres round it, and the cover paint that can reach it, in its order. The
// Campaign Builder's live picture rebuilds only the maps a terrain edit reached.
inline bool sameTerrainOver(const TerrainShape* a,const TerrainShape* b,float ox,float oy){
 if(a==b)return true;if(!a||!b)return false;
 if(a->seed!=b->seed||a->base!=b->base||a->amplitude!=b->amplitude||a->scale!=b->scale||a->roughness!=b->roughness||a->horizonDepth!=b->horizonDepth||
    a->horizonSpacing!=b->horizonSpacing||a->horizonRise!=b->horizonRise||a->horizonModels!=b->horizonModels||a->ground.empty()!=b->ground.empty())return false;
 auto reaching=[&](const TerrainShape& s){std::vector<float> out;for(const auto& g:s.ground)if(g.x2>=ox-10&&g.x1<=ox+34&&g.y2>=oy-10&&g.y1<=oy+34)out.insert(out.end(),{g.x1,g.y1,g.x2,g.y2,float(g.kind),g.edge});return out;};
 if(reaching(*a)!=reaching(*b))return false;
 for(int y=int(oy)-3;y<int(oy)+27;++y)for(int x=int(ox)-3;x<int(ox)+27;++x)
  if(authoredTerrainTop(*a,x,y)!=authoredTerrainTop(*b,x,y)||std::abs(authoredTerrainHeight(*a,x+.5f,y+.5f)-authoredTerrainHeight(*b,x+.5f,y+.5f))>1e-4f)return false;
 // Footing pads set exact heights the voxel tops do not show.
 auto pads=[&](const TerrainShape& s){std::vector<float> out;for(const auto& g:s.flats)if(g.pad&&g.x2>=ox-3&&g.x1<=ox+27&&g.y2>=oy-3&&g.y1<=oy+27)out.insert(out.end(),{g.x1,g.y1,g.x2,g.y2,g.height,g.blend,float(g.pad)});return out;};
 if(pads(*a)!=pads(*b))return false;
 // The underground: the strokes that can reach the map, in order (a stroke changes only its own box).
 auto caves=[&](const TerrainShape& s){std::vector<CaveStroke> out;for(const auto& c:s.caves)if(caveBoundsMeet(caveStrokeBounds(c),ox-4,oy-4,ox+28,oy+28))out.push_back(c);return out;};
 if(caves(*a)!=caves(*b))return false;
 return true;
}
// TERRAIN_SCULPT offsets as text: whole decimetres, comma separated, a run of
// one value written N*v ("24*0,3,5,22*0"). Returns false with why on a fault.
inline bool parseTerrainSculptValues(const std::string& text,size_t count,std::vector<float>& out,std::string& why){
 out.clear();out.reserve(count);size_t at=0;
 while(at<=text.size()){
  size_t end=text.find(',',at);if(end==std::string::npos)end=text.size();
  std::string item=text.substr(at,end-at);size_t star=item.find('*');long repeat=1;
  try{
   if(star!=std::string::npos){size_t used=0;repeat=std::stol(item.substr(0,star),&used);if(used!=star||repeat<1)throw 0;item=item.substr(star+1);}
   size_t used=0;long value=std::stol(item,&used);if(used!=item.size())throw 0;
   if(std::abs(value)>long(TerrainSculptMaxOffset*10)){why="sculpt offset beyond 30 m";return false;}
   if(out.size()+size_t(repeat)>count){why="more sculpt offsets than cells";return false;}
   out.insert(out.end(),size_t(repeat),float(value)/10.f);
  }catch(...){why="sculpt offsets are whole decimetres, N*v repeats";return false;}
  if(end==text.size())break;at=end+1;
 }
 if(out.size()!=count){why="fewer sculpt offsets than cells";return false;}
 return true;
}
inline std::string formatTerrainSculptValues(const std::vector<float>& offsets){
 std::string text;
 for(size_t i=0;i<offsets.size();){
  long v=std::lround(offsets[i]*10.f);size_t run=1;while(i+run<offsets.size()&&std::lround(offsets[i+run]*10.f)==v)++run;
  if(!text.empty())text+=',';
  if(run>2)text+=std::to_string(run)+'*';else run=1;
  text+=std::to_string(v);i+=run;
 }
 return text;
}
}
