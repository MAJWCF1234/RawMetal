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
  if(!check(world.floorHeight(12,12)==-9&&world.ceilingHeight(12,12)>-6.2f,"Dry service bridge has headroom"))return false;
  // A quarter-metre walk grid uses the same full hull and step allowance as
  // movement. It catches sealed bays and one-way drops around water ramps.
  std::array<bool,N*N> reached{};std::queue<int> pending;
  int start=6*N+48;reached[start]=true;pending.push(start);
  auto point=[](int index){return Vec2{(index%N+.5f)*.25f,(index/N+.5f)*.25f};};
  auto floorAt=[&](Vec2 p){return game.groundHeight(p,-8.f);};
  while(!pending.empty()){
   int from=pending.front();pending.pop();auto a=point(from);float az=floorAt(a);
   for(int offset:{-N,N,-1,1}){
    int to=from+offset;if(to<0||to>=N*N||(offset==-1&&from%N==0)||(offset==1&&from%N==N-1)||reached[to])continue;
    auto b=point(to);float bz=floorAt(b);
    if(std::fabs(bz-az)>.215f||!game.hullFits(b,bz,1))continue;
    reached[to]=true;pending.push(to);
   }
  }
  for(Vec2 target:{Vec2{12,22},Vec2{3.5f,19.5f},Vec2{20.5f,19.5f}}){
   int at=int(target.y*4)*N+int(target.x*4);
   if(!check(reached[at],"Service route reaches exit and both maintenance bays"))return false;
  }
  if(level==5){
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
