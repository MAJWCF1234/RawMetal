#include "../core/FrameProfile.h"
#include "World.h"
#include "CollisionQueryBatch.h"
#include "TerrainSurfaceCache.h"
#include "../core/Ray.h"
#include "CampaignMaps.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <set>
#include <bit>
#include <cstdio>

namespace retro {
namespace {
std::atomic<uint64_t> nextTerrainRevision{1};
constexpr MapRows ashfallRegion(int region){
 MapRows rows={
  "........................","........................","........................","........................",
  "........................","........................","........................","........................",
  "........................","........................","........................","........................",
  "........................","........................","........................","........................",
  "........................","........................","........................","........................",
  "........................","........................","........................","........................"};
 switch(region){
  case 0: rows[10]="..................CC....";rows[18]=".....##.................";break;
  case 1: rows[5]="...B....................";rows[17]="....................##..";break;
  case 2: rows[11]="................CC......";rows[20]="..##....................";break;
  case 3: rows[6]="....##..................";rows[18]="..................B.....";break;
  case 4: rows[4]="...................##...";rows[19]="....CC..................";break;
  case 5: rows[8]="..B.....................";rows[21]="..................##....";break;
  case 6: rows[6]="................CC......";rows[17]="...##...................";break;
  case 7: rows[9]="....................B...";rows[20]=".....##.................";break;
  case 8: rows[7]="...CC...................";rows[18]="...................##...";break;
  case 9: rows[5]="..................##....";rows[19]="....B...................";break;
  case 10: rows[8]=".....##.................";rows[21]=".................CC.....";break;
  case 11: rows[6]="....................##..";rows[18]="....CC..................";break;
 }
 return rows;
}
static_assert([]{for(int i=0;i<AshfallChunkCount;++i)for(auto row:ashfallRegion(i))if(row.size()!=24)return false;return true;}(),"Ashfall rows must be exactly 24 cells");

constexpr std::array<std::string_view,72> AshfallVoxelTop{{
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111222221111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111122222223222222111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111222333333333332111111111111111111111111111111111111122222111111111",
 "111111111111112222222221111112223333333333333111111111111222222222111111111111222222322222211111",
 "111111111112222223332222222212233432222222223111111222222223333322222222111112223332222222221111",
 "111111111112333333333333332222233432222222223111112222333333333333333222211122333332111111112111",
 "111111111112334444444443333222334432222222223111112233334444444444433332221122334432111111112111",
 "111111111112344455555444443322334432222222223111112333444445555544444333221122344532111111112111",
 "111111111112355555555555443322334432222222223111112334445555555555544433221223344532111111112211",
 "111111111112355666666655443332233432222222223111113333333333336665554433322223344532111111112211",
 "111111222222355667776655544332233432222222223111113222222222237666554443322223344532111111112211",
 "111111233333356677777665544333223333333333333211111211111111237766555443322122344532111111112111",
 "111111223344555667776655544332222333333333332211111211111111237666554443322122334432222222222111",
 "111111223334455666666655443332222222223222222211111211111111236665554433322122333333333333322111",
 "111111122334455555555555443322111111222221111111111211111111235555544433221112223333333332221111",
 "111111122334444455555444443322111111111111111111111211111111235544444333221111222222322222211111",
 "111111122233334444444443333222111111111111111111111211111111234444433332221111111122222111111111",
 "111111112223333333333333332221111111111111111111112211111111233333333222211111111111111111111111",
 "111111111222222223332222222211111111111111111111111211111111233322222222111111111111111111111111",
 "111111111111112222222221111111111111111111111111111111111222222222111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111222222211111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111122222222222222222111111111111111111111111111111111111111111122222111111111",
 "111111111222111111112222223333333332222221111111111111111111111111111111111111222222222222211111",
 "111111222222222111112233333333433333333221111111111112222222222211111111111122222233333222222111",
 "112222222222222222222233344444444444333222111111222222222222222222222111111122233333333333222111",
 "112222233333332222222333444455555444433322111122222111111111233332222221111222333334443333322211",
 "112233333333333332222333445555555554433322211122222111111111233333333221111223334444444443332211",
 "122233334444433332222334445566666554443322211222222111111111234444433222111223344445554444332211",
 "111111111111111132223334455566666555443332211222111111111111111144433322111223344555555544332211",
 "111111111111111111111111111111116554443322212222111111111111111111111111111111114555655544333221",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111222222221111",
 "122211111111123311111111111111111111111111111111111333333333336511111111111111111111222222221111",
 "122211111111123332222222223333331111111111111111111455666666666554443322211223341111222222221111",
 "122211111111123332221122222222222222111111112211111455555666555554433322211223344443222222222211",
 "112211111111123332211111111222222211111111111211111445555555555544433322111223334443222222222211",
 "112211111111122222211111111111111111111111111211111444444444444444433222111222333333222222222211",
 "112211111111122222211111111111111111111111111111111333344444443333333221111122233333222222222111",
 "111111222222222111111111111111111111111111111111111233333333333332222221111122222233222222222111",
 "111111111222111111111111111111111111111111111111111222222333222222222111111111222222222222211111",
 "111111111111111111111111111111111111111111111111111112222222222211111111111111111122222111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111112222222111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111222222222222222221111111111111111111",
 "111111111111111222222211111111111111111111111111111111111222222222333332222222221111111112111111",
 "111111111112222222222222221111111111111111111111111111112222233333333333333322222111112222222111",
 "111111112222222333333322222221111111111222222111111111112223333333444443333333222112222222222222",
 "111111112222233333333333332221111112222222333111112211122233334444444444444333322212222222222222",
 "111111111111233444444433333221111122233333333111112221122233344444455544444433322212111111112322",
 "111111111111234444444444433222111222222222222222223322122233344455555555544433322222111111112332",
 "111111111111234555555544433322111222111111112222223322222333444555556555554443332222111111112332",
 "111111111111235555555554433322211222111111112222223322222333444555666665554443332222111111112332",
 "111111111111235566666554443322212232111111112222223332223333333556666666555444333222111111112332",
 "111111111111235566666555443332212232111111112222223222222222223555666665554443332222111111112333",
 "111111111111235566666554443322212332111111112222223211111111123555556555554443332222111111112332",
 "111111222222235555555554433322212232111111112322222211111111123455555555544433322222222222222332",
 "111111122333334555555544433322112232111111112322222211111111123444455544444433322222333333333332",
 "111111122233444444444444433222111222222222222322222211111111123444444444444333322222333344433332",
 "111111112233333444444433333221111223333333333322222211111111123333444443333333222112233333333322",
 "111111112223333333333333332221111223333444444422222211111111123333333333333322222112222333332222",
 "111111112222222333333322222221111122233333333322222211111111122222333332222222221112222222222222",
 "111111111112222222222222221111111112222222333222222211111111122222222222222221111111112222222111",
 "111111111111111222222211111111111111111222222222111111111111111112222222111111111111111112111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111",
 "111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111111"
}};
static_assert([]{for(auto row:AshfallVoxelTop)if(row.size()!=96)return false;return true;}(),"Ashfall voxel rows must be 96 cells wide");
int ashfallVoxelTop(int worldX,int worldY){
 if(worldX>=96){
  // The old eastern seam remains at height one. A walkable stone shelf rises
  // inland, then falls through sea level into an actual submerged seabed.
  int x=worldX-96;
  int shelf=std::clamp((x-2)/4,0,2);
  int y=std::clamp(worldY,0,71);
  int bend=int(std::round(1.5*std::sin(y*.19)+.7*std::sin(y*.47)));
  int erosion=std::max(0,x-9+bend);
  return std::clamp(1+shelf-(erosion*3)/4,-9,4);
 }
 if(worldX<0||worldY<0||worldX>=96||worldY>=72)return 1;
 char value=AshfallVoxelTop[size_t(worldY)][size_t(worldX)];
 return value>='0'&&value<='9'?value-'0':1;
}
bool authoredAshfallVoxel(int worldX,int worldY,int worldZ){
 return worldZ<=ashfallVoxelTop(worldX,worldY);
}
std::uint8_t authoredAshfallMaterial(int worldX,int worldY,int worldZ){
 int top=ashfallVoxelTop(worldX,worldY);
 if(worldX>=96)return top<=1?TerrainSoil:TerrainRock;
 return worldZ<top-1||top>=5?TerrainRock:TerrainSoil;
}
// Exclusion Zone outdoor packs sample world XY (origin+local) so stitched custom
// chunks share one continuous Surface Nets field. Keep gradients soft — steep
// authored cliffs explode triangle count and tear at chunk seams.
// Smooth value noise: a hashed lattice, quintic-blended, summed over octaves.
float latticeValue(std::uint32_t seed,int x,int y){
 std::uint32_t h=seed*0x9E3779B1u^std::uint32_t(x)*0x85EBCA77u^std::uint32_t(y)*0xC2B2AE3Du;
 h^=h>>15;h*=0x2C1B3C6Du;h^=h>>12;h*=0x297A2D39u;h^=h>>15;return float(h&0xffffff)/float(0xffffff)*2.f-1.f;
}
float valueNoise(std::uint32_t seed,float x,float y){
 int ix=int(std::floor(x)),iy=int(std::floor(y));float fx=x-ix,fy=y-iy;
 auto fade=[](float t){return t*t*t*(t*(t*6-15)+10);};float u=fade(fx),v=fade(fy);
 float a=latticeValue(seed,ix,iy),b=latticeValue(seed,ix+1,iy),c=latticeValue(seed,ix,iy+1),d=latticeValue(seed,ix+1,iy+1);
 return (a*(1-u)+b*u)*(1-v)+(c*(1-u)+d*u)*v;
}
// The continuous (unquantized) surface height of an authored terrain shape.
float authoredTerrainHeight(const TerrainShape& shape,float wx,float wy){
 float sum=0,amp=1,freq=1/std::max(4.f,shape.scale),norm=0;
 for(int octave=0;octave<4;++octave){sum+=valueNoise(shape.seed+std::uint32_t(octave)*101u,wx*freq,wy*freq)*amp;norm+=amp;amp*=shape.roughness;freq*=2.03f;}
 float h=shape.base+shape.amplitude*sum/norm;
 for(const auto& flat:shape.flats){
  float dx=std::max({flat.x1-wx,0.f,wx-flat.x2}),dy=std::max({flat.y1-wy,0.f,wy-flat.y2});
  float d=std::sqrt(dx*dx+dy*dy),t=flat.blend>0?std::clamp(1-d/flat.blend,0.f,1.f):(d<=0?1.f:0.f);
  t=t*t*(3-2*t);h=h+(flat.height-h)*t;
 }
 return h;
}
int authoredTerrainTop(const TerrainShape& shape,int wx,int wy){
 return std::clamp(int(std::lround(authoredTerrainHeight(shape,wx+.5f,wy+.5f))),World::TerrainMinZ+1,World::TerrainMaxZ-2);
}
int authoredGroundKind(const TerrainShape& shape,float wx,float wy){
 // Meadow: two noise fields pick grass, leafy grass, clover, wild and dry
 // patches a few metres across, PSX-style hard edges between triangles.
 float a=valueNoise(shape.seed+7001u,wx*.11f,wy*.11f)+valueNoise(shape.seed+7002u,wx*.31f,wy*.31f)*.35f;
 float b=valueNoise(shape.seed+7003u,wx*.07f,wy*.07f)+valueNoise(shape.seed+7004u,wx*.23f,wy*.23f)*.4f;
 // Green dominates; wild and dry ground are occasional clearings.
 int kind=a>.62f?GroundDry:a>.42f?GroundWild:b>.38f?GroundClover:b<-.2f?GroundLeafyGrass:GroundShortGrass;
 for(const auto& paint:shape.ground){
  float jitter=valueNoise(shape.seed+7005u,wx*.6f,wy*.6f)*paint.edge;
  float dx=std::max(paint.x1-wx,wx-paint.x2),dy=std::max(paint.y1-wy,wy-paint.y2);
  if(std::max(dx,dy)<jitter)kind=paint.kind==GroundMeadow?kind:paint.kind;
 }
 return kind;
}
int exclusionVoxelTop(int worldX,int worldY){
 // Forward facility pad on map 0 stays level so walls and NPCs sit cleanly.
 if(worldX>=5&&worldX<=17&&worldY>=10&&worldY<=19)return 2;
 float hx=worldX*.19f,hy=worldY*.15f;
 float wave=std::sin(hx)*.55f+std::cos(hy)*.45f+std::sin((hx+hy)*.33f)*.35f;
 return std::clamp(2+int(std::lround(wave)),1,3);
}
bool authoredExclusionVoxel(int worldX,int worldY,int worldZ){
 return worldZ<=exclusionVoxelTop(worldX,worldY);
}
std::uint8_t authoredExclusionMaterial(int worldX,int worldY,int worldZ){
 int top=exclusionVoxelTop(worldX,worldY);
 return worldZ<top-1||top>=3?TerrainRock:TerrainSoil;
}

bool insideFixture(const Fixture&fixture,float x,float y,float margin=0){
 float dx=x-fixture.position.x,dy=y-fixture.position.y,c=std::cos(fixture.yaw),s=std::sin(fixture.yaw);
 return std::fabs(dx*c-dy*s)<fixture.width*fixture.core*.5f+margin&&std::fabs(dx*s+dy*c)<fixture.depth*fixture.core*.5f+margin;
}
constexpr float ShelfTiers[]={.17f,.54f,.92f};
bool benchTerminal(const World& world,const Terminal& terminal){return terminal.paper()?terminal.paperOnBench():world.campaign()&&world.level()>=6&&!terminal.control&&terminal.reactorAction==0;} // Reactor consoles keep their own housing.
bool insideTerminal(const World& world,const Terminal& terminal,float x,float y){
 if(terminal.paper()&&!terminal.paperOnBench())return false; // the wall or the desk does the colliding
 if(benchTerminal(world,terminal))return x>terminal.position.x-.48f&&x<terminal.position.x+.42f&&std::fabs(y-terminal.position.y)<.36f;
 return std::fabs(x-terminal.position.x)<.27f&&std::fabs(y-terminal.position.y)<.18f;
}
float terminalHeight(const World& world,const Terminal& terminal,float x,float y){
 if(!benchTerminal(world,terminal))return .95f;
 if(terminal.paper())return .815f; // the bench top: no monitor, no keyboard
 float yaw=world.level()==6&&!world.authored()?-kPi*.5f:terminal.yaw,c=std::cos(yaw),s=std::sin(yaw);
 float dx=x-terminal.position.x,dy=y-terminal.position.y;
 // The monitor has its own narrow footprint. Treating its full height as a
 // desk-sized box made objects stand on invisible space beside the screen.
 if(std::fabs(dx*c-dy*s)<.1932545f&&std::fabs(dx*s+dy*c)<.266175f)return 1.32005f;
 float frontX=std::sin(yaw)*.22f,frontY=std::cos(yaw)*.22f;
 if(std::fabs((dx-frontX)*c-(dy-frontY)*s)<.14f&&std::fabs((dx-frontX)*s+(dy-frontY)*c)<.105f)return .85f;
 return .815f;
}
}

void World::buildPopulation(){
 using C=CreatureKind;using P=PickupKind;
 if(!campaign()){
  if(coast()){
   // Keep encounters on the dry headland; the eastern half of each chunk is
   // submerged and ordinary grounded actors cannot navigate its seabed.
   m_creatureSpawns.push_back({C::Wasp,{5.5f,17.5f},-999});
   if(m_level==13)m_pickupSpawns.push_back({{4.5f,7.5f},P::Ammo});
   return;
  }
  const Vec2 encounter[]={{18.5f,18.5f},{5.5f,18.5f},{18.5f,5.5f},{5.5f,5.5f}};
  auto p=encounter[m_level%4];
  if(m_level%5!=3)m_creatureSpawns.push_back({m_level%4==2?C::Wasp:m_level%6==5?C::Brute:C::Huntsman,p,-999});
  if(m_level==6||m_level==10)m_creatureSpawns.push_back({C::Wasp,{19.5f,18.5f},-999});
  if(m_level%3==0)m_pickupSpawns.push_back({{4.5f,19.5f},P::Ammo});
  if(m_level==5||m_level==11)m_pickupSpawns.push_back({{20.5f,4.5f},P::Health});
  int junk=2+(m_level%2);
  for(int i=0;i<junk;++i)m_clutterSpawns.push_back({(i+m_level)%6,{3.5f+i*1.4f,20.5f-float((i+m_level)%2)*1.2f},-999,(i+m_level)*.7f});
  return;
 }
}

World::World(int level,std::shared_ptr<const AuthoredMapData> map):m_level(level),m_worldId(WorldId::Custom){loadAuthoredMap(std::move(map));}
void World::settleExclusionFacilityOnTerrain(){
 if(!m_exclusionTerrain||!m_mapData)return;
 auto ground=[&](float x,float y){return terrainSurfaceBelow(x,y,float(TerrainMaxZ+1));};
 // Structures use absolute heights, while props/fixtures/terminals already add
 // their local floor in rendering AND collision. Lifting those offsets again
 // placed the controls in mid-air. Ground screens on their own footprint;
 // a sloping approach must not bury most of its intended concealment height.
 for(auto&s:m_structures){float low=1000,high=-1000;
  int nx=std::max(1,int(std::ceil((s.x2-s.x1)*2))),ny=std::max(1,int(std::ceil((s.y2-s.y1)*2)));
  for(int y=0;y<=ny;++y)for(int x=0;x<=nx;++x){float z=ground(s.x1+(s.x2-s.x1)*float(x)/nx,s.y1+(s.y2-s.y1)*float(y)/ny);low=std::min(low,z);high=std::max(high,z);}
  s.bottom+=low;s.top+=high;
 }
 for(auto&c:m_clutterSpawns)if(c.z>-100)c.z+=ground(c.position.x,c.position.y);
 // Hazards and emitters are authored relative to the ground at their centre.
 for(auto&h:m_hazards){float z=ground((h.x1+h.x2)*.5f,(h.y1+h.y2)*.5f);h.bottom+=z;h.top+=z;}
 for(auto&e:m_particleEmitters)e.z+=ground(e.position.x,e.position.y);
 for(auto&c:m_creatureSpawns)if(c.z>-100)c.z+=ground(c.position.x,c.position.y);
 for(auto&p:m_pickupSpawns)if(p.z>-100)p.z+=ground(p.position.x,p.position.y);
 auto data=std::make_shared<AuthoredMapData>(*m_mapData);
 data->definition.spawnHeight=terrainSurfaceBelow(data->definition.playerStart.x,data->definition.playerStart.y,float(TerrainMaxZ+1));
 for(auto&sign:data->signs)sign.z+=ground(sign.position.x,sign.position.y);
 // Like hazards, a shelter's height band is measured from the ground at its centre.
 for(auto&s:data->shelters){float z=ground((s.x1+s.x2)*.5f,(s.y1+s.y2)*.5f);s.bottom+=z;s.top+=z;}
 for(auto&hint:data->navHints)hint.z+=ground(hint.position.x,hint.position.y);
 for(auto&light:m_lights)light.z+=ground(light.position.x,light.position.y);
 for(auto&sequence:data->timedSequences)if(sequence.x1>=0&&sequence.y1>=0&&sequence.x2<=Width&&sequence.y2<=Height){
  float low=1000,high=-1000;for(float y=sequence.y1;y<=sequence.y2;y+=.5f)for(float x=sequence.x1;x<=sequence.x2;x+=.5f){float z=ground(x,y);low=std::min(low,z);high=std::max(high,z);}
  sequence.bottom+=low;sequence.top+=high;
 }
 for(auto&track:data->actorTracks){
  for(auto&key:track.keys)key.z=terrainSurfaceBelow(key.position.x,key.position.y,float(TerrainMaxZ+1));
  for(auto&point:track.ai.patrol)point.z=terrainSurfaceBelow(point.position.x,point.position.y,float(TerrainMaxZ+1));
  for(auto&key:track.idleKeys)key.z=terrainSurfaceBelow(key.position.x,key.position.y,float(TerrainMaxZ+1));
 }
 m_mapData=std::move(data);
 // Rebuild terminal views from the settled map so title strings stay owned.
 m_terminals.clear();m_terminals.reserve(m_mapData->terminals.size());
 for(const auto&source:m_mapData->terminals)m_terminals.push_back({source.position,source.title.c_str(),source.line1.c_str(),source.line2.c_str(),source.z,source.control,source.reactorAction,source.activateState,source.toggleState,source.yaw,source.requireState,source.readable,source.taped,source.surface});
 m_actorPoses.clear();for(const auto&track:m_mapData->actorTracks)m_actorPoses.push_back(track.idleKeys.empty()?sampleActor(track,0):sampleActorKeys(track.idleKeys,0,true));
 m_structureCells.assign(size_t(Width*Height),{});
 if(m_structures.size()>65535)throw std::runtime_error("Exclusion facility exceeds the collision structure capacity");
 for(size_t index=0;index<m_structures.size();++index){const auto&s=m_structures[index];
  for(int y=int(std::floor(s.y1-.001f));y<=int(std::floor(s.y2+.001f));++y)for(int x=int(std::floor(s.x1-.001f));x<=int(std::floor(s.x2+.001f));++x)
   if(x>=0&&x<Width&&y>=0&&y<Height)m_structureCells[size_t(y*Width+x)].push_back(static_cast<uint16_t>(index));}
 m_fixtureRotation.clear();
 m_collisionCells={};m_collisionAll={};
}
void World::loadAuthoredMap(std::shared_ptr<const AuthoredMapData> map){
 m_lightSolids.clear();m_lightNodes.clear();
 m_collisionCells={};m_collisionAll={};
 if(!map)throw std::runtime_error("Missing authored map data");
 if(map->layers.empty())throw std::runtime_error("Authored map has no floor layers");
 // Collect every malformed record so an author can repair a map in one pass.
 std::ostringstream invalid;
 auto reject=[&](const char* kind,size_t index,const std::string& values){
  std::string record="map "+std::to_string(m_level)+" ("+map->name+") "+kind+"["+std::to_string(index)+"] "+values;
  invalid<<record<<'\n';
 };
 for(size_t i=0;i<map->doors.size();++i){const auto& d=map->doors[i];if(!(d.left>=0&&d.left<d.right&&d.right<=24&&d.maxOpen>0&&d.maxOpen<=1))reject("door",i,"left="+std::to_string(d.left)+" right="+std::to_string(d.right)+" maxOpen="+std::to_string(d.maxOpen));}
 for(size_t i=0;i<map->structures.size();++i){const auto& v=map->structures[i];if(!(v.bottom<=v.top&&v.x2>v.x1&&v.y2>v.y1))reject("structure",i,"bounds="+std::to_string(v.x1)+","+std::to_string(v.y1)+","+std::to_string(v.x2)+","+std::to_string(v.y2)+" bottom="+std::to_string(v.bottom)+" top="+std::to_string(v.top));}
 for(size_t i=0;i<map->compactors.size();++i)if(!(map->compactors[i].period>0))reject("compactor",i,"period="+std::to_string(map->compactors[i].period));
 for(size_t i=0;i<map->fixtures.size();++i)if(!(map->fixtures[i].core>=0&&map->fixtures[i].core<=1))reject("fixture",i,"core="+std::to_string(map->fixtures[i].core));
 for(size_t i=0;i<map->timedSequences.size();++i){const auto& v=map->timedSequences[i];
  if(v.durationMs<=0||v.finishAtMs<1||v.finishAtMs>v.durationMs||v.soundIntervalMs<0)reject("timedSequence",i,"durationMs="+std::to_string(v.durationMs)+" finishAtMs="+std::to_string(v.finishAtMs)+" soundIntervalMs="+std::to_string(v.soundIntervalMs));
  for(size_t j=0;j<map->sequenceCues.size();++j){const auto& cue=map->sequenceCues[j];if(cue.timerState==v.timerState&&(cue.timeMs<0||cue.timeMs>v.durationMs))reject("sequenceCue",j,"sequence="+std::to_string(i)+" timerState="+std::to_string(v.timerState)+" timeMs="+std::to_string(cue.timeMs)+" durationMs="+std::to_string(v.durationMs));}
 }
 // updateScripts checks the region for every event, including state-gated events.
 for(size_t i=0;i<map->scriptEvents.size();++i){const auto& e=map->scriptEvents[i];if(!e.id||e.x2<=e.x1||e.y2<=e.y1)reject("scriptEvent",i,"id="+std::to_string(e.id)+" bounds="+std::to_string(e.x1)+","+std::to_string(e.y1)+","+std::to_string(e.x2)+","+std::to_string(e.y2));}
 for(size_t i=0;i<map->clutterSpawns.size();++i){const auto&v=map->clutterSpawns[i];if(v.kind<0||v.kind>7||(v.kind==7&&(v.model<0||v.model>=FacilityModelCount||!(v.width>0&&v.width<=1.6f&&v.depth>0&&v.depth<=1.6f&&v.height>0&&v.height<=1.6f)||!std::isfinite(v.position.x)||!std::isfinite(v.position.y)||!std::isfinite(v.z)||!std::isfinite(v.yaw)||!std::isfinite(v.pitch)||!std::isfinite(v.roll))))reject("prop",i,"invalid model, dimensions or pose");}
 for(size_t i=0;i<map->breakables.size();++i){const auto&v=map->breakables[i];if(!(v.x1>=.3f&&v.y1>=.3f&&v.x2<=23.7f&&v.y2<=23.7f&&v.x2>v.x1&&v.y2>v.y1&&v.bottom<v.top&&std::isfinite(v.bottom)&&std::isfinite(v.top)&&v.health>=0&&std::isfinite(v.health)&&v.material==17)||bool(map->terrain)||(map->definition.environment==Environment::Outdoor&&(map->skybox=="brutal_wasteland"||map->skybox=="zone_overcast")))reject("breakable",i,"invalid bounds, material or terrain");}
 if(!invalid.str().empty())throw std::runtime_error(invalid.str());
 const auto& lift=map->cargoLift;
 if(lift.x1!=lift.x2||lift.y1!=lift.y2||lift.lower!=lift.upper){
  if(lift.x1>=lift.x2||lift.y1>=lift.y2||lift.lower>=lift.upper||lift.speed<=0||!lift.callState||!lift.releaseState||!lift.positionState||!lift.downState||!lift.arrivedState||!lift.descendedState)throw std::runtime_error("Invalid authored cargo lift or control states");
 }
 for(const auto& sequence:map->timedSequences)if(!sequence.timerState||!sequence.finishState||sequence.durationMs<1||sequence.durationMs>3600000||sequence.finishAtMs<1||sequence.finishAtMs>sequence.durationMs||sequence.soundIntervalMs<0||sequence.x1>sequence.x2||sequence.y1>sequence.y2||sequence.bottom>sequence.top||sequence.gain<0||sequence.pitch<=0)throw std::runtime_error("Invalid authored timed sequence");
 for(const auto& s:map->timedSequences)if(s.sightActor<-1||s.sightActor>=int(map->actorTracks.size())||!std::isfinite(s.sightDistance)||s.sightDistance<=0)throw std::runtime_error("Invalid sequence sight target");
 auto timerExists=[&](StateId id){return std::any_of(map->timedSequences.begin(),map->timedSequences.end(),[&](const auto& s){return s.timerState==id;});};
 for(const auto& track:map->actorTracks){
  const auto& ai=track.ai;
  if(int(ai.mode)<0||int(ai.mode)>3||!std::isfinite(ai.speed)||ai.speed<=0||ai.speed>4||!std::isfinite(ai.followDistance)||ai.followDistance<1.2f||ai.followDistance>5||!std::isfinite(ai.dangerRange)||ai.dangerRange<2||ai.dangerRange>20||(ai.mode!=ActorAiMode::Scripted&&(!humanActorVisual(track.visual)||track.platform)))throw std::runtime_error("Invalid actor AI configuration");
  if(int(ai.faction)<0||int(ai.faction)>5||(ai.squad&&ai.mode!=ActorAiMode::Guard))throw std::runtime_error("Invalid actor faction or guard squad");
  if(ai.grenades<0||ai.grenades>4||((ai.suppress||ai.grenades)&&(ai.mode!=ActorAiMode::Guard||!ai.armed)))throw std::runtime_error("Tactical combat requires an armed guard and bounded grenade inventory");
  if(ai.weapon<1||ai.weapon>2||(ai.weapon==2&&(ai.mode!=ActorAiMode::Guard||!ai.armed)))throw std::runtime_error("Invalid actor firearm");
  if(int(ai.role)<0||int(ai.role)>6||(ai.role!=GuardSquadRole::Default&&(ai.mode!=ActorAiMode::Guard||!ai.armed||!ai.squad)))throw std::runtime_error("Squad roles require an equipped guard and a named squad");
  if(ai.mode==ActorAiMode::Guard){
   if(!std::isfinite(ai.sightRange)||ai.sightRange<1||ai.sightRange>30||!std::isfinite(ai.sightDot)||ai.sightDot<0||ai.sightDot>.99f||ai.recognitionMs<100||ai.recognitionMs>5000||ai.searchMs<250||ai.searchMs>30000||ai.patrol.empty()||ai.patrol.size()>64)throw std::runtime_error("Invalid guard AI configuration");
   for(const auto& point:ai.patrol)if(!std::isfinite(point.position.x)||!std::isfinite(point.position.y)||point.position.x<.25f||point.position.x>23.75f||point.position.y<.25f||point.position.y>23.75f||!std::isfinite(point.z)||std::fabs(point.z)>100||point.pauseMs<0||point.pauseMs>60000)throw std::runtime_error("Invalid guard patrol point");
   if(ai.aimMs<100||ai.aimMs>3000||ai.recoveryMs<300||ai.recoveryMs>5000||!std::isfinite(ai.attackRange)||ai.attackRange<1||ai.attackRange>18||!std::isfinite(ai.attackDamage)||ai.attackDamage<1||ai.attackDamage>100)throw std::runtime_error("Invalid guard weapon configuration");
  }else if(!ai.patrol.empty()||ai.sightRange!=11||ai.sightDot!=.65f||ai.recognitionMs!=600||ai.searchMs!=4000||ai.alertState||ai.armed||ai.aimMs!=650||ai.recoveryMs!=1000||ai.attackRange!=9||ai.attackDamage!=18)throw std::runtime_error("Guard configuration requires Guard actor AI mode");
  if(!timerExists(track.timerState)||track.scale<=0||track.scale>20||!std::isfinite(track.scale)||track.keys.size()<2||track.keys.front().timeMs!=0||track.footprint.x<=0||track.footprint.y<=0||track.thickness<=0||!std::isfinite(track.footprint.x)||!std::isfinite(track.footprint.y)||!std::isfinite(track.thickness))throw std::runtime_error("Invalid authored actor track");
  int previous=-1;for(const auto& k:track.keys){if(k.timeMs<=previous||k.timeMs>3600000||!std::isfinite(k.position.x)||!std::isfinite(k.position.y)||!std::isfinite(k.z)||!std::isfinite(k.yaw)||!std::isfinite(k.phase)||k.phase<0||k.phase>64||k.lookAtActor<-1||k.lookAtActor>=int(map->actorTracks.size())||k.clip<0||k.clip>lastActorClip(track.visual))throw std::runtime_error("Invalid actor pose key");previous=k.timeMs;}
  if(!track.idleKeys.empty()){
   if(track.idleKeys.size()<2||track.idleKeys.front().timeMs!=0||track.idleUntilMs<1||track.approachUntilMs<=track.idleUntilMs||track.approachUntilMs>track.keys.back().timeMs)throw std::runtime_error("Invalid actor prelude");
   int previous=-1;for(const auto& k:track.idleKeys){if(k.timeMs<=previous||k.timeMs>3600000||!std::isfinite(k.position.x)||!std::isfinite(k.position.y)||!std::isfinite(k.z)||!std::isfinite(k.yaw)||!std::isfinite(k.phase)||k.phase<0||k.phase>64||k.clip<0||k.clip>lastActorClip(track.visual)||k.lookAtActor!=-1)throw std::runtime_error("Invalid actor idle key");previous=k.timeMs;}
  }
  if(track.health<0||track.health>100000||(track.health&&(!track.damageState||!track.deadState)))throw std::runtime_error("Invalid actor health states");
 }
 // Every conversation an actor opens and every reply's next line is in this map.
 auto node=[&](StateId id){return std::any_of(map->dialogue.begin(),map->dialogue.end(),[&](const DialogueNode&n){return n.id==id;});};
 for(const auto& track:map->actorTracks)if(track.talk&&!node(track.talk))throw std::runtime_error("Actor opens a dialogue node this map does not have");
 for(const auto& n:map->dialogue){// Up to six replies may be written; states gate them, and at most four show at once.
  if(n.line.empty()||n.replies.size()>6)throw std::runtime_error("Dialogue node needs a line and at most six replies");
  for(const auto& reply:n.replies)if(reply.text.empty()||(reply.next&&!node(reply.next)))throw std::runtime_error("Dialogue reply has no text or leads to a missing node");}
 for(const auto& cue:map->sequenceCues)if(!timerExists(cue.timerState)||cue.timeMs<0||int(cue.sound)<0||int(cue.sound)>=int(Sound::Count)||cue.gain<0||cue.pitch<=0||!std::isfinite(cue.gain)||!std::isfinite(cue.pitch)||std::isinf(cue.z))throw std::runtime_error("Invalid actor sequence cue");
 for(const auto& layer:map->layers){
  if(!std::isfinite(layer.elevation)||!std::isfinite(layer.thickness)||layer.thickness<0)throw std::runtime_error("Invalid authored layer elevation or thickness");
  for(const auto& row:layer.rows)if(row.size()!=Width)throw std::runtime_error("Authored map rows must match the chunk width");
 }
 for(const auto& fixture:map->fixtures)if(fixture.model<0||fixture.model>=FacilityModelCount||fixture.width<=0||fixture.depth<=0||fixture.height<=0)throw std::runtime_error("Invalid authored fixture model or dimensions");
 for(const auto& light:map->lights)if(!std::isfinite(light.position.x)||!std::isfinite(light.position.y)||!std::isfinite(light.z)||!std::isfinite(light.yaw)||!std::isfinite(light.intensity)||!std::isfinite(light.range)||int(light.mount)<0||int(light.mount)>int(WorldLightMount::Bare)||light.intensity<0||light.intensity>8||light.range<=0||light.range>24)throw std::runtime_error("Invalid authored light mount or influence");
 for(const auto& light:map->lights)if(!std::isfinite(light.kelvin)||light.kelvin<1000||light.kelvin>12000||!std::isfinite(light.tint)||light.tint< -1||light.tint>1||!std::isfinite(light.period)||!std::isfinite(light.onTime)||!std::isfinite(light.phase)||!std::isfinite(light.flicker)||light.period<0||light.onTime<0||light.onTime>light.period||light.flicker<0||light.flicker>1)throw std::runtime_error("Invalid authored light switching: finite light colour/timing, period >= 0, on-time within the period, flicker 0-1");
 for(const auto& zone:map->darkZones)if(!std::isfinite(zone.x1)||!std::isfinite(zone.y1)||!std::isfinite(zone.x2)||!std::isfinite(zone.y2)||!std::isfinite(zone.bottom)||!std::isfinite(zone.top)||zone.x1>=zone.x2||zone.y1>=zone.y2||zone.bottom>=zone.top||zone.x2<=0||zone.y2<=0||zone.x1>=Width||zone.y1>=Height)throw std::runtime_error("Invalid authored dark zone: a non-empty box overlapping its map");
 for(const auto& sign:map->signs)if(!std::isfinite(sign.width)||!std::isfinite(sign.height)||!std::isfinite(sign.z)||!std::isfinite(sign.yaw)||!std::isfinite(sign.position.x)||!std::isfinite(sign.position.y)||sign.width<=0||sign.height<=0)throw std::runtime_error("Invalid authored sign dimensions");
 for(const auto& prop:map->props)if(prop.kind<0||prop.kind>=4)throw std::runtime_error("Invalid authored prop model");
 for(int surface:{map->wallSurface,map->floorSurface,map->ceilingSurface})if(surface<SurfaceDefault||surface>=SurfaceCount)throw std::runtime_error("Invalid authored surface set");
 for(const auto& stair:map->stairs)if(stair.steps<1||stair.steps>4096||stair.x1>=stair.x2||stair.y1>=stair.y2||stair.bottom>stair.top||!std::isfinite(stair.treadThickness)||stair.treadThickness<.025f||stair.treadThickness>.5f)throw std::runtime_error("Invalid authored staircase");
 m_definitionSource=map.get();m_mapData=std::move(map);
 m_actorPoses.clear();for(const auto& track:m_mapData->actorTracks)m_actorPoses.push_back(track.idleKeys.empty()?sampleActor(track,0):sampleActorKeys(track.idleKeys,0,true));
 m_openNorthBoundary=m_mapData->openNorth;m_openSouthBoundary=m_mapData->openSouth;
 m_openWestBoundary=m_mapData->openWest;m_openEastBoundary=m_mapData->openEast;
 m_doors=m_mapData->doors;m_props=m_mapData->props;m_fixtures=m_mapData->fixtures;m_pipes=m_mapData->pipes;m_lights=m_mapData->lights;
 m_creatureSpawns=m_mapData->creatureSpawns;m_pickupSpawns=m_mapData->pickupSpawns;m_clutterSpawns=m_mapData->clutterSpawns;
 m_waterVolumes=m_mapData->waterVolumes;m_hazards=m_mapData->hazards;m_particleEmitters=m_mapData->emitters;m_compactors=m_mapData->compactors;m_structures=m_mapData->structures;
 m_scriptEvents=m_mapData->scriptEvents;
 m_pushCrates=m_mapData->pushCrates;m_cratePlates=m_mapData->cratePlates;
 for(auto& crate:m_pushCrates){crate.home=crate.position;crate.z=m_mapData->layers.empty()?crate.z:m_mapData->layers.front().elevation+crate.z;}
 m_cargoLift=m_mapData->cargoLift;m_cargoLiftHeight=m_cargoLift.lower;
 m_layers.reserve(m_mapData->layers.size());
 // Boundary doors are bounded apertures. The tall hall above their frames is
 // solid structure, not a second route over a locked door into another floor.
 for(const auto& door:m_doors)if(door.entry||door.transfer){
  float base=m_mapData->layers.front().elevation+door.z,top=m_mapData->definition.ceiling;
  if(top>base+2.65f)m_structures.push_back({door.left,door.y-.14f,door.right,door.y+.14f,base+2.65f,top,false,3});
 }
 for(const auto& source:m_mapData->layers){
  MapRows rows{};for(size_t row=0;row<rows.size();++row)rows[row]=source.rows[row];
  m_layers.push_back({source.name,source.elevation,source.thickness,rows});
 }
 m_terminals.reserve(m_mapData->terminals.size());
 for(const auto& source:m_mapData->terminals)m_terminals.push_back({source.position,source.title.c_str(),source.line1.c_str(),source.line2.c_str(),source.z,source.control,source.reactorAction,source.activateState,source.toggleState,source.yaw,source.requireState,source.readable,source.taped,source.surface});
 m_breakableFirst=int(m_structures.size());m_brokenBreakables.clear();m_breakableDamage.assign(breakables().size(),0);
 for(const auto&b:breakables())m_structures.push_back({b.x1,b.y1,b.x2,b.y2,b.bottom,b.top,false,b.material});

 buildLayers(m_mapData->stairs);
 if(hasLift())refreshReactorTerminals();
 // Outdoor wasteland custom packs own a world-space Surface Nets heightfield so
 // the facility and NPCs sit on dirt/rock instead of a flat authored slab.
 if(outdoors()&&(m_mapData->skybox=="brutal_wasteland"||m_mapData->skybox=="zone_overcast")){
  m_exclusionTerrain=true;m_internalWallHeight=2.8f;buildTerrain();settleExclusionFacilityOnTerrain();
 }
}

World::World(int level,WorldId id):m_worldId(id) {
 if(worldChunkCount(id)<=0)throw std::invalid_argument("Custom worlds require authored map data");
 m_level=std::clamp(level,0,worldChunkCount(m_worldId)-1);
 if(campaign())if(auto map=campaignMap(m_level)){loadAuthoredMap(std::move(map));return;}
 buildPopulation();
 if(horrorMode()){
  const char* regionNames[]={
   "Ashfall / west approach","Ashfall / ridge road","Ashfall / dry interchange","Ashfall / east escarpment",
   "Ashfall / motel flats","Ashfall / relay crossroads","Ashfall / scrap basin","Ashfall / utility mesa",
   "Ashfall / south wash","Ashfall / dead subdivision","Ashfall / breaker yard","Ashfall / evacuation edge",
   "Ashfall Coast / north headland","Ashfall Coast / tidal shelf","Ashfall Coast / south bluffs"};
  auto rows=ashfallRegion(m_level);
  m_layers={{regionNames[m_level],0,0,rows}};
  int col=m_level>=12?4:m_level%4,row=m_level>=12?m_level-12:m_level/4;
  m_openWestBoundary=col>0;m_openEastBoundary=col<4;
  m_openNorthBoundary=row>0;m_openSouthBoundary=row<2;
  m_internalWallHeight=2.8f;
  buildTerrain();
  if(coast()){
   // One ocean record drives water physics and the visible sea; the terrain
   // mesh continues several metres beneath it for a proper shallow shelf.
   m_waterVolumes.push_back({0,0,24,24,-9.f,-.45f});
   buildLayers({});return;
  }

  constexpr Vec2 pads[]={{7,7},{15,8},{8,16},{16,9},{8,14},{16,16},{7,7},{16,15},{8,8},{16,10},{8,15},{16,8}};
  auto pad=pads[m_level];
  float base=floorHeight(pad.x,pad.y);
  auto junkShack=[&](float cx,float cy,float width,float depth,float height,int material,int variant){
   float x0=cx-width*.5f,x1=cx+width*.5f,y0=cy-depth*.5f,y1=cy+depth*.5f,t=.16f,door=.62f;
   m_structures.push_back({x0,y1-t,x1,y1,base,base+height,false,material});
   m_structures.push_back({x0,y0,x0+t,y1,base,base+height,false,material});
   if(variant!=2)m_structures.push_back({x1-t,y0,x1,y1,base,base+height*(variant?0.72f:1.f),false,material});
   m_structures.push_back({x0,y0,cx-door,y0+t,base,base+height,false,material});
   m_structures.push_back({cx+door,y0,x1,y0+t,base,base+height,false,material});
   if(variant==0)m_structures.push_back({x0+t,y0+t,cx-.15f,y1-t,base+height,base+height+.12f,false,2});
   if(variant==1)m_structures.push_back({cx+.25f,y0+t,x1-t,y1-t,base+height*.82f,base+height*.94f,false,2});
  };
  auto junkWall=[&](float x,float y,float length,bool alongY){
   if(alongY)m_structures.push_back({x-.10f,y,x+.10f,y+length,base,base+1.55f,false,3});
   else m_structures.push_back({x,y-.10f,x+length,y+.10f,base,base+1.55f,false,3});
  };

  switch(m_level){
   case 0:
    junkShack(pad.x,pad.y,5.6f,4.6f,2.35f,3,1);
    m_props.push_back({1,{pad.x+2.8f,pad.y+1.2f},1.1f,1.9f,kPi*.5f,{.30f,.95f},0});
    m_fixtures.push_back({8,{pad.x-2.35f,pad.y+.4f},.65f,.7f,.20f,1.f,kPi*.5f,true});
    break;
   case 1:
    junkWall(pad.x-2.5f,pad.y+1.8f,5.f,false);
    m_fixtures.push_back({6,{pad.x+1.3f,pad.y-.6f},0,1.8f,.55f,.95f,.25f,true});
    break;
   case 2:
    junkShack(pad.x,pad.y,6.2f,5.2f,2.5f,3,2);
    m_props.push_back({0,{pad.x-2.8f,pad.y+2.6f},1.35f,2.2f,.2f,{1.1f,.66f},0});
    break;
   case 3:
    junkWall(pad.x-3.f,pad.y,6.f,false);
    junkWall(pad.x+1.8f,pad.y-2.2f,4.4f,true);
    break;
   case 4:
    junkShack(pad.x,pad.y,5.2f,4.4f,2.2f,2,0);
    m_fixtures.push_back({7,{pad.x+1.9f,pad.y},0,1.7f,.5f,1.8f,kPi*.5f,true});
    break;
   case 5:
    junkShack(pad.x,pad.y,6.0f,5.2f,2.45f,3,0);
    m_props.push_back({0,{pad.x+3.4f,pad.y-1.4f},1.45f,2.35f,kPi*.5f,{1.15f,.7f},0});
    m_fixtures.push_back({8,{pad.x-2.55f,pad.y+.8f},.7f,.7f,.20f,1.f,kPi*.5f,true});
    break;
   case 6:
    junkWall(pad.x-2.6f,pad.y-2.f,5.2f,false);
    junkWall(pad.x-2.6f,pad.y-2.f,4.f,true);
    m_props.push_back({2,{pad.x+2.2f,pad.y+1.5f},.35f,3.5f,.4f,{1.7f,.18f},0});
    break;
   case 7:
    junkShack(pad.x,pad.y,5.8f,5.f,2.5f,2,1);
    m_fixtures.push_back({6,{pad.x-2.6f,pad.y+2.2f},0,1.7f,.55f,.95f,kPi*.5f,true});
    break;
   case 8:
    m_props.push_back({1,{pad.x+1.8f,pad.y},1.05f,1.8f,.7f,{.28f,.9f},0});
    break;
   case 9:
    junkShack(pad.x,pad.y,6.4f,4.8f,2.3f,3,2);
    junkWall(pad.x-3.5f,pad.y+2.9f,4.5f,false);
    break;
   case 10:
    junkWall(pad.x-3.f,pad.y-2.7f,6.f,false);
    junkWall(pad.x-3.f,pad.y+2.7f,6.f,false);
    m_fixtures.push_back({6,{pad.x,pad.y},0,2.2f,.7f,1.15f,.35f,true});
    m_props.push_back({0,{pad.x+3.1f,pad.y+.8f},1.35f,2.2f,kPi*.5f,{1.1f,.66f},0});
    break;
   case 11:
    junkShack(pad.x,pad.y,7.2f,5.8f,2.65f,3,0);
    junkWall(pad.x-4.f,pad.y+3.4f,8.f,false);
    m_fixtures.push_back({7,{pad.x+3.2f,pad.y+1.5f},0,1.8f,.5f,1.8f,kPi*.5f,true});
    break;
  }

  const auto wallCount=m_structures.size();
  for(size_t i=0;i<wallCount;++i){const auto wall=m_structures[i];if(std::fabs(wall.bottom-base)>.01f)continue;
   float bottom=base;
   for(float x=wall.x1;x<=wall.x2+.001f;x+=.2f)for(float y=wall.y1;y<=wall.y2+.001f;y+=.2f)
    bottom=std::min(bottom,floorHeight(x,y)-.12f);
   if(bottom<base-.02f)m_structures.push_back({wall.x1,wall.y1,wall.x2,wall.y2,bottom,base,false,3});
  }
  const char* relayLines[]={
   "WEST APPROACH / NO CIVIL TRAFFIC.","RIDGE ROAD / POWER LINES DOWN.","DRY INTERCHANGE / EAST ROUTE OPEN.","EAST ESCARPMENT / LONG RANGE VISIBILITY.",
   "MOTEL FLATS / STRUCTURES UNSAFE.","RELAY CROSSROADS / GRID INTERMITTENT.","SCRAP BASIN / SALVAGE SCATTERED.","UTILITY MESA / SUBSTATION DEAD.",
   "SOUTH WASH / FLASH FLOOD CHANNEL.","DEAD SUBDIVISION / NO RESPONSE.","BREAKER YARD / HIGH VOLTAGE ISOLATED.","EVACUATION EDGE / OUTER GATE LOST."};
  if(m_level==0||m_level==5||m_level==11)
   m_terminals={{{pad.x,pad.y-1.6f},"ASHFALL FIELD RELAY",relayLines[m_level],"96 X 72 M SURFACE GRID / LOCAL LINK.",0,false}};
  buildLayers({});return;
 }
 if(m_level>=6){
  using A=ScriptAction::Type;
  const float roof=definition().ceiling;
  auto wall=[&](float x1,float y1,float x2,float y2,float bottom,float top){m_structures.push_back({x1,y1,x2,y2,bottom,top,false,3});};
  auto shelf=[&](Vec2 p,float yaw=0.f,float z=0.f){m_fixtures.push_back({7,p,z,1.8f,.5f,1.8f,yaw,true});};
  auto cabinet=[&](Vec2 p,float yaw=0.f){m_fixtures.push_back({13,p,0,.9066f,.4956f,2.2f,yaw,true});};
  auto tank=[&](Vec2 p,float height){float scale=height/2.390135f;m_fixtures.push_back({14,p,0,1.58f*scale,1.62f*scale,height,0,true});};
  auto post=[&](float x,float y,float bottom,float top,float half=.12f){m_structures.push_back({x-half,y-half,x+half,y+half,bottom,top,false,2});};
  auto event=[&](const char* id,float x1,float y1,float x2,float y2,float lo,float hi,std::vector<ScriptAction> actions){
   ScriptEvent e;e.id=stateId(id);e.x1=x1;e.y1=y1;e.x2=x2;e.y2=y2;e.bottom=lo;e.top=hi;e.actions=std::move(actions);m_scriptEvents.push_back(std::move(e));
  };
  auto action=[](A type,StateId id=0,int value=0,float amount=0.f){ScriptAction a;a.type=type;a.id=id;a.value=value;a.amount=amount;return a;};
  std::vector<Staircase> stairs;
  // === DYNAMIC_CAMPAIGN_MAPS_START ===
  // Each standalone slot is intentionally an independent if block so the
  // injector can replace one level without rewriting adjacent campaign maps.
  // === DYNAMIC_CAMPAIGN_MAPS_END ===
  buildLayers(stairs);return;
 }
 throw std::runtime_error("No builder for campaign map "+std::to_string(m_level));
}
void World::buildLights(int startX,int startY){
 for(int y=startY;y<Height-1;y+=4)for(int x=startX;x<Width-1;x+=5)if(tile(x,y)!='#')
  for(const auto&span:spansAt(x,y))if(span.ceiling-span.floor>1.8f)m_lights.push_back({{x+.5f,y+.35f},span.ceiling-.15f});
}
std::size_t World::terrainSampleIndex(int sx,int sy,int sz)const{
 return size_t((sz*TerrainSamplesY+sy)*TerrainSamplesX+sx);
}
void World::buildTerrainBounds(){
 m_terrainRevision=nextTerrainRevision.fetch_add(1,std::memory_order_relaxed);
 m_terrainTops.assign(size_t(TerrainSamplesX)*TerrainSamplesY,255);
 for(int z=0;z<TerrainSamplesZ;++z)for(int y=0;y<TerrainSamplesY;++y)for(int x=0;x<TerrainSamplesX;++x)
  if(m_terrainDensity[terrainSampleIndex(x,y,z)]>0)m_terrainTops[size_t(y*TerrainSamplesX+x)]=static_cast<std::uint8_t>(z);
}
float World::outskirtHeight(float worldX,float worldY,float outside)const{
 const auto* shape=terrainShape();if(!shape)return 0;
 float t=std::clamp(outside/std::max(1.f,shape->horizonDepth*.6f),0.f,1.f);
 return authoredTerrainHeight(*shape,worldX,worldY)+shape->horizonRise*t*t*(3-2*t);
}
int World::groundKind(float worldX,float worldY)const{
 if(!m_exclusionTerrain||!m_mapData||!m_mapData->terrain||m_mapData->terrain->ground.empty())return -1;
 return authoredGroundKind(*m_mapData->terrain,worldX,worldY);
}
void World::buildTerrain(){
 m_terrain.clear();
 const size_t samples=size_t(TerrainSamplesX)*TerrainSamplesY*TerrainSamplesZ;
 m_terrainDensity.assign(samples,0);m_terrainMaterial.assign(samples,0);
 const auto origin=definition().origin;
 for(int sz=0;sz<TerrainSamplesZ;++sz)for(int sy=0;sy<TerrainSamplesY;++sy)for(int sx=0;sx<TerrainSamplesX;++sx){
  int lx=sx-TerrainBorder,ly=sy-TerrainBorder,z=TerrainMinZ-1+sz;
  int wx=int(origin.x)+lx,wy=int(origin.y)+ly;
  bool solid;std::uint8_t material;
  if(m_exclusionTerrain&&m_mapData&&m_mapData->terrain){
   // Authored campaign terrain: soil on top, rock beneath and on the high,
   // steep ground where the surface breaks away from its neighbours.
   const auto& shape=*m_mapData->terrain;int top=authoredTerrainTop(shape,wx,wy);solid=z<=top;
   int relief=std::max({std::abs(authoredTerrainTop(shape,wx+1,wy)-top),std::abs(authoredTerrainTop(shape,wx,wy+1)-top)});
   // Painted (zone) terrain keeps its cover on high ground; only sharp relief is rock.
   material=z<top-1||relief>=2||(shape.ground.empty()&&top>=shape.base+shape.amplitude*.7f)?TerrainRock:TerrainSoil;
  }else{
   solid=m_exclusionTerrain?authoredExclusionVoxel(wx,wy,z):authoredAshfallVoxel(wx,wy,z);
   material=m_exclusionTerrain?authoredExclusionMaterial(wx,wy,z):authoredAshfallMaterial(wx,wy,z);
  }
  m_terrainDensity[terrainSampleIndex(sx,sy,sz)]=solid?32:-32;
  m_terrainMaterial[terrainSampleIndex(sx,sy,sz)]=material;
 }

 buildTerrainBounds();
 constexpr int corner[8][3]={{0,0,0},{1,0,0},{0,1,0},{1,1,0},{0,0,1},{1,0,1},{0,1,1},{1,1,1}};
 constexpr int edges[12][2]={{0,1},{1,3},{3,2},{2,0},{4,5},{5,7},{7,6},{6,4},{0,4},{1,5},{2,6},{3,7}};
 constexpr int cellsX=TerrainSamplesX-1,cellsY=TerrainSamplesY-1,cellsZ=TerrainSamplesZ-1;
 auto cellSlot=[=](int cx,int cy,int cz){return size_t((cz*cellsY+cy)*cellsX+cx);};
 std::vector<int> cellVertex(size_t(cellsX)*cellsY*cellsZ,-1);
 std::vector<TerrainVertex> vertices;
 std::vector<std::uint8_t> materials;
 vertices.reserve(Width*Height*10);materials.reserve(Width*Height*10);
 auto sample=[&](int x,int y,int z){
  int sx=x+TerrainBorder,sy=y+TerrainBorder,sz=z-(TerrainMinZ-1);
  return float(m_terrainDensity[terrainSampleIndex(sx,sy,sz)])/32.f;
 };
 auto sampleMaterial=[&](int x,int y,int z){
  int sx=x+TerrainBorder,sy=y+TerrainBorder,sz=z-(TerrainMinZ-1);
  return m_terrainMaterial[terrainSampleIndex(sx,sy,sz)];
 };
 auto cellIndex=[&](int x,int y,int z)->int{
  if(x<-1||x>Width||y<-1||y>Height||z<TerrainMinZ-1||z>TerrainMaxZ)return -1;
  return cellVertex[cellSlot(x+1,y+1,z-(TerrainMinZ-1))];
 };

 for(int cz=0;cz<cellsZ;++cz)for(int cy=0;cy<cellsY;++cy)for(int cx=0;cx<cellsX;++cx){
  int x=cx-1,y=cy-1,z=TerrainMinZ-1+cz;float d[8];bool positive=false,negative=false;
  for(int i=0;i<8;++i){d[i]=sample(x+corner[i][0],y+corner[i][1],z+corner[i][2]);positive|=d[i]>0;negative|=d[i]<=0;}
  if(!positive||!negative)continue;
  float px=0,py=0,pz=0;int crossings=0;std::uint8_t material=0;float strongest=0;
  for(int i=0;i<8;++i)if(d[i]>strongest){strongest=d[i];material=sampleMaterial(x+corner[i][0],y+corner[i][1],z+corner[i][2]);}
  for(const auto&e:edges){
   int a=e[0],b=e[1];if((d[a]>0)==(d[b]>0))continue;
   float t=d[a]/(d[a]-d[b]);
   px+=x+corner[a][0]+(corner[b][0]-corner[a][0])*t;
   py+=y+corner[a][1]+(corner[b][1]-corner[a][1])*t;
   pz+=z+corner[a][2]+(corner[b][2]-corner[a][2])*t;
   ++crossings;
  }
  if(!crossings)continue;
  float inv=1.f/crossings;TerrainVertex v{px*inv,py*inv,pz*inv,(origin.x+px*inv)*.18f,(origin.y+py*inv)*.18f};
  int index=int(vertices.size());vertices.push_back(v);materials.push_back(material);cellVertex[cellSlot(cx,cy,cz)]=index;
 }
 auto emit=[&](int ia,int ib,int ic,int id,float nx,float ny,float nz,std::uint8_t material){
  if(ia<0||ib<0||ic<0||id<0)return;
  auto a=vertices[size_t(ia)],b=vertices[size_t(ib)],c=vertices[size_t(ic)],d=vertices[size_t(id)];
  float ux=b.x-a.x,uy=b.y-a.y,uz=b.z-a.z,vx=c.x-a.x,vy=c.y-a.y,vz=c.z-a.z;
  float cx=uy*vz-uz*vy,cy=uz*vx-ux*vz,cz=ux*vy-uy*vx;
  if(cx*nx+cy*ny+cz*nz>=0){m_terrain.push_back({a,b,c,material});m_terrain.push_back({a,c,d,material});}
  else {m_terrain.push_back({a,d,c,material});m_terrain.push_back({a,c,b,material});}
 };
 for(int z=TerrainMinZ;z<=TerrainMaxZ;++z)for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
  float a=sample(x,y,z),b=sample(x+1,y,z);if((a>0)!=(b>0)){
   auto mat=a>0?sampleMaterial(x,y,z):sampleMaterial(x+1,y,z);float n=a>0?1.f:-1.f;
   emit(cellIndex(x,y-1,z-1),cellIndex(x,y,z-1),cellIndex(x,y,z),cellIndex(x,y-1,z),n,0,0,mat);
  }
  a=sample(x,y,z);b=sample(x,y+1,z);if((a>0)!=(b>0)){
   auto mat=a>0?sampleMaterial(x,y,z):sampleMaterial(x,y+1,z);float n=a>0?1.f:-1.f;
   emit(cellIndex(x-1,y,z-1),cellIndex(x-1,y,z),cellIndex(x,y,z),cellIndex(x,y,z-1),0,n,0,mat);
  }
  if(z<TerrainMaxZ){a=sample(x,y,z);b=sample(x,y,z+1);if((a>0)!=(b>0)){
   auto mat=a>0?sampleMaterial(x,y,z):sampleMaterial(x,y,z+1);float n=a>0?1.f:-1.f;
   emit(cellIndex(x-1,y-1,z),cellIndex(x,y-1,z),cellIndex(x,y,z),cellIndex(x-1,y,z),0,0,n,mat);
  }}
 }
}
float World::terrainDensity(float x,float y,float z)const{
 if(m_terrainDensity.empty())return -1;
 float sx=std::clamp(x+TerrainBorder,0.f,float(TerrainSamplesX-1));
 float sy=std::clamp(y+TerrainBorder,0.f,float(TerrainSamplesY-1));
 float sz=std::clamp(z-float(TerrainMinZ-1),0.f,float(TerrainSamplesZ-1));
 int x0=std::min(TerrainSamplesX-2,int(std::floor(sx))),y0=std::min(TerrainSamplesY-2,int(std::floor(sy))),z0=std::min(TerrainSamplesZ-2,int(std::floor(sz)));
 int x1=x0+1,y1=y0+1,z1=z0+1;float tx=sx-x0,ty=sy-y0,tz=sz-z0;
 auto d=[&](int ix,int iy,int iz){return float(m_terrainDensity[terrainSampleIndex(ix,iy,iz)])/32.f;};
 auto mix=[](float a,float b,float t){return a+(b-a)*t;};
 float a=mix(d(x0,y0,z0),d(x1,y0,z0),tx),b=mix(d(x0,y1,z0),d(x1,y1,z0),tx);
 float c=mix(d(x0,y0,z1),d(x1,y0,z1),tx),e=mix(d(x0,y1,z1),d(x1,y1,z1),tx);
 return mix(mix(a,b,ty),mix(c,e,ty),tz);
}
namespace {
// One vertical query reuses its exact XY interpolation and density layers.
// The cache lives only on this query's stack: edits, chunks and callers cannot
// observe stale terrain, and concurrent read-only queries never share state.
class TerrainColumn {
 static constexpr int X=World::Width+3,Y=World::Height+3,Z=World::TerrainMaxZ-World::TerrainMinZ+3;
 const std::vector<std::int8_t>& samples;
 int base;float tx,ty;
 float upperAir=std::numeric_limits<float>::infinity();
 mutable std::array<float,Z> layers;
 mutable std::uint64_t ready=0;
 float layer(int z)const{
  auto bit=std::uint64_t{1}<<z;if(ready&bit)return layers[size_t(z)];
  int at=z*X*Y+base;
  auto d=[&](int offset){return float(samples[size_t(at+offset)])/32.f;};
  auto mix=[](float a,float b,float t){return a+(b-a)*t;};
  float a=mix(d(0),d(1),tx),b=mix(d(X),d(X+1),tx);
  float value=mix(a,b,ty);layers[size_t(z)]=value;ready|=bit;return value;
 }
public:
 TerrainColumn(const std::vector<std::int8_t>& data,const std::vector<std::uint8_t>& tops,float x,float y):samples(data){
  float sx=std::clamp(x+1,0.f,float(X-1)),sy=std::clamp(y+1,0.f,float(Y-1));
  int ix=std::min(X-2,int(std::floor(sx))),iy=std::min(Y-2,int(std::floor(sy)));
  base=iy*X+ix;tx=sx-ix;ty=sy-iy;
  if(tops.size()==size_t(X*Y)){int highest=-1;
   for(int offset:{0,1,X,X+1})if(tops[size_t(base+offset)]!=255)highest=std::max(highest,int(tops[size_t(base+offset)]));
   // Tighten the conservative corner bound at this exact XY, using the
   // same interpolated layers as the reference density. Above the highest
   // positive layer the field is linear, so its zero crossing bounds air.
   // A positive final layer remains solid under clamped queries above it.
   if(highest==Z-1&&layer(highest)>0)return;
   while(highest>=0&&layer(highest)<=0)--highest;
   if(highest<0)upperAir=float(World::TerrainMinZ-1);
   else{float solid=layer(highest),air=layer(highest+1);
    upperAir=float(World::TerrainMinZ-1+highest)+solid/(solid-air)+.001f;
   }
  }
 }
 float airTop()const{return upperAir;}
 float operator()(float z)const{
  float sz=std::clamp(z-float(World::TerrainMinZ-1),0.f,float(Z-1));
  int iz=std::min(Z-2,int(std::floor(sz)));float tz=sz-iz;
  float a=layer(iz),b=layer(iz+1);return a+(b-a)*tz;
 }
};
template<class Density> float terrainFloor(float feet,const Density& density,float airTop=std::numeric_limits<float>::infinity()){
 float high=std::min(feet+.04f,float(World::TerrainMaxZ+1));
 if(high>airTop)high-=std::floor((high-airTop)*8)*.125f;
 float highD=density(high);
 while(high-.125f>=World::TerrainMinZ-1){
  // Density is linear between integer Z samples. Skip only samples in
  // that same slab, retaining the original eighth-metre query lattice.
  int steps=std::max(1,int((high-std::floor(high))*8));
  float low=high-steps*.125f,lowD=density(low);
  if(highD<=0&&lowD>0){
   for(float probe=high-.125f;probe>low;probe-=.125f){if(density(probe)>0){low=probe;break;}high=probe;}
   float solid=low,air=high;for(int i=0;i<8;++i){float mid=(solid+air)*.5f;if(density(mid)>0)solid=mid;else air=mid;}return (solid+air)*.5f;
  }
  high=low;highD=lowD;
 }
 return float(World::TerrainMinZ-1);
}
template<class Density> float terrainCeiling(float feet,float limit,const Density& density){
 float low=feet+.03f,lowD=density(low);if(lowD>0)return feet;
 // Only a surface below the hull's top can reject that hull. Keep one
 // full original sample beyond the limit to retain crossing/refinement ties.
 float end=std::min(float(World::TerrainMaxZ+1),limit+.125f);
 while(low+.125f<=end){
  int steps=std::max(1,int((std::floor(low)+1-low)*8));
  steps=std::min(steps,int((end-low)*8));
  // Rounding can pass the loop test with under one sample left to `end`
  // (a 1.78 m hull from some feet): that is the end, not a step of zero.
  if(steps<1)break;
  float high=low+steps*.125f,highD=density(high);
  if(lowD<=0&&highD>0){
   for(float probe=low+.125f;probe<high;probe+=.125f){if(density(probe)>0){high=probe;break;}low=probe;}
   float air=low,solid=high;for(int i=0;i<8;++i){float mid=(air+solid)*.5f;if(density(mid)>0)solid=mid;else air=mid;}return (air+solid)*.5f;
  }
  low=high;lowD=highD;
 }
 return 128.f;
}
}
float World::terrainSurfaceBelow(float x,float y,float feet)const{
 if(m_terrainDensity.empty())return 0;
 return TerrainSurfaceCache::below(m_terrainRevision,x,y,feet,[&]{TerrainColumn density(m_terrainDensity,m_terrainTops,x,y);return terrainFloor(feet,density,density.airTop());});
}
float World::terrainSurfaceAbove(float x,float y,float feet)const{
 if(m_terrainDensity.empty())return 128.f;
 TerrainColumn density(m_terrainDensity,m_terrainTops,x,y);
 if(feet+.03f>=density.airTop())return 128.f;
 return terrainCeiling(feet,128.f,density);
}
float World::floorHeight(float x,float y)const{
 if(outdoors())return hasTerrain()?terrainSurfaceBelow(x,y,float(TerrainMaxZ+1)):(m_layers.empty()?0.f:m_layers.front().elevation);
 for(const auto& water:m_waterVolumes)if(x>=water.x1&&x<water.x2&&y>=water.y1&&y<water.y2){
  float shore=std::min({x-water.x1,water.x2-x,y-water.y1,water.y2-y});
  float bank=m_layers.empty()?-9.f:m_layers.front().elevation;
  return bank+(water.bed-bank)*std::clamp(shore/.85f,0.f,1.f);
 }
 if(m_mapData)return m_layers.empty()?definition().spawnHeight:m_layers.front().elevation;
 if(m_level>=6)return m_layers.empty()?definition().spawnHeight:m_layers.front().elevation;
 return -9.f; // Coolant Return
}
float World::waterSurface(float x,float y)const{
 for(const auto& water:m_waterVolumes)if(x>=water.x1&&x<water.x2&&y>=water.y1&&y<water.y2)return water.surface;
 return -1000.f;
}
float World::ceilingHeight(float x,float y)const{
 if(outdoors())return 128.f;
 if(m_mapData)return definition().ceiling;
 if(m_level>=6)return definition().ceiling;
 return -5.6f; // Coolant Return
}
void World::buildCollisionCandidates()const{
 m_fixtureRotation.clear();
 m_collisionCells={};m_collisionAll={};for(auto&cells:m_collisionCells)cells.resize(Width*Height);
 auto add=[&](int kind,size_t index,float x1,float y1,float x2,float y2){
  m_collisionAll[kind].push_back(index);
  for(int y=std::max(0,int(std::floor(y1-.001f)));y<=std::min(Height-1,int(std::floor(y2+.001f)));++y)
   for(int x=std::max(0,int(std::floor(x1-.001f)));x<=std::min(Width-1,int(std::floor(x2+.001f)));++x)m_collisionCells[kind][y*Width+x].push_back(index);
 };
 for(size_t i=0;i<m_fixtures.size();++i){
  const auto& f=m_fixtures[i];float cosine=std::cos(f.yaw),sine=std::sin(f.yaw);
  m_fixtureRotation.push_back({cosine,sine});float c=std::fabs(cosine),s=std::fabs(sine);
  float hx=(f.width*c+f.depth*s)*.5f,hy=(f.width*s+f.depth*c)*.5f;
  // solid expands each local fixture axis by .2, including rotated corners.
  hx=std::max(hx,(f.width*f.core*.5f+.2f)*c+(f.depth*f.core*.5f+.2f)*s);
  hy=std::max(hy,(f.width*f.core*.5f+.2f)*s+(f.depth*f.core*.5f+.2f)*c);
  add(0,i,f.position.x-hx,f.position.y-hy,f.position.x+hx,f.position.y+hy);
  // wallSpaceFree tests only the query's axes. Preserve its conservative
  // projected-rectangle hits, which can extend beyond the rotated footprint.
  float radius=(f.width+f.depth)*.5f;
  add(3,i,f.position.x-radius,f.position.y-radius,f.position.x+radius,f.position.y+radius);
 }
 for(size_t i=0;i<m_props.size();++i){const auto&p=m_props[i];add(1,i,p.position.x-p.halfSize.x-.2f,p.position.y-p.halfSize.y-.2f,p.position.x+p.halfSize.x+.2f,p.position.y+p.halfSize.y+.2f);}
 for(size_t i=0;i<m_terminals.size();++i){const auto&t=m_terminals[i];
  // Lift-control consoles can travel without changing the terminal count.
  // Include them in every cell; the original exact tests still decide hits.
  if(t.control){add(2,i,0,0,Width,Height);continue;}
  // solid uses the terminal integer cell even for paper on a wall.
  bool desk=benchTerminal(*this,t);
  float cx=float(int(t.position.x)),cy=float(int(t.position.y));
  add(2,i,std::min(t.position.x-(desk?.48f:.27f),cx<=0?cx-1:cx),std::min(t.position.y-(desk?.36f:.18f),cy<=0?cy-1:cy),std::max(t.position.x+(desk?.42f:.27f),cx>=0?cx+1:cx),std::max(t.position.y+(desk?.36f:.18f),cy>=0?cy+1:cy));
 }
}
std::span<const size_t> World::collisionCandidates(int kind,float x,float y)const{
 if(m_collisionCells[0].empty()||m_collisionAll[0].size()!=m_fixtures.size()||m_collisionAll[1].size()!=m_props.size()||m_collisionAll[2].size()!=m_terminals.size())buildCollisionCandidates();
 int ix=int(std::floor(x)),iy=int(std::floor(y));
 if(!m_collisionCandidatesEnabled||ix<0||iy<0||ix>=Width||iy>=Height)return m_collisionAll[kind];
 return m_collisionCells[kind][iy*Width+ix];
}
bool World::insideFixtureCandidate(size_t index,float x,float y,float margin)const{
 const auto& f=m_fixtures[index];if(!m_collisionCandidatesEnabled)return insideFixture(f,x,y,margin);const auto& rotation=m_fixtureRotation[index];
 float dx=x-f.position.x,dy=y-f.position.y,c=rotation.x,s=rotation.y;
 return std::fabs(dx*c-dy*s)<f.width*f.core*.5f+margin&&std::fabs(dx*s+dy*c)<f.depth*f.core*.5f+margin;
}
std::span<const size_t> World::wallCandidates(Vec2 center,Vec2 along,float width,std::vector<size_t>& scratch)const{
 auto all=collisionCandidates(3,-1,-1);
 // The exact test uses the supplied basis without normalizing it.
 float norm=dot(along,along);if(!(norm>0))return all;
 Vec2 normal{-along.y,along.x};float tangent=width*.5f+.02f;
 float hx=(std::fabs(along.x)*tangent+std::fabs(normal.x)*.06f)/norm;
 float hy=(std::fabs(along.y)*tangent+std::fabs(normal.y)*.06f)/norm;
 float x1=center.x-hx,y1=center.y-hy,x2=center.x+hx,y2=center.y+hy;
 if(!m_collisionCandidatesEnabled||x1<0||y1<0||x2>=Width||y2>=Height)return all;
 scratch.clear();for(int y=int(std::floor(y1));y<=int(std::floor(y2));++y)for(int x=int(std::floor(x1));x<=int(std::floor(x2));++x){const auto& cell=m_collisionCells[3][y*Width+x];scratch.insert(scratch.end(),cell.begin(),cell.end());}
 std::sort(scratch.begin(),scratch.end());scratch.erase(std::unique(scratch.begin(),scratch.end()),scratch.end());return scratch;
}
namespace {
bool collisionVerify(){static const bool enabled=[] {const char* value=std::getenv("RAWMETAL_COLLISION_VERIFY");return value&&std::string_view(value)=="1";}();return enabled;}
struct CollisionVerification {
 std::array<uint64_t,4> queries{},mismatches{};
 ~CollisionVerification(){const char* names[]={"solid","wall_space_free","clearance_above","spans_at"};for(size_t i=0;i<queries.size();++i)if(queries[i])std::fprintf(stderr,"collision_verify %s queries=%llu mismatches=%llu\n",names[i],static_cast<unsigned long long>(queries[i]),static_cast<unsigned long long>(mismatches[i]));}
};
thread_local CollisionVerification collisionVerification;
void verifyCollision(size_t kind,bool equal){++collisionVerification.queries[kind];if(!equal)++collisionVerification.mismatches[kind];}
bool sameFloat(float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
bool sameSpans(const std::vector<Span>& a,const std::vector<Span>& b){if(a.size()!=b.size())return false;for(size_t i=0;i<a.size();++i)if(!sameFloat(a[i].floor,b[i].floor)||!sameFloat(a[i].ceiling,b[i].ceiling)||a[i].flags!=b[i].flags)return false;return true;}
}
__declspec(noinline) bool World::testCollisionCandidates(){
 // A map with taped paper builds its grid once and keeps it.
 for(int level:{14,15,58}){World world(level);bool taped=false;for(const auto&t:world.terminals())taped|=t.paper()&&!t.paperOnBench();
  world.fits(12,12,world.floorHeight(12,12),1.f);const auto* cells=world.m_collisionCells[2].data();world.fits(6,6,world.floorHeight(6,6),1.f);
  if(!taped||world.m_collisionAll[2].size()!=world.terminals().size()||world.m_collisionCells[2].data()!=cells)return false;}
 for(int level:{0,1,3,5,6,7,8,9,14,16,20}){World world(level);std::vector<Vec2> samples;
  for(int y=-1;y<=Height;++y)for(int x=-1;x<=Width;++x)samples.push_back({x+.137f,y+.713f});
  for(const auto&f:world.fixtures())for(float u:{-.51f,-.49f,0.f,.49f,.51f})for(float v:{-.51f,-.49f,0.f,.49f,.51f}){float c=std::cos(f.yaw),s=std::sin(f.yaw);samples.push_back({f.position.x+u*f.width*c+v*f.depth*s,f.position.y-u*f.width*s+v*f.depth*c});}
  for(auto p:samples)for(float offset:{-.04f,.02f,.5f,1.5f,3.f})for(bool cavities:{false,true}){
   float feet=world.floorHeight(p.x,p.y)+offset;world.m_collisionCandidatesEnabled=true;bool optimized=world.fits(p.x,p.y,feet,.015f,false,cavities);float support=world.supportHeight(p.x,p.y,false,cavities),below=world.supportBelow(p.x,p.y,feet);
   world.m_collisionCandidatesEnabled=false;bool reference=world.fits(p.x,p.y,feet,.015f,false,cavities);
   if(optimized!=reference||support!=world.supportHeight(p.x,p.y,false,cavities)||below!=world.supportBelow(p.x,p.y,feet))return false;
  }
  world.m_collisionCandidatesEnabled=true;
  uint32_t random=uint32_t(level+1);auto unit=[&]{random=random*1664525u+1013904223u;return float(random>>8)/16777216.f;};
  for(int sample=0;sample<10000;++sample){Vec2 from{unit()*26-1,unit()*26-1},to=from+Vec2{unit()*8-4,unit()*8-4};float feet=world.floorHeight(from.x,from.y)+unit()*3-.1f,height=.3f+unit()*1.8f;
   if(world.structureSegmentClear(from,feet,to,height)!=world.structureSegmentClear(from,feet,to,height,false))return false;
  }
  for(const auto&s:world.structures())for(float offset:{-.2001f,-.1999f,0.f,.1999f,.2001f}){
   Vec2 from{s.x1+offset,(s.y1+s.y2)*.5f},to{s.x2+offset,from.y};float feet=s.bottom+.03f;
   if(world.structureSegmentClear(from,feet,to,1.6f)!=world.structureSegmentClear(from,feet,to,1.6f,false))return false;
  }
 }
 // Isolated leaf comparison uses the preserved returning signature on both
 // paths. A checksum keeps both timed loops observable under optimisation.
 {
  World racks(19);racks.buildCollisionCandidates();uint64_t checksum=0;
  for(bool indexed:{false,true})for(bool spans:{false,true}){
   auto start=std::chrono::steady_clock::now();
   constexpr int samples=200000;
   for(int i=0;i<samples;++i){int x=(i*17)%Width,y=(i/Width*13)%Height;
    if(spans){auto result=indexed?racks.spansAt(x,y):racks.spansAtReference(x,y);checksum+=result.size();}
    else checksum+=indexed?racks.solidIndexed(x+.5f,y+.5f):racks.solidReference(x+.5f,y+.5f);
   }
   double ns=std::chrono::duration<double,std::nano>(std::chrono::steady_clock::now()-start).count()/samples;
   std::fprintf(stderr,"racks_leaf %s %s ns_per_query=%.3f checksum=%llu\n",spans?"spans_at":"solid",indexed?"indexed":"reference",ns,static_cast<unsigned long long>(checksum));
  }
 }
 // Every compiled campaign chunk and every Ashfall chunk, at exact eighth-metre
 // positions and seeded jitter. The compiled campaign count is authoritative.
 uint64_t checked=0;
 for(WorldId id:{WorldId::Campaign,WorldId::Ashfall})for(int level=0;level<worldChunkCount(id);++level){
  World world(level,id);uint32_t seed=uint32_t(level+1);auto unit=[&]{seed=seed*1664525u+1013904223u;return float(seed>>8)/16777216.f;};
  world.buildCollisionCandidates();std::vector<Span> optimizedSpans;
  for(float doorState:{0.f,.5f,1.f}){
   for(size_t i=0;i<world.m_doors.size();++i)world.setDoor(int(i),doorState,false);
   for(int iy=0;iy<=Height*8;++iy)for(int ix=0;ix<=Width*8;++ix)for(int jitter=0;jitter<2;++jitter){
    float x=ix*.125f+(jitter?(unit()-.5f)*.125f:0),y=iy*.125f+(jitter?(unit()-.5f)*.125f:0);
    if(world.solidIndexed(x,y)!=world.solidReference(x,y)){std::fprintf(stderr,"solid mismatch %d %d %g %g\n",int(id),level,x,y);return false;}
    // spansAt accepts integer cells; exercise every cell including the border,
    // once per door state, rather than repeating the same integer query 128 times.
    if(!jitter&&ix%8==0&&iy%8==0){world.spansAtIndexed(ix/8,iy/8,optimizedSpans);if(!sameSpans(optimizedSpans,world.spansAtReference(ix/8,iy/8)))return false;}
    float floor=world.floorHeight(x,y);
    for(float offset:{-.04f,.02f,1.5f,3.f}){
     float feet=floor+offset;
     if(!sameFloat(world.clearanceAboveIndexed(x,y,feet),world.clearanceAboveReference(x,y,feet)))return false;
     float angle=unit()*2*kPi;Vec2 along{std::cos(angle),std::sin(angle)};
     if(world.wallSpaceFreeIndexed({x,y},along,1.2f,feet,feet+1.8f)!=world.wallSpaceFreeReference({x,y},along,1.2f,feet,feet+1.8f)){std::fprintf(stderr,"wall mismatch %d %d %g %g\n",int(id),level,x,y);return false;}
     ++checked;
    }
   }
  }
  std::fprintf(stderr,"collision differential world=%d chunk=%d checks=%llu mismatches=0\n",int(id),level,static_cast<unsigned long long>(checked));
 }
 // Compare terrain acceleration against the original fixed-step queries,
 // including synthetic stacked floors/overhangs and fractional starts.
 World terrain(0,WorldId::Ashfall);
 auto reference=[&](float x,float y,float feet,bool above){
  float start=above?feet+.03f:std::min(feet+.04f,float(TerrainMaxZ+1));
  float previous=start,d=terrain.terrainDensity(x,y,start);
  if(above&&d>0)return feet;
  for(float z=start+(above?.125f:-.125f);above?z<=TerrainMaxZ+1:z>=TerrainMinZ-1;z+=above?.125f:-.125f){
   float next=terrain.terrainDensity(x,y,z);
   if(d<=0&&next>0){float air=previous,solid=z;for(int i=0;i<8;++i){float mid=(air+solid)*.5f;if(terrain.terrainDensity(x,y,mid)>0)solid=mid;else air=mid;}return (air+solid)*.5f;}
   previous=z;d=next;
  }
  return above?128.f:float(TerrainMinZ-1);
 };
 for(int shape=0;shape<3;++shape){
  if(shape)for(int z=0;z<TerrainSamplesZ;++z)for(int y=0;y<TerrainSamplesY;++y)for(int x=0;x<TerrainSamplesX;++x){
   int value=shape==1?(((z+x/3+y/4)%7)<3?32:-32):((x*17+y*23+z*13)%65-32);
   terrain.m_terrainDensity[terrain.terrainSampleIndex(x,y,z)]=static_cast<int8_t>(value);
  }
  terrain.buildTerrainBounds();
  for(float x:{-100.f,-.1f,.001f,1.137f,7.51f,12.f,23.999f,24.1f,100.f})for(float y:{.013f,3.71f,12.5f,23.99f}){
   TerrainColumn column(terrain.m_terrainDensity,terrain.m_terrainTops,x,y);
   float surface=reference(x,y,float(TerrainMaxZ+1),false);
   for(float offset:{-.030f,-.02501f,-.025f,-.02499f,0.f,.030f}){float feet=surface+offset;
    if(terrain.terrainSurfaceBelow(x,y,feet+.03f)!=reference(x,y,feet+.03f,false))return false;
   }
   for(int z=-105;z<=176;++z){float feet=z*.125f+.037f;
    if(column(feet)!=terrain.terrainDensity(x,y,feet))return false;
    if(feet>=column.airTop()&&column(feet)>0)return false;
    float originalFloor=reference(x,y,feet+.03f,false);
    if(feet-.125f>=column.airTop()){
     if(feet<originalFloor-.025f||reference(x,y,feet,true)!=128.f)return false;
     for(float z=feet+.04f;z<feet+5.f;z+=.16f)if(terrain.terrainDensity(x,y,z)>0)return false;
    }
    if(terrain.terrainSurfaceBelow(x,y,feet)!=reference(x,y,feet,false)||terrain.terrainSurfaceAbove(x,y,feet)!=reference(x,y,feet,true))return false;
    float full=reference(x,y,feet,true);
    for(float height:{.01f,.4f,1.1f,1.6f,2.4f,5.f}){
     float bounded=terrainCeiling(feet,feet+height,[&](float probe){return terrain.terrainDensity(x,y,probe);});
     if((feet+height>full+.005f)!=(feet+height>bounded+.005f))return false;
    }
   }
  }
 }
 // Retained height results are exact, copy-safe and invalidated when an
 // immutable density field is rebuilt. A reference query bypasses all memos.
 if(TerrainSurfaceCache::enabled.load(std::memory_order_relaxed)){
  TerrainSurfaceCache::resetStatistics();World original(0,WorldId::Ashfall);
  for(int i=0;i<1000;++i){float x=float(i%31)*.731f-.15f,y=float(i%29)*.811f-.2f,z=float(i%7)-2.037f;
   float cached=original.terrainSurfaceBelow(x,y,z),again=original.terrainSurfaceBelow(x,y,z);
   CollisionQueryBatch reference(&original,false);float full=original.terrainSurfaceBelow(x,y,z);
   if(std::bit_cast<uint32_t>(cached)!=std::bit_cast<uint32_t>(again)||std::bit_cast<uint32_t>(cached)!=std::bit_cast<uint32_t>(full))return false;
  }
  if(TerrainSurfaceCache::statistics().hits<1000)return false;
  float before=original.terrainSurfaceBelow(12,12,13);World copy=original;
  auto hits=TerrainSurfaceCache::statistics().hits;if(copy.terrainSurfaceBelow(12,12,13)!=before||TerrainSurfaceCache::statistics().hits!=hits+1)return false;
  std::fill(copy.m_terrainDensity.begin(),copy.m_terrainDensity.end(),int8_t(-32));copy.buildTerrainBounds();
  if(copy.terrainSurfaceBelow(12,12,13)!=float(TerrainMinZ-1)||original.terrainSurfaceBelow(12,12,13)!=before)return false;
  auto queries=TerrainSurfaceCache::statistics().queries;{CollisionQueryBatch reference(&original,false);original.terrainSurfaceBelow(12,12,13);}
  if(TerrainSurfaceCache::statistics().queries!=queries)return false;
 }
 return true;
}
float World::supportHeight(float x,float y,bool dynamic,bool shelfCavities)const{
 for(auto index:collisionCandidates(0,x,y)){const auto&fixture=m_fixtures[index];if(fixture.solid&&fixture.base<.025f&&insideFixtureCandidate(index,x,y)){
  if(shelfCavities&&fixture.model==7)continue;
  return floorHeight(fixture.position.x,fixture.position.y)+fixture.base+fixture.height;
 }}
 for(auto index:collisionCandidates(1,x,y)){const auto&p=m_props[index];if(p.base<.025f&&std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y)return floorHeight(p.position.x,p.position.y)+p.base+p.height;}
 for(auto index:collisionCandidates(2,x,y)){const auto&terminal=m_terminals[index];if((dynamic||!hasLift()||!terminal.control)&&terminal.z==0&&insideTerminal(*this,terminal,x,y))return floorHeight(terminal.position.x,terminal.position.y)+terminalHeight(*this,terminal,x,y);}
 float floor=floorHeight(x,y);switch(tile(int(std::floor(x)),int(std::floor(y)))){
 case '#':return wallHeight(int(std::floor(x)),int(std::floor(y)));case 'C':return floor+.60f;case 'B':return floor+1.1f;
 case 'T':return floor+2.62f;default:return floor;
 }
}
bool World::doorBlocks(float x,float y,float feet,float height)const{
 perf::count(perf::DoorBlocks);
 for(size_t index=0;index<m_doors.size();++index){const auto&door=m_doors[index];
  float open=m_doorPlan.open&&index<m_doorPlan.open->size()&&(*m_doorPlan.open)[index]?door.maxOpen:door.open;
  if(door.swinging){
   float angle=open*kPi*.5f*door.swing,width=door.right-door.left;
   Vec2 hinge{door.left,door.y},axis{std::cos(angle),std::sin(angle)};
   Vec2 delta=Vec2{x,y}-hinge;
   float along=std::clamp(dot(delta,axis),0.f,width);
   Vec2 nearest=hinge+axis*along;
   if(lengthSq(Vec2{x,y}-nearest)<.15f*.15f){float base=floorHeight((door.left+door.right)*.5f,door.y)+door.z;
    if(feet+height>base+.015f&&feet<base+2.35f)return true;
   }
  }else if(x>door.left&&x<door.right&&std::fabs(y-door.y)<.13f){float base=floorHeight((door.left+door.right)*.5f,door.y)+door.z;
   if(feet+height>base+open*2.65f+.015f&&feet<base+open*2.65f+2.48f)return true;
  }
 }
 return false;
}
float World::clearanceHeight(float x,float y,float feet)const{
 float height=ceilingHeight(x,y);for(auto&door:m_doors)if(x>door.left&&x<door.right&&std::fabs(y-door.y)<.62f){
  float header=floorHeight((door.left+door.right)*.5f,door.y)+door.z+2.5f;if(feet<header-.05f)height=std::min(height,header);}
 return height;
}
void World::buildLightOcclusion()const{
 m_lightSolids.clear();m_lightNodes.clear();
 auto box=[&](float x1,float y1,float x2,float y2,float bottom,float top){if(x2<=x1||y2<=y1||top<=bottom)return;m_lightSolids.push_back({{(x1+x2)*.5f,(y1+y2)*.5f,(bottom+top)*.5f},{(x2-x1)*.5f,(y2-y1)*.5f,(top-bottom)*.5f}});};
 auto oriented=[&](Vec2 p,float width,float depth,float bottom,float top,float yaw){m_lightSolids.push_back({{p.x,p.y,(bottom+top)*.5f},{width*.5f,depth*.5f,(top-bottom)*.5f},std::cos(yaw),std::sin(yaw)});};
 if(m_mapData||level()>=6)box(0,0,Width,Height,-100,floorHeight(12,12)-.025f);
 else for(int y=0;y<Height*2;++y)for(int x=0;x<Width*2;++x)box(x*.5f,y*.5f,(x+1)*.5f,(y+1)*.5f,-100,floorHeight(x*.5f+.25f,y*.5f+.25f)-.025f);
 // Adjacent ceiling cells at exactly the same height have one solid union.
 // Keep every height boundary and the original one-metre thickness, while
 // avoiding hundreds of redundant BVH leaves in large constant-height halls.
 std::array<float,Width*Height> ceilings{};std::array<bool,Width*Height> covered{};
 for(int y=0;y<Height;++y)for(int x=0;x<Width;++x)ceilings[size_t(y*Width+x)]=ceilingHeight(x+.5f,y+.5f);
 for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){
  if(covered[size_t(y*Width+x)])continue;float ceiling=ceilings[size_t(y*Width+x)];int right=x+1,bottom=y+1;
  while(right<Width&&!covered[size_t(y*Width+right)]&&ceilings[size_t(y*Width+right)]==ceiling)++right;
  while(bottom<Height){bool same=true;for(int column=x;column<right;++column)if(covered[size_t(bottom*Width+column)]||ceilings[size_t(bottom*Width+column)]!=ceiling){same=false;break;}if(!same)break;++bottom;}
  for(int row=y;row<bottom;++row)for(int column=x;column<right;++column)covered[size_t(row*Width+column)]=true;
  box(float(x),float(y),float(right),float(bottom),ceiling,ceiling+1);
 }
 for(int y=0;y<Height;++y)for(int x=0;x<Width;++x){float floor=floorHeight(x+.5f,y+.5f);char t=tile(x,y);
  if(t=='#'||t=='C'||t=='B'||t=='T')box(float(x),float(y),float(x+1),float(y+1),floor,t=='#'?wallHeight(x,y):floor+(t=='C'?.60f:t=='B'?1.1f:2.62f));
 }
 for(const auto&s:m_structures){
  // Glass and painted floor markings do not cast an opaque box shadow.
  if(s.material==17||((s.material==16||s.material==14||s.material==13)&&s.top-s.bottom<.04f))continue;
  if(!s.rail){box(s.x1,s.y1,s.x2,s.y2,s.bottom,s.top);continue;}
  // Movement uses the whole guardrail envelope; light passes between its bars.
  if(s.top-s.bottom>1.5f){
   if(hasLift()){box(s.x1,s.y1,s.x2,s.y2,s.bottom,s.bottom+.16f);box(s.x1,s.y1,s.x2,s.y2,s.bottom+.92f,s.bottom+.96f);
    int bars=std::max(1,int(std::max(s.x2-s.x1,s.y2-s.y1)/.4f));for(int i=1;i<=bars;++i){float t=float(i)/(bars+1),x=s.x1+(s.x2-s.x1)*t,y=s.y1+(s.y2-s.y1)*t;box(x-.012f,y-.012f,x+.012f,y+.012f,s.bottom+.16f,s.top-.07f);}
   }else box(s.x1+.012f,s.y1+.012f,s.x2-.012f,s.y2-.012f,s.bottom+.012f,s.top-.08f);
  }
  box(s.x1,s.y1,s.x2,s.y2,s.top-.07f,s.top);box(s.x1,s.y1,s.x1+.055f,s.y1+.055f,s.bottom,s.top-.07f);box(s.x2-.055f,s.y2-.055f,s.x2,s.y2,s.bottom,s.top-.07f);
 }
 for(const auto&f:m_fixtures)if(f.solid){float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7){for(float tier:ShelfTiers){float top=base+f.height*tier;oriented(f.position,f.width,f.depth,top-.045f,top,f.yaw);}
   float c=std::cos(f.yaw),s=std::sin(f.yaw);for(float x:{-f.width*.5f+.02f,f.width*.5f-.02f})for(float y:{-f.depth*.5f+.02f,f.depth*.5f-.02f})oriented({f.position.x+x*c+y*s,f.position.y-x*s+y*c},.04f,.04f,base,base+f.height,f.yaw);
  }else if(f.model==19||f.model==18){
   float c=std::cos(f.yaw),s=std::sin(f.yaw),top=f.model==19?base+f.height:base+f.height*.48f;
   oriented(f.position,f.width,f.depth,top-.05f,top,f.yaw);
   for(float x:{-f.width*.5f+.05f,f.width*.5f-.05f})for(float y:{-f.depth*.5f+.05f,f.depth*.5f-.05f})oriented({f.position.x+x*c+y*s,f.position.y-x*s+y*c},.05f,.05f,base,top-.05f,f.yaw);
   if(f.model==18){Vec2 back{f.position.x+(f.depth*.5f-.03f)*s,f.position.y+(f.depth*.5f-.03f)*c};oriented(back,f.width,.05f,top,base+f.height,f.yaw);}
  }else oriented(f.position,f.width,f.depth,base,base+f.height,f.yaw);
 }
 for(const auto&p:m_props){float base=floorHeight(p.position.x,p.position.y)+p.base;box(p.position.x-p.halfSize.x,p.position.y-p.halfSize.y,p.position.x+p.halfSize.x,p.position.y+p.halfSize.y,base,base+p.height);}
 for(const auto&t:m_terminals){if((hasLift()&&t.control)||(t.paper()&&!t.paperOnBench()))continue;float base=floorHeight(t.position.x,t.position.y)+t.z;
  if(benchTerminal(*this,t)&&t.paper())box(t.position.x-.48f,t.position.y-.36f,t.position.x+.42f,t.position.y+.36f,base+.74f,base+.815f);
  else if(benchTerminal(*this,t)){box(t.position.x-.48f,t.position.y-.36f,t.position.x+.42f,t.position.y+.36f,base+.74f,base+.815f);oriented(t.position,.386509f,.53235f,base+.815f,base+1.32005f,level()==6?-kPi*.5f:t.yaw);}
  else oriented(t.position,.54f,.36f,base,base+.95f,t.yaw);
 }
 for(const auto&d:m_doors){float base=floorHeight((d.left+d.right)*.5f,d.y)+d.z;box(d.left,d.y-.10f,d.right,d.y+.10f,base+2.5f,ceilingHeight((d.left+d.right)*.5f,d.y));}
 auto bounds=[](const LightSolid&s){float hx=std::fabs(s.cosine)*s.half.x+std::fabs(s.sine)*s.half.y,hy=std::fabs(s.sine)*s.half.x+std::fabs(s.cosine)*s.half.y;return std::pair{s.center-Point3{hx,hy,s.half.z},s.center+Point3{hx,hy,s.half.z}};};
 auto build=[&](auto&&self,int begin,int end)->int{int index=int(m_lightNodes.size());LightNode node;node.begin=begin;node.end=end;node.minimum={10000,10000,10000};node.maximum={-10000,-10000,-10000};
  for(int i=begin;i<end;++i){auto [lo,hi]=bounds(m_lightSolids[i]);node.minimum={std::min(node.minimum.x,lo.x),std::min(node.minimum.y,lo.y),std::min(node.minimum.z,lo.z)};node.maximum={std::max(node.maximum.x,hi.x),std::max(node.maximum.y,hi.y),std::max(node.maximum.z,hi.z)};}
  m_lightNodes.push_back(node);if(end-begin<=4)return index;
  auto extent=node.maximum-node.minimum;int axis=extent.x>=extent.y&&extent.x>=extent.z?0:extent.y>=extent.z?1:2;auto coordinate=[&](const LightSolid&s){return axis==0?s.center.x:axis==1?s.center.y:s.center.z;};int middle=(begin+end)/2;
  std::nth_element(m_lightSolids.begin()+begin,m_lightSolids.begin()+middle,m_lightSolids.begin()+end,[&](const auto&a,const auto&b){return coordinate(a)<coordinate(b);});
  int left=self(self,begin,middle),right=self(self,middle,end);m_lightNodes[index].left=left;m_lightNodes[index].right=right;return index;
 };
 if(!m_lightSolids.empty())build(build,0,int(m_lightSolids.size()));
}
bool World::lightRayClear(Vec2 a,float az,Vec2 b,float bz)const{
 perf::count(perf::LightRay);
 // Terrain and shaped water beds retain the volumetric reference trace.
 if(outdoors()||!m_waterVolumes.empty())return rayClear(a,az,b,bz,false,false,true);
 if(m_lightNodes.empty())buildLightOcclusion();
 Point3 origin{a.x,a.y,az},delta{b.x-a.x,b.y-a.y,bz-az};
 auto intersects=[](Point3 o,Point3 d,Point3 lo,Point3 hi){float nearT=.0001f,farT=.9999f;
  auto slab=[&](float p,float v,float lower,float upper){if(std::fabs(v)<.000001f)return p>=lower-.00001f&&p<=upper+.00001f;float t1=(lower-p)/v,t2=(upper-p)/v;if(t1>t2)std::swap(t1,t2);nearT=std::max(nearT,t1);farT=std::min(farT,t2);return farT>nearT;};
  return slab(o.x,d.x,lo.x,hi.x)&&slab(o.y,d.y,lo.y,hi.y)&&slab(o.z,d.z,lo.z,hi.z);
 };
 if(m_lightNodes.empty())return true;std::array<int,64> pending{};int count=1;pending[0]=0;
 while(count){const auto&node=m_lightNodes[pending[--count]];if(!intersects(origin,delta,node.minimum,node.maximum))continue;
  if(node.left>=0){pending[count++]=node.left;pending[count++]=node.right;continue;}
  for(int i=node.begin;i<node.end;++i){const auto&s=m_lightSolids[i];auto p=origin-s.center;Point3 local{p.x*s.cosine-p.y*s.sine,p.x*s.sine+p.y*s.cosine,p.z},direction{delta.x*s.cosine-delta.y*s.sine,delta.x*s.sine+delta.y*s.cosine,delta.z};
   if(intersects(local,direction,s.half*-1.f,s.half))return false;
  }
 }return true;
}
bool World::rayClear(Vec2 a,float az,Vec2 b,float bz,bool doors,bool dynamic,bool shelfCavities)const{
 auto delta=b-a;float dz=bz-az;int steps=std::max(1,int(std::ceil(std::sqrt(lengthSq(delta)+dz*dz)/.12f)));
 perf::count(perf::RayClear);perf::count(perf::RayClearSteps,uint64_t(steps-1));
 for(int i=1;i<steps;++i){float t=float(i)/steps;auto p=a+delta*t;float z=az+(bz-az)*t;
  if(!fits(p.x,p.y,z,.015f,dynamic,shelfCavities)|| (doors&&doorBlocks(p.x,p.y,z,.015f)))return false;
 }return true;
}
bool World::navigable(int x,int y,int nx,int ny,float height)const{
 perf::count(perf::Navigable);
 if(solid(nx+.5f,ny+.5f))return false;
 float floor=floorHeight(nx+.5f,ny+.5f);
 return std::fabs(floor-floorHeight(x+.5f,y+.5f))<=.45f&&clearanceHeight(nx+.5f,ny+.5f)-floor>=height&&!doorBlocks(nx+.5f,ny+.5f,floor,height);
}
void World::updateDoors(float dt,bool includeTransfers){for(auto&door:m_doors)if(includeTransfers||(!door.entry&&!door.transfer))door.open=std::clamp(door.open+(door.opening?1.f:-1.f)*dt*.85f,0.f,door.maxOpen);}
bool World::openDoor(int index){if(index<0||size_t(index)>=m_doors.size()||m_doors[index].opening)return false;m_doors[index].opening=true;return true;}
bool World::toggleDoor(int index){if(index<0||size_t(index)>=m_doors.size())return false;m_doors[index].opening=!m_doors[index].opening;return true;}
int World::nearbyDoor(Vec2 position,Vec2 forward,float feet)const{
 int best=-1;float distance=2.f;
 for(size_t i=0;i<m_doors.size();++i){auto&d=m_doors[i];Vec2 closest{std::clamp(position.x,d.left+.2f,d.right-.2f),d.y};auto delta=closest-position;float lengthTo=length(delta);
  if(std::fabs(feet-floorHeight((d.left+d.right)*.5f,d.y)-d.z)>1.25f)continue;
  if(lengthTo<distance&&dot(delta,forward)>-.15f){best=int(i);distance=lengthTo;}
 }return best;
}

