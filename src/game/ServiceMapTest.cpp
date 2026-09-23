#include "Game.h"
#include <array>
#include <cmath>
#include <fstream>
#include <queue>

namespace retro {
bool Game::testServiceMaps(){
 std::ofstream out("service-map-test.txt");
 auto check=[&](bool ok,const char* what){out<<what<<": "<<(ok?"PASS":"FAIL")<<'\n';out.flush();return ok;};
 constexpr int N=96;
 for(int level:{4,5}){
  Game game;game.loadLevel(level,false);game.updateStreaming(0);
  const auto& world=game.world();
  for(const auto& fixture:world.fixtures())if(fixture.solid){
   bool clear=true;
   for(int u=0;u<=16;++u)for(int v=0;v<=8;++v){
    float x=(u/16.f-.5f)*fixture.width,y=(v/8.f-.5f)*fixture.depth;
    float px=fixture.position.x+x*std::cos(fixture.yaw)+y*std::sin(fixture.yaw);
    float py=fixture.position.y-x*std::sin(fixture.yaw)+y*std::cos(fixture.yaw);
    if(world.tile(int(std::floor(px)),int(std::floor(py)))=='#')clear=false;
    for(const auto& s:world.structures())if(px>s.x1&&px<s.x2&&py>s.y1&&py<s.y2&&s.top>-9+fixture.base+.02f&&s.bottom<-9+fixture.base+fixture.height)clear=false;
   }
   if(!check(clear,"Entire equipment footprint clears walls and structural columns"))return false;
  }
  World doorsOpen=world;
  for(size_t d=0;d<world.doors().size();++d)doorsOpen.openDoor(int(d));
  doorsOpen.updateDoors(2);
  for(const auto& door:doorsOpen.doors()){
   float x=(door.left+door.right)*.5f;
   for(float y:{door.y-.45f,door.y,door.y+.45f})
    if(!check(doorsOpen.fits(x,y,-9,1.7f)&&!doorsOpen.doorBlocks(x,y,-9,1.7f),"Opened door has standing clearance on both sides"))return false;
  }
  if(!check(world.floorHeight(12,12)==-9&&world.ceilingHeight(12,12)>-6.2f,"Dry service bridge has headroom"))return false;
  // A quarter-metre walk grid uses the same full hull and step allowance as
  // movement. It catches sealed bays and one-way drops around water ramps.
  std::array<bool,N*N> reached{};std::queue<int> pending;
  auto spawn=world.definition().playerStart;
  int start=int(spawn.y*4)*N+int(spawn.x*4);reached[start]=true;pending.push(start);
  auto point=[](int index){return Vec2{(index%N+.5f)*.25f,(index/N+.5f)*.25f};};
  auto floorAt=[&](Vec2 p){return game.groundHeight(p,-8.f);};
  while(!pending.empty()){
   int from=pending.front();pending.pop();auto a=point(from);float az=floorAt(a);
   for(int offset:{-N,N,-1,1}){
    int to=from+offset;if(to<0||to>=N*N||(offset==-1&&from%N==0)||(offset==1&&from%N==N-1)||reached[to])continue;
    auto b=point(to);float bz=floorAt(b);
    if(std::fabs(bz-az)>.215f||!game.hullFits(b,bz,1.7f))continue;
    reached[to]=true;pending.push(to);
   }
  }
  for(Vec2 target:{Vec2{12,22},Vec2{3.5f,19.5f},Vec2{20.5f,19.5f}}){
   int at=int(target.y*4)*N+int(target.x*4);
   if(!check(reached[at],"Service route reaches exit and both maintenance bays"))return false;
  }
  if(level==4){
   for(Vec2 target:{Vec2{5.5f,9.5f},Vec2{19.5f,9},Vec2{5.5f,20},Vec2{19,20}})
    if(!check(reached[int(target.y*4)*N+int(target.x*4)],"Workshop, electrical room, stores and plant bay are accessible"))return false;
  }
  for(const auto& pipe:world.pipes()){
   bool clear=true;
   for(int i=0;i<=32;++i){auto p=pipe.start+(pipe.end-pipe.start)*(i/32.f);
    clear&=pipe.z-pipe.radius>=-9+2.2f&&pipe.z+pipe.radius<world.ceilingHeight(p.x,p.y);
   }
   if(!check(clear,"Authored headers fit below ceiling and above standing clearance"))return false;
  }
  if(level==5){
   if(!check(world.particleEmitters().empty(),"Settling pools do not emit fountain jets"))return false;
   if(!check(world.waterVolumes().size()==4,"Four authored coolant basins"))return false;
   for(const auto& basin:world.waterVolumes()){
    float x=(basin.x1+basin.x2)*.5f,y=(basin.y1+basin.y2)*.5f;
    if(!check(world.waterSurface(x,y)==basin.surface&&std::fabs(world.floorHeight(x,y)-basin.bed)<.001f,"Render, bed and buoyancy share basin definition"))return false;
   }
   if(!check(world.waterSurface(12,9)<-100&&world.floorHeight(12,9)==-9,"Central walkway remains dry"))return false;
   if(!check(world.doors().size()==1&&world.doors()[0].swinging&&world.doorBlocks(19.7f,20.76f,-9,1.7f),"Rear store has a closed swinging door"))return false;
   World opened=world;opened.openDoor(0);opened.updateDoors(2);
   if(!check(!opened.doorBlocks(19.7f,20.76f,-9,1.7f)&&opened.fits(19.7f,21.3f,-9,1.7f),"Swinging leaf clears a walkable store entrance"))return false;
  }
 }
 Game player;player.loadLevel(5,false);player.m_enemies.clear();player.m_player.pos={9.f,11.4f};player.m_player.z=-9;player.m_player.angle=-kPi*.5f;player.m_player.grounded=true;
 World channelGeometry(5);
 for(const auto& basin:channelGeometry.waterVolumes())for(float direction:{-1.f,1.f}){
  Game escape;escape.loadLevel(5,false);escape.m_enemies.clear();
  escape.m_player.pos={(basin.x1+basin.x2)*.5f,(basin.y1+basin.y2)*.5f};
  escape.m_player.z=basin.bed;escape.m_player.grounded=true;escape.m_player.angle=direction*kPi*.5f;
  InputState forward{};forward.forward=true;
  float end=direction<0?basin.y1-.45f:basin.y2+.45f;
  for(int frame=0;frame<600&&direction*(escape.player().pos.y-end)<0;++frame)escape.update(forward,1.f/120);
  if(!check(direction*(escape.player().pos.y-end)>=0&&escape.player().z>-9.08f,"Each channel has walkable ramps at both ends"))return false;
 }
 InputState walk{};walk.forward=true;float lowest=0;
 for(int i=0;i<110;++i){player.update(walk,1.f/120);lowest=std::min(lowest,player.player().z);}
 if(!check(lowest<-9.12f,"Player enters sloped flooded trench"))return false;
 player.m_player.angle=kPi*.5f;player.m_velocity={};
 for(int i=0;i<125;++i)player.update(walk,1.f/120);
 if(!check(player.player().pos.y>10.9f&&player.player().z>-9.08f,"Player walks back out without one-way drop"))return false;
 Game debris;debris.loadLevel(5,false);debris.m_enemies.clear();debris.m_clutter.clear();
 Clutter bottle;bottle.kind=2;bottle.pos={9,9};bottle.z=-9.10f;debris.m_clutter.push_back(bottle);
 Clutter metal;metal.kind=3;metal.pos={9.5f,9};metal.z=-9.10f;debris.m_clutter.push_back(metal);
 for(int i=0;i<360;++i)debris.updateClutter({},1.f/120);
 float bed=debris.world().floorHeight(9,9);
 if(!check(debris.m_clutter[0].z>bed+.1f&&!debris.m_clutter[0].sleeping,"Buoyant debris stays at the water surface"))return false;
 auto& heavy=debris.m_clutter[1];auto extent=heavy.extent();float contact=-100;
 for(float dx:{-extent[0],0.f,extent[0]})for(float dy:{-extent[1],0.f,extent[1]})
  contact=std::max(contact,debris.world().floorHeight(heavy.pos.x+dx,heavy.pos.y+dy));
 if(!check(std::fabs(heavy.z-contact)<.035f,"Heavy debris settles against the sloped basin floor"))return false;
 Game restored;
 if(!check(restored.decodeSave(debris.encodeSave())&&restored.level()==5&&restored.world().waterVolumes().size()==4&&restored.clutter().size()==2,
           "Saving and restoring retains basin geometry and debris state"))return false;
 return true;
}
}
