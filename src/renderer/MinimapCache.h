#pragma once
#include "../world/World.h"
#include <array>
#include <atomic>
#include <bit>
#include <fstream>
namespace retro {
// Exact snapshot of every World::solid input at the HUD's fixed sample grid.
// This shares no gameplay/collision state and never approximates occupancy.
class MinimapCache {
public:
 using Cells=std::array<uint8_t,World::Width*World::Height>;
 struct Statistics {uint64_t queries=0,rebuilds=0;};
 inline static std::atomic<bool> enabled{true};
 static Statistics statistics(){return s_statistics;}
 static void resetStatistics(){s_statistics={};}
 const Cells&cells(const World&world,bool useCache=true){
  ++s_statistics.queries;
  if(!useCache||!enabled.load(std::memory_order_relaxed)){sample(world,m_reference);return m_reference;}
  m_scratch.clear();auto word=[&](uint32_t value){m_scratch.push_back(value);};auto scalar=[&](float value){word(std::bit_cast<uint32_t>(value));};
  for(int y=0;y<World::Height;++y)for(int x=0;x<World::Width;x+=4){uint32_t row=0;for(int i=0;i<4;++i)row|=uint32_t(uint8_t(world.tile(x+i,y)))<<(i*8);word(row);}
  word(uint32_t(world.fixtures().size()));for(const auto&f:world.fixtures()){word(f.solid);if(f.solid)for(float value:{f.position.x,f.position.y,f.width,f.depth,f.yaw})scalar(value);}
  word(uint32_t(world.props().size()));for(const auto&p:world.props())for(float value:{p.position.x,p.position.y,p.halfSize.x,p.halfSize.y})scalar(value);
  word(uint32_t(world.terminals().size()));for(const auto&t:world.terminals())for(float value:{t.position.x,t.position.y,t.z})scalar(value);
  if(!m_ready||m_snapshot!=m_scratch){sample(world,m_cells);m_snapshot.swap(m_scratch);m_ready=true;++s_statistics.rebuilds;}
  return m_cells;
 }
 static bool test(){
  std::ofstream report("minimap-cache-test.txt");bool passed=true;int checks=0;MinimapCache cache;
  auto check=[&](bool ok,const char*name){report<<(ok?"PASS ":"FAIL ")<<name<<'\n';report.flush();passed&=ok;++checks;};
  auto compare=[&](const World&world){auto retained=cache.cells(world);return retained==cache.cells(world,false);};
  auto map=std::make_shared<AuthoredMapData>();map->definition={{0,0},{2,2},0,Environment::Interior,true,8,.3f,42};AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');layer.rows[8][8]='B';map->layers.push_back(layer);
  map->fixtures.push_back({1,{4.5f,4.5f},0,.8f,1.f,1.f,.43f,true});map->props.push_back({0,{6.5f,4.5f},1,1,0,{.6f,.4f}});
  map->terminals.push_back({{2.5f,3.5f},"A","","",0});map->terminals.push_back({{12.5f,8.5f},"Lift","","",9});World world(0,map);
  check(compare(world),"Rotated fixtures, props, terminals and tiles match live solidity");auto builds=s_statistics.rebuilds;
  check(compare(world)&&s_statistics.rebuilds==builds,"An unchanged world reuses the HUD bitmap");
  bool occupied=cache.cells(world)[8*World::Width+8]!=0;world.destroyTile(8,8);
  check(occupied&&compare(world)&&!cache.cells(world)[8*World::Width+8],"Destroyed cover changes the next bitmap immediately");
  bool elevated=!cache.cells(world)[8*World::Width+12];world.startLift();for(int i=0;i<6000;++i)world.updateLift(1.f/60);
  check(elevated&&world.terminals()[1].z==0&&compare(world)&&cache.cells(world)[8*World::Width+12],"A moving lift terminal appearing at ground level refreshes the bitmap");
  auto replacement=std::make_shared<AuthoredMapData>(*map);replacement->fixtures[0].yaw=1.2f;replacement->fixtures[0].position={9,5};replacement->props[0].position={3,12};replacement->terminals[0].position={18,18};World changed(0,replacement);
  check(compare(changed),"Same-size replacement geometry cannot reuse stale occupancy");
  for(int level:{0,1,3,5,6,14,16,20}){World campaign(level);check(compare(campaign),"Campaign occupancy matches the live reference");}
  report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
 }
private:
 static void sample(const World&world,Cells&cells){for(int y=0;y<World::Height;++y)for(int x=0;x<World::Width;++x)cells[size_t(y*World::Width+x)]=world.solid(x+.5f,y+.5f);}
 Cells m_cells{},m_reference{};std::vector<uint32_t> m_snapshot,m_scratch;bool m_ready=false;
 inline static thread_local Statistics s_statistics{};
};
}