char World::tile(int x, int y) const {
    if (x < 0 || y < 0 || x >= Width || y >= Height || m_layers.empty() || size_t(x)>=m_layers.front().rows[y].size()) return '#';
    for(auto p:m_destroyedTiles)if(p.first==x&&p.second==y)return '.';
    return m_layers.front().rows[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)];
}

bool World::solid(float x, float y) const {
 auto result=solidIndexed(x,y);if(collisionVerify())verifyCollision(0,result==solidReference(x,y));return result;
}
bool World::solidIndexed(float x, float y) const {
    perf::count(perf::Solid);
    for(auto index:collisionCandidates(0,x,y)){const auto&fixture=m_fixtures[index];if(fixture.solid&&insideFixtureCandidate(index,x,y,.2f))return true;}
    for(auto index:collisionCandidates(1,x,y)){const auto&p=m_props[index];if(std::fabs(x-p.position.x)<p.halfSize.x+.2f&&std::fabs(y-p.position.y)<p.halfSize.y+.2f)return true;}
    for(auto index:collisionCandidates(2,x,y)){const auto&terminal=m_terminals[index];if(terminal.z==0&&int(x)==int(terminal.position.x)&&int(y)==int(terminal.position.y))return true;}
    const char t = tile(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
    return t=='#'||t=='C'||t=='B'||t=='T';
}
bool World::wallSpaceFree(Vec2 center,Vec2 along,float width,float bottom,float top)const{
 auto result=wallSpaceFreeIndexed(center,along,width,bottom,top);if(collisionVerify())verifyCollision(1,result==wallSpaceFreeReference(center,along,width,bottom,top));return result;
}
bool World::wallSpaceFreeIndexed(Vec2 center,Vec2 along,float width,float bottom,float top)const{
 Vec2 normal{-along.y,along.x};
 thread_local std::vector<size_t> scratch;
 for(auto index:wallCandidates(center,along,width,scratch)){const auto&f=m_fixtures[index];float base=floorHeight(f.position.x,f.position.y)+f.base;if(base>=top||base+f.height<=bottom)continue;
  Vec2 a{m_fixtureRotation[index].x,-m_fixtureRotation[index].y},b{-a.y,a.x},delta=f.position-center;
  float tangentExtent=std::fabs(dot(along,a))*f.width*.5f+std::fabs(dot(along,b))*f.depth*.5f;
  float normalExtent=std::fabs(dot(normal,a))*f.width*.5f+std::fabs(dot(normal,b))*f.depth*.5f;
  if(std::fabs(dot(delta,along))<tangentExtent+width*.5f+.02f&&std::fabs(dot(delta,normal))<normalExtent+.06f)return false;
 }return true;
}
std::vector<Vec2> World::machines()const{
 std::vector<Vec2> result;for(auto&fixture:m_fixtures)if(fixture.model==6)result.push_back(fixture.position);for(auto&p:m_props)if(p.kind<2)result.push_back(p.position);return result;
}

bool World::isExit(float x, float y) const {
    return tile(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y))) == 'X';
}

void World::buildLayers(std::span<const Staircase> stairs){
 m_lightSolids.clear();m_lightNodes.clear();
 for(const auto& layer:m_layers){
  for(auto row:layer.rows)if(row.size()!=Width)throw std::runtime_error("Invalid map layer row width");
  if(layer.thickness<=0)continue;
  const float z=layer.elevation, underside=z-layer.thickness;
  const bool collarWall=hasLift()&&z==0;
  const float railHeight=collarWall?2.7f:.55f;
  // Wall backing above a deck edge takes the chunk's wall finish, not the grating's.
  const int wallBacker=campaign()&&authored()?3:0;
  auto deck=[&](int x,int y){return x>=0&&y>=0&&x<Width&&y<Height&&layer.rows[y][x]=='=';};
  auto stairConnection=[&](float x,float y){
   if(hasLift()&&z==0&&x>=11&&x<=13&&y>=7.9f&&y<=10)return true;
   for(const auto& stair:stairs)if(x>=stair.x1&&x<stair.x2&&y>=stair.y1&&y<stair.y2){
    float t=stair.alongY?(y-stair.y1)/(stair.y2-stair.y1):(x-stair.x1)/(stair.x2-stair.x1);
    if(!stair.ascending)t=1-t;
    float top=stair.bottom+(stair.top-stair.bottom)*(std::min(stair.steps-1,int(t*stair.steps))+1)/stair.steps;
    // The first/last tread can differ from the landing by one legal step.
    // A guardrail across that connection seals otherwise walkable stairs.
    if(std::fabs(top-z)<.215f)return true;
   }
   return false;
  };
  for(int y=0;y<Height;++y)for(int x=0;x<Width;){
   if(!deck(x,y)){++x;continue;}int start=x;while(x<Width&&deck(x,y))++x;
   m_structures.push_back({float(start),float(y),float(x),float(y+1),underside,z});
  }
  for(int y=1;y<Height-1;++y)for(int x=1;x<Width-1;++x)if(deck(x,y)){
   // Legacy decks got thin posts; authored maps carry their own columns.
   if(!hasLift()&&!custom()&&!m_mapData&&m_level<6&&(x+y)%5==0&&tile(x,y)!='#')
    m_structures.push_back({x+.06f,y+.06f,x+.14f,y+.14f,floorHeight(x+.1f,y+.1f),underside});
   if(!deck(x-1,y)&&!(hasLift()&&tile(x-1,y)=='#')&&!stairConnection(x-.001f,y+.5f)){
    m_structures.push_back({float(x),float(y),x+.055f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x-1,y)=='#')m_structures.push_back({float(x),float(y),x+.055f,y+1.f,z,hasLift()?z+2.7f:ceilingHeight(x+.5f,y+.5f),false,wallBacker});
   }
   if(!deck(x+1,y)&&!(hasLift()&&tile(x+1,y)=='#')&&!stairConnection(x+1.001f,y+.5f)){
    m_structures.push_back({x+.945f,float(y),x+1.f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x+1,y)=='#')m_structures.push_back({x+.945f,float(y),x+1.f,y+1.f,z,hasLift()?z+2.7f:ceilingHeight(x+.5f,y+.5f),false,wallBacker});
   }
   if(!deck(x,y-1)&&!(hasLift()&&tile(x,y-1)=='#')&&!stairConnection(x+.5f,y-.001f)){
    m_structures.push_back({float(x),float(y),x+1.f,y+.055f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x,y-1)=='#')m_structures.push_back({float(x),float(y),x+1.f,y+.055f,z,hasLift()?z+2.7f:ceilingHeight(x+.5f,y+.5f),false,wallBacker});
   }
   if(!deck(x,y+1)&&!(hasLift()&&tile(x,y+1)=='#')&&!stairConnection(x+.5f,y+1.001f)){
    m_structures.push_back({float(x),y+.945f,x+1.f,y+1.f,z,z+railHeight,!collarWall,collarWall?3:0});
    if(tile(x,y+1)=='#')m_structures.push_back({float(x),y+.945f,x+1.f,y+1.f,z,hasLift()?z+2.7f:ceilingHeight(x+.5f,y+.5f),false,wallBacker});
   }
  }
 }
 for(const auto& stair:stairs)for(int step=0;step<stair.steps;++step){
  float lo=float(step)/stair.steps,hi=float(step+1)/stair.steps;
  float top=stair.bottom+(stair.top-stair.bottom)*(stair.ascending?hi:1-lo);
  m_structures.push_back({
   stair.alongY?stair.x1:stair.x1+(stair.x2-stair.x1)*lo,
   stair.alongY?stair.y1+(stair.y2-stair.y1)*lo:stair.y1,
   stair.alongY?stair.x2:stair.x1+(stair.x2-stair.x1)*hi,
   stair.alongY?stair.y1+(stair.y2-stair.y1)*hi:stair.y2,stair.openUnderside?top-stair.treadThickness:stair.bottom,top,false,stair.openUnderside?2:hasLift()?4:0});
  if(stair.sideRails){
   // Side guards have the same tread-local elevation in rendering, optical
   // occlusion and hull collision. They leave the under-stair space open.
   float x1=stair.alongY?stair.x1:stair.x1+(stair.x2-stair.x1)*lo;
   float x2=stair.alongY?stair.x2:stair.x1+(stair.x2-stair.x1)*hi;
   float y1=stair.alongY?stair.y1+(stair.y2-stair.y1)*lo:stair.y1;
   float y2=stair.alongY?stair.y1+(stair.y2-stair.y1)*hi:stair.y2;
   if(stair.alongY){m_structures.push_back({x1,y1,x1+.07f,y2,top,top+1.05f,true,2});m_structures.push_back({x2-.07f,y1,x2,y2,top,top+1.05f,true,2});}
   else{m_structures.push_back({x1,y1,x2,y1+.07f,top,top+1.05f,true,2});m_structures.push_back({x1,y2-.07f,x2,y2,top,top+1.05f,true,2});}
  }
 }
 m_structureCells.resize(Width*Height);
 if(m_structures.size()>65535)throw std::runtime_error("Map exceeds the collision structure capacity");
 for(size_t index=0;index<m_structures.size();++index){
  const auto&s=m_structures[index];
  for(int y=int(std::floor(s.y1-.001f));y<=int(std::floor(s.y2+.001f));++y)for(int x=int(std::floor(s.x1-.001f));x<=int(std::floor(s.x2+.001f));++x)
   if(x>=0&&x<Width&&y>=0&&y<Height)m_structureCells[y*Width+x].push_back(static_cast<uint16_t>(index));
 }
}
float World::wallHeight(int x,int y)const{return outdoors()?floorHeight(x+.5f,y+.5f)+(m_internalWallHeight>0?m_internalWallHeight:definition().ceiling):m_internalWallHeight>0&&x>0&&x<Width-1&&y>0&&y<Height-1?m_internalWallHeight:ceilingHeight(x+.5f,y+.5f);}
float World::supportBelow(float x,float y,float feet)const{
 perf::LeafScope queryProfile(perf::Support);perf::count(perf::SupportCalls);
 auto*batch=CollisionQueryBatch::active(this);float value;
 if(batch&&batch->find(CollisionQueryBatch::SupportBelow,x,y,feet,0,0,value))return value;
 value=supportBelowUncached(x,y,feet);
 if(batch)batch->store(CollisionQueryBatch::SupportBelow,x,y,feet,0,0,value);
 return value;
}
float World::supportBelowUncached(float x,float y,float feet)const{
 float fixtureTop=-100;
 for(auto index:collisionCandidates(0,x,y)){const auto&f=m_fixtures[index];if(f.solid&&insideFixtureCandidate(index,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7)for(float tier:ShelfTiers){float top=base+f.height*tier;if(top<=feet+.025f)fixtureTop=std::max(fixtureTop,top);}
  else if(base+f.height<=feet+.025f)fixtureTop=std::max(fixtureTop,base+f.height);
 }}
 for(auto index:collisionCandidates(1,x,y)){const auto&p=m_props[index];if(p.base<.025f&&std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float top=floorHeight(p.position.x,p.position.y)+p.base+p.height;if(top<=feet+.025f)fixtureTop=std::max(fixtureTop,top);}}
 float result=hasTerrain()?terrainSurfaceBelow(x,y,feet+.03f):floorHeight(x,y),base=supportHeight(x,y);if(base<=feet+.025f)result=base;
 result=std::max(result,fixtureTop);
 for(auto index:collisionCandidates(2,x,y)){const auto&terminal=m_terminals[index];if(insideTerminal(*this,terminal,x,y)){
  float top=floorHeight(terminal.position.x,terminal.position.y)+terminal.z+terminalHeight(*this,terminal,x,y);
  if(top<=feet+.025f)result=std::max(result,top);
 }}
 if(insideLift(x,y)&&m_liftHeight<=feet+.025f)result=std::max(result,m_liftHeight);
 if(insideCargoLift(x,y)&&m_cargoLiftHeight<=feet+.025f)result=std::max(result,m_cargoLiftHeight);
 for(const auto& c:m_pushCrates)if(std::fabs(x-c.position.x)<c.size*.5f&&std::fabs(y-c.position.y)<c.size*.5f&&c.z+c.size<=feet+.025f)result=std::max(result,c.z+c.size);
 for(size_t i=0;i<actorTracks().size();++i){const auto& t=actorTracks()[i];const auto& p=m_actorPoses[i];if(t.platform&&std::fabs(x-p.position.x)<=t.footprint.x*.5f&&std::fabs(y-p.position.y)<=t.footprint.y*.5f&&p.z<=feet+.025f)result=std::max(result,p.z);}
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.top<=feet+.025f)result=std::max(result,s.top);}
 return result;
}
float World::clearanceAbove(float x,float y,float feet)const{
 auto result=clearanceAboveIndexed(x,y,feet);if(collisionVerify())verifyCollision(2,sameFloat(result,clearanceAboveReference(x,y,feet)));return result;
}
float World::clearanceAboveIndexed(float x,float y,float feet)const{
 float ceiling=hasTerrain()?std::min(clearanceHeight(x,y,feet),terrainSurfaceAbove(x,y,feet)):clearanceHeight(x,y,feet);
 for(auto index:collisionCandidates(0,x,y)){const auto&f=m_fixtures[index];if(f.solid&&insideFixtureCandidate(index,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7)for(float tier:ShelfTiers){float underside=base+f.height*tier-.045f;if(underside>feet+.025f)ceiling=std::min(ceiling,underside);}
  else if(base>feet+.025f)ceiling=std::min(ceiling,base);
 }}
 for(auto index:collisionCandidates(1,x,y)){const auto&p=m_props[index];if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float base=floorHeight(p.position.x,p.position.y)+p.base;if(base>feet+.025f)ceiling=std::min(ceiling,base);}}
 if(insideLift(x,y)&&feet<m_liftHeight+2.6f)ceiling=std::min(ceiling,m_liftHeight+2.6f);
 if(insideCargoLift(x,y)&&feet<m_cargoLiftHeight+2.4f)ceiling=std::min(ceiling,m_cargoLiftHeight+2.4f);
 for(const auto& c:m_pushCrates)if(std::fabs(x-c.position.x)<c.size*.5f&&std::fabs(y-c.position.y)<c.size*.5f&&c.z>=feet+.025f)ceiling=std::min(ceiling,c.z);
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.bottom>=feet+.025f)ceiling=std::min(ceiling,s.bottom);}
 for(auto index:collisionCandidates(2,x,y)){const auto&t=m_terminals[index];float base=floorHeight(t.position.x,t.position.y)+t.z;if(base>feet+.025f&&insideTerminal(*this,t,x,y))ceiling=std::min(ceiling,base);}
 return ceiling;
}
bool World::fits(float x,float y,float feet,float height,bool dynamic,bool shelfCavities)const{
 perf::LeafScope queryProfile(perf::Fits);perf::count(perf::FitsCalls);
 auto*batch=CollisionQueryBatch::active(this);float value;uint32_t flags=uint32_t(dynamic)|(uint32_t(shelfCavities)<<1);
 if(batch&&batch->find(CollisionQueryBatch::Fits,x,y,feet,height,flags,value))return value!=0;
 bool result=fitsUncached(x,y,feet,height,dynamic,shelfCavities);
 if(batch)batch->store(CollisionQueryBatch::Fits,x,y,feet,height,flags,result?1.f:0.f);
 return result;
}
bool World::crateBlocks(float x,float y,float feet,float height,int except)const{
 for(size_t i=0;i<m_pushCrates.size();++i){if(int(i)==except)continue;const auto& c=m_pushCrates[i];const float half=c.size*.5f;
  if(std::fabs(x-c.position.x)<half&&std::fabs(y-c.position.y)<half&&feet<c.z+c.size-.025f&&feet+height>c.z+.025f)return true;}
 return false;
}
bool World::fitsUncached(float x,float y,float feet,float height,bool dynamic,bool shelfCavities)const{
 perf::count(perf::FitsComputed);
 if(dynamic&&crateBlocks(x,y,feet,height))return false;
 if(dynamic)for(size_t i=0;i<actorTracks().size();++i){const auto& t=actorTracks()[i];const auto& p=m_actorPoses[i];if(t.platform&&std::fabs(x-p.position.x)<t.footprint.x*.5f&&std::fabs(y-p.position.y)<t.footprint.y*.5f&&feet<p.z-.025f&&feet+height>p.z-t.thickness)return false;}
 if(dynamic&&insideCargoLift(x,y)&&feet<m_cargoLiftHeight-.025f&&feet+height>m_cargoLiftHeight-.18f)return false;
 if(dynamic&&insideCargoLift(x,y)&&feet<m_cargoLiftHeight+2.52f&&feet+height>m_cargoLiftHeight+2.4f)return false;
 if(dynamic&&insideLift(x,y)&&feet<m_liftHeight+2.8f&&feet+height>m_liftHeight-.25f){
  if(feet<m_liftHeight-.025f||feet+height>m_liftHeight+2.605f)return false;
  if(x<10.12f||x>13.88f)return false;
  bool northOpen=m_liftPhase==LiftPhase::Ready&&x>11&&x<13;
  bool southOpen=m_liftPhase==LiftPhase::Crashed&&m_liftTimer>=3.65f&&x>11&&x<13;
  if((y<10.12f&&!northOpen)||(y>13.88f&&!southOpen))return false;
 }
 if(hasTerrain()){
  TerrainColumn density(m_terrainDensity,m_terrainTops,x,y);
  if(feet-.125f<density.airTop()||feet+height>128.f+.005f){
   float support=terrainFloor(feet+.03f,density),ceiling=terrainCeiling(feet,feet+height,density);
   if(feet<support-.025f||feet+height>ceiling+.005f)return false;
   for(float z=feet+.04f;z<feet+height-.02f;z+=.16f)if(density(z)>0)return false;
  }
  char terrainTile=tile(int(std::floor(x)),int(std::floor(y)));
  if(terrainTile=='#'||terrainTile=='C'||terrainTile=='B'||terrainTile=='T'){
   float base=floorHeight(x,y),top=terrainTile=='#'?wallHeight(int(std::floor(x)),int(std::floor(y))):base+(terrainTile=='C'?.60f:terrainTile=='B'?1.1f:2.62f);
   if(feet<top-.025f&&feet+height>base+.005f)return false;
  }
 }else if(feet<supportHeight(x,y,dynamic,shelfCavities)-.025f||feet+height>clearanceHeight(x,y,feet)+.005f)return false;
 for(auto index:collisionCandidates(0,x,y)){const auto&f=m_fixtures[index];if(f.solid&&insideFixtureCandidate(index,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(shelfCavities&&f.model==7){
   for(float tier:ShelfTiers){float top=base+f.height*tier;if(feet<top-.005f&&feet+height>top-.045f)return false;}
   float dx=x-f.position.x,dy=y-f.position.y,c=std::cos(f.yaw),s=std::sin(f.yaw);
   float localX=std::fabs(dx*c-dy*s),localY=std::fabs(dx*s+dy*c);
   if(localX>f.width*.5f-.04f&&localY>f.depth*.5f-.04f&&feet<base+f.height&&feet+height>base)return false;
  }else if(feet<base+f.height-.025f&&feet+height>base+.005f)return false;
 }}
 for(auto index:collisionCandidates(1,x,y)){const auto&p=m_props[index];if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float base=floorHeight(p.position.x,p.position.y)+p.base;if(feet<base+p.height-.025f&&feet+height>base+.005f)return false;}}
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&feet<s.top-.025f&&feet+height>s.bottom+.005f)return false;}
 for(auto index:collisionCandidates(2,x,y)){const auto&t=m_terminals[index];if(!dynamic&&hasLift()&&t.control)continue;float base=floorHeight(t.position.x,t.position.y)+t.z;if(insideTerminal(*this,t,x,y)&&feet<base+terminalHeight(*this,t,x,y)-.025f&&feet+height>base)return false;}
 return true;
}
bool World::structureSegmentClear(Vec2 start,float feet,Vec2 goal,float height,bool indexed)const{
 auto clear=[&](const Structure&s){
  if(s.x2<=s.x1||s.y2<=s.y1||s.top<=s.bottom)return true;
  if(s.top<=feet+.265f||s.bottom>=feet+height-.005f)return true;
  return !(rayBoxFraction({start.x,start.y,feet},{goal.x-start.x,goal.y-start.y,0},
   {s.x1-.1999f,s.y1-.1999f,s.bottom-height+.005f},{s.x2+.1999f,s.y2+.1999f,s.top-.025f})<1);
 };
 float lowX=std::min(start.x,goal.x)-.2001f,lowY=std::min(start.y,goal.y)-.2001f;
 float highX=std::max(start.x,goal.x)+.2001f,highY=std::max(start.y,goal.y)+.2001f;
 auto all=[&]{for(const auto&s:m_structures)if(!clear(s))return false;return true;};
 // Sweeps outside the indexed map and long diagonals use the full reference.
 if(!indexed||!m_collisionCandidatesEnabled||m_structureCells.size()!=Width*Height||!(lowX>=0&&lowY>=0&&highX<Width&&highY<Height))return all();
 int x0=int(std::floor(lowX)),y0=int(std::floor(lowY)),x1=int(std::floor(highX)),y1=int(std::floor(highY));
 if((x1-x0+1)*(y1-y0+1)>64)return all();
 for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x)for(auto index:m_structureCells[y*Width+x])if(!clear(m_structures[index]))return false;
 return true;
}
bool World::railBlocksHull(float x,float y,float radius,float feet,float height)const{
 if(m_structureCells.empty())return false;
 int x0=std::max(0,int(std::floor(x-radius))),x1=std::min(Width-1,int(std::floor(x+radius)));
 int y0=std::max(0,int(std::floor(y-radius))),y1=std::min(Height-1,int(std::floor(y+radius)));
 for(int cy=y0;cy<=y1;++cy)for(int cx=x0;cx<=x1;++cx)for(auto index:m_structureCells[cy*Width+cx]){
  const auto&s=m_structures[index];if(!s.rail&&!isBreakableStructure(index))continue;
  if(x+radius<=s.x1||x-radius>=s.x2||y+radius<=s.y1||y-radius>=s.y2)continue;
  if(feet<s.top-.025f&&feet+height>s.bottom+.005f)return true;
 }
 return false;
}
std::vector<Span> World::spansAt(int x,int y)const{
 std::vector<Span> result;spansAt(x,y,result);return result;
}
void World::spansAt(int x,int y,std::vector<Span>& result)const{
 spansAtIndexed(x,y,result);if(collisionVerify())verifyCollision(3,sameSpans(result,spansAtReference(x,y)));
}
void World::spansAtIndexed(int x,int y,std::vector<Span>& result)const{
 perf::count(perf::SpansAt);
 float px=x+.5f,py=y+.5f,roof=ceilingHeight(px,py);thread_local std::vector<std::pair<float,float>> solids;solids.clear();solids.push_back({-100.f,supportHeight(px,py)});
 for(auto index:collisionCandidates(0,px,py)){const auto&f=m_fixtures[index];if(f.solid&&f.base>.025f&&insideFixtureCandidate(index,px,py)){float base=floorHeight(px,py)+f.base;solids.push_back({base,base+f.height});}}
 for(auto index:collisionCandidates(1,px,py)){const auto&p=m_props[index];if(p.base>.025f&&std::fabs(px-p.position.x)<p.halfSize.x&&std::fabs(py-p.position.y)<p.halfSize.y){float base=floorHeight(px,py)+p.base;solids.push_back({base,base+p.height});}}
 for(auto index:structureIndices(px,py)){auto&s=m_structures[index];if(px>=s.x1&&px<s.x2&&py>=s.y1&&py<s.y2)solids.push_back({s.bottom,s.top});}
 std::sort(solids.begin(),solids.end());result.clear();float bottom=-100.f;
 for(auto solid:solids){if(solid.first>bottom)result.push_back({bottom,solid.first,0});bottom=std::max(bottom,solid.second);}
 if(bottom<roof)result.push_back({bottom,roof,0});return;
}
bool World::solidReference(float x, float y) const {

    for(const auto&fixture:m_fixtures)if(fixture.solid&&insideFixture(fixture,x,y,.2f))return true;
    for(auto&p:m_props)if(std::fabs(x-p.position.x)<p.halfSize.x+.2f&&std::fabs(y-p.position.y)<p.halfSize.y+.2f)return true;
    for(auto&terminal:m_terminals)if(terminal.z==0&&int(x)==int(terminal.position.x)&&int(y)==int(terminal.position.y))return true;
    const char t = tile(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)));
    return t=='#'||t=='C'||t=='B'||t=='T';
}
bool World::wallSpaceFreeReference(Vec2 center,Vec2 along,float width,float bottom,float top)const{
 Vec2 normal{-along.y,along.x};
 for(auto&f:m_fixtures){float base=floorHeight(f.position.x,f.position.y)+f.base;if(base>=top||base+f.height<=bottom)continue;
  Vec2 a{std::cos(f.yaw),-std::sin(f.yaw)},b{-a.y,a.x},delta=f.position-center;
  float tangentExtent=std::fabs(dot(along,a))*f.width*.5f+std::fabs(dot(along,b))*f.depth*.5f;
  float normalExtent=std::fabs(dot(normal,a))*f.width*.5f+std::fabs(dot(normal,b))*f.depth*.5f;
  if(std::fabs(dot(delta,along))<tangentExtent+width*.5f+.02f&&std::fabs(dot(delta,normal))<normalExtent+.06f)return false;
 }return true;
}
float World::clearanceAboveReference(float x,float y,float feet)const{
 float ceiling=hasTerrain()?std::min(clearanceHeight(x,y,feet),terrainSurfaceAbove(x,y,feet)):clearanceHeight(x,y,feet);
 for(auto&f:m_fixtures)if(f.solid&&insideFixture(f,x,y)){
  float base=floorHeight(f.position.x,f.position.y)+f.base;
  if(f.model==7)for(float tier:ShelfTiers){float underside=base+f.height*tier-.045f;if(underside>feet+.025f)ceiling=std::min(ceiling,underside);}
  else if(base>feet+.025f)ceiling=std::min(ceiling,base);
 }
 for(auto&p:m_props)if(std::fabs(x-p.position.x)<p.halfSize.x&&std::fabs(y-p.position.y)<p.halfSize.y){float base=floorHeight(p.position.x,p.position.y)+p.base;if(base>feet+.025f)ceiling=std::min(ceiling,base);}
 if(insideLift(x,y)&&feet<m_liftHeight+2.6f)ceiling=std::min(ceiling,m_liftHeight+2.6f);
 if(insideCargoLift(x,y)&&feet<m_cargoLiftHeight+2.4f)ceiling=std::min(ceiling,m_cargoLiftHeight+2.4f);
 for(const auto& c:m_pushCrates)if(std::fabs(x-c.position.x)<c.size*.5f&&std::fabs(y-c.position.y)<c.size*.5f&&c.z>=feet+.025f)ceiling=std::min(ceiling,c.z);
 for(auto index:structureIndices(x,y)){auto&s=m_structures[index];if(x>=s.x1&&x<s.x2&&y>=s.y1&&y<s.y2&&s.bottom>=feet+.025f)ceiling=std::min(ceiling,s.bottom);}
 for(auto&t:m_terminals){float base=floorHeight(t.position.x,t.position.y)+t.z;if(base>feet+.025f&&insideTerminal(*this,t,x,y))ceiling=std::min(ceiling,base);}
 return ceiling;
}
std::vector<Span> World::spansAtReference(int x,int y)const{

 float px=x+.5f,py=y+.5f,roof=ceilingHeight(px,py);std::vector<std::pair<float,float>> solids={{-100.f,supportHeight(px,py)}};
 for(auto&f:m_fixtures)if(f.solid&&f.base>.025f&&insideFixture(f,px,py)){float base=floorHeight(px,py)+f.base;solids.push_back({base,base+f.height});}
 for(auto&p:m_props)if(p.base>.025f&&std::fabs(px-p.position.x)<p.halfSize.x&&std::fabs(py-p.position.y)<p.halfSize.y){float base=floorHeight(px,py)+p.base;solids.push_back({base,base+p.height});}
 for(auto index:structureIndices(px,py)){auto&s=m_structures[index];if(px>=s.x1&&px<s.x2&&py>=s.y1&&py<s.y2)solids.push_back({s.bottom,s.top});}
 std::sort(solids.begin(),solids.end());std::vector<Span> result;float bottom=-100.f;
 for(auto solid:solids){if(solid.first>bottom)result.push_back({bottom,solid.first,0});bottom=std::max(bottom,solid.second);}
 if(bottom<roof)result.push_back({bottom,roof,0});return result;
}
bool World::destroyTile(int x,int y){if(x<0||y<0||x>=Width||y>=Height||m_layers.empty()||tile(x,y)=='.')return false;if(tile(x,y)!='C'&&tile(x,y)!='B')return false;m_destroyedTiles.push_back({x,y});m_lightSolids.clear();m_lightNodes.clear();return true;}
}
