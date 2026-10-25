#include "Game.h"
#include "../world/CustomCampaign.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <set>
#include <bit>
#include <algorithm>

namespace retro {
namespace {
auto exclusionCampaign(){auto path=std::filesystem::path("custom maps")/"RawMetal-Ashfall-Exclusion-Zone.txt";return std::filesystem::exists(path)?loadCustomCampaignFile(path):loadEmbeddedExclusionZoneCampaign();}
}
Game Game::zoneInspection(int level,Vec2 position,float yawDegrees,float pitch,float lift){
 Game g(exclusionCampaign());
 if(g.level()!=level){g.storeChunk();g.loadLevel(level,true);g.updateStreaming(0);}
 g.m_player.pos=position;g.m_player.z=g.groundHeight(position,float(World::TerrainMaxZ+1));
 g.m_player.angle=yawDegrees*kPi/180.f;g.m_player.pitch=pitch;g.equipWeapon(WeaponId::Fists);
 for(int i=0;i<12;++i)g.update({},1.f/60);
 g.m_player.angle=yawDegrees*kPi/180.f;g.m_player.pitch=pitch;g.stowWeapon();g.m_player.z+=lift;
 return g;
}
namespace {
// The open zone is 5 x 4 maps of 24 m; views are given in zone metres.
int zoneLevel(Vec2 world){return std::clamp(int(world.y/24),0,3)*5+std::clamp(int(world.x/24),0,4);}
Game zoneView(Vec2 world,Vec2 target,float pitch=-6,float lift=0){
 int level=zoneLevel(world);Vec2 origin{float(level%5)*24,float(level/5)*24};
 return Game::zoneInspection(level,world-origin,std::atan2(target.y-world.y,target.x-world.x)*180.f/kPi,pitch,lift);
}
}
Game Game::exclusionInspection(int stage){
 const bool endurance=stage==12;if(endurance)stage=2;
 if(stage<0||stage>16||stage==12)throw std::runtime_error("Unknown Exclusion encounter view");
 if(stage==8||stage==10||stage==11){
  // Main-campaign Receiving views kept for the renderer comparisons.
  if(stage==8){Game g;g.loadLevel(14,false);g.updateStreaming(0);g.m_player.pos={10.8f,21.8f};g.m_player.z=-25;g.m_player.angle=std::atan2(16.f-21.8f,13.f-10.8f);g.m_player.pitch=-6;g.equipWeapon(WeaponId::Fists);return g;}
  Game g;g.loadLevel(14,false);g.updateStreaming(0);g.m_player.pos={1.4f,20};g.m_player.z=-25;
  std::vector<size_t> detail;for(size_t i=0;i<g.npcActors().size();++i)if(g.npcActors()[i].homeLevel==14&&g.npcActors()[i].track().ai.squad==stateId("receiving_cleanup"))detail.push_back(i);
  if(detail.size()!=6)throw std::runtime_error("Receiving breach preview requires the authored detail");
  // Stage the two opening player kills through the real damage path; the
  // remaining deaths and retreat are produced by ordinary combat/navigation.
  for(auto index:{detail[0],detail[2]})g.damageFriendly(index,1000,{7,22},{ShotKind::Player,14,0});
  g.updateScripts(0);for(int frame=0;frame<3600;++frame)g.update({},1.f/60);
  if(!g.state("receiving_retreat_sealed"))throw std::runtime_error("Receiving retreat did not complete");
  if(stage==10){g.m_player.pos={10.8f,21.8f};g.m_player.angle=std::atan2(16.f-21.8f,13.f-10.8f);g.m_player.pitch=-12;}
  else{g.loadLevel(15,false);g.m_player.pos={12,14};g.m_player.angle=kPi*.5f;g.m_player.pitch=-6;}
  g.m_player.z=-25;g.m_logTime=0;g.m_activeLog=-1;g.equipWeapon(WeaponId::Fists);return g;
 }
 // Zone places, each from a road or clearing: 0 camp, 1 fuel station,
 // 2 checkpoint (the guarded encounter), 3 scrapyard, 4 radio tower, 5 forest,
 // 6 swamp, 7 anomalies, 9 industrial, 13 mine, 14 village, 15 hunter's camp,
 // 16 hilltop overview.
 struct View {Vec2 camera,target;float pitch=-6,lift=0;};
 const View views[17]={{{10,81.5f},{14.5f,85}},{{30,81.5f},{26,66}},{{44,52.5f},{60,52}},{{74,54},{88,62}},{{101,42},{109,40}},
  {{45,14.5f},{20,20}},{{42.5f,51},{17,46}},{{73.5f,30},{58,30}},{},{{73.5f,20},{98,17}},{},{},{},
  {{60,14.5f},{56,7}},{{82,80},{98,85}},{{36,18},{33.5f,15.5f}},{{4,4},{40,40},-6,5}};
 const auto& view=views[stage];
 // Nudge onto clear ground: authored clutter may stand on the nominal spot.
 Vec2 camera=view.camera;bool found=false;
 for(int ring=0;ring<10&&!found;++ring)for(int k=0;k<8&&!found;++k){float a=k*kPi/4;Vec2 probe=view.camera+Vec2{std::cos(a),std::sin(a)}*(ring*.35f);
  auto g=zoneView(probe,view.target);if(g.hullFits(g.player().pos,g.player().z,Player::StandingHeight)){camera=probe;found=true;}}
 if(!found)throw std::runtime_error("Zone view "+std::to_string(stage)+" has no clear standing spot");
 auto g=zoneView(camera,view.target,view.pitch,view.lift);
 // Benchmark only: keep the target alive through a sustained squad encounter.
 if(endurance)g.m_player.health=100000;
 return g;
}
bool Game::testExclusion(){
 std::ofstream report("exclusion-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const std::string& text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 Game g(exclusionCampaign());
 // Layout, sky and terrain.
 check(g.chunkCount()==20,"The zone spans twenty 24 m maps");
 bool grid=true,sky=true,terrain=true;
 for(int level=0;level<g.chunkCount();++level){auto o=g.chunkOffset(level);grid&=o.x==float(level%5)*24&&o.y==float(level/5)*24;
  sky&=g.chunkWorld(level).zoneSky();terrain&=g.chunkWorld(level).hasTerrain();}
 check(grid,"Maps form a 5 x 4 grid in zone metres");
 check(sky&&terrain,"Every map uses the open-zone sky over authored terrain");
 check(g.world().sun().intensity>0&&g.world().sun().direction.z>.2f&&g.world().sun().direction.z<.6f,"A low warm sun lights the zone");
 // Seams: neighbouring maps agree on the ground along their shared edge.
 float worstSeam=0;
 for(int level=0;level<g.chunkCount();++level){const auto& a=g.chunkWorld(level);
  if(level%5<4){const auto& b=g.chunkWorld(level+1);for(float y=.5f;y<24;y+=1)worstSeam=std::max(worstSeam,std::fabs(a.floorHeight(23.9f,y)-b.floorHeight(.1f,y)));}
  if(level/5<3){const auto& b=g.chunkWorld(level+5);for(float x=.5f;x<24;x+=1)worstSeam=std::max(worstSeam,std::fabs(a.floorHeight(x,23.9f)-b.floorHeight(x,.1f)));}}
 report<<"worst seam step "<<worstSeam<<" m\n";
 check(worstSeam<=1.01f,"Terrain is continuous across every map seam (no step above one voxel)");
 {Game again(exclusionCampaign());bool same=true;
  for(int level=0;level<g.chunkCount();++level)for(float y=.5f;y<24;y+=3)for(float x=.5f;x<24;x+=3)same&=g.chunkWorld(level).floorHeight(x,y)==again.chunkWorld(level).floorHeight(x,y);
  check(same,"The seeded terrain generator is deterministic");}
 // Ground cover follows the authored paint.
 check(g.chunkWorld(15).groundKind(20,81.5f)==GroundDry,"The camp road is painted as a dirt track");
 {unsigned kinds=0;for(float y=2;y<96;y+=7)for(float x=2;x<120;x+=7){int kind=g.chunkWorld(zoneLevel({x,y})).groundKind(x,y);if(kind>=0&&kind<GroundKinds)kinds|=1u<<kind;}
  check(std::popcount(kinds)>=6,"The zone shows at least six kinds of ground cover");}
 // Spawns, NPCs and pickups stand on clear ground.
 check(g.hullFits(g.player().pos,g.player().z,Player::StandingHeight),"The rookie-camp spawn is clear");
 int guards=0;std::set<StateId> squads;
 for(size_t actor=0;actor<g.npcActors().size();++actor){const auto&npc=g.npcActors()[actor];const auto&track=npc.track();
  check(g.friendlyBodyFits(npc.level,npc.pose.position,npc.pose.z,track.scale),"NPC spawn has a clear standing hull (map "+std::to_string(npc.level)+" at "+std::to_string(npc.pose.position.x)+","+std::to_string(npc.pose.position.y)+")");
  for(const auto&point:track.ai.patrol)check(g.friendlyBodyFits(npc.level,point.position,point.z,track.scale),"Patrol pause has a clear standing hull (map "+std::to_string(npc.level)+" at "+std::to_string(point.position.x)+","+std::to_string(point.position.y)+")");
  if(track.ai.mode==ActorAiMode::Guard){++guards;squads.insert(track.ai.squad);}}
 check(guards>=7&&squads.count(stateId("zone_military"))&&squads.count(stateId("zone_bandits")),"Military and bandit squads hold their posts");
 size_t fixtures=0;
 for(int level=0;level<g.chunkCount();++level){Game probe=zoneInspection(level,{12,12},0,0);fixtures+=probe.world().fixtures().size();
  for(const auto&spawn:probe.world().creatureSpawns())check(probe.hullFits(spawn.position,probe.groundHeight(spawn.position,float(World::TerrainMaxZ+1)),.5f),"Creature spawns stand clear of geometry");
  for(const auto&spawn:probe.world().pickupSpawns())check(probe.hullFits(spawn.position,probe.groundHeight(spawn.position,float(World::TerrainMaxZ+1)),Player::StandingHeight),"Pickups are reachable standing");}
 report<<"fixtures "<<fixtures<<'\n';
 check(fixtures>=1500,"The zone is dressed with at least 1500 trees, plants, ruins and props");
 // Real movement: walk the camp road east across a map seam on rough ground.
 {Game walk=zoneView({10,81.5f},{40,81.5f});walk.m_player.health=100000;
  const Vec2 junction{40-24,81.5f-72};bool crossed=false;
  for(int i=0;i<2400;++i){Vec2 pos=walk.player().pos;if(walk.level()==16&&length(pos-junction)<.4f){crossed=true;break;}
   if(i%60==0){report<<"walk "<<i<<" map "<<walk.level()<<" at "<<pos.x<<','<<pos.y<<'\n';report.flush();}
   Vec2 goal=walk.level()==16?junction:Vec2{30,9.5f};auto d=goal-pos;walk.m_player.angle=std::atan2(d.y,d.x);InputState step;step.forward=true;walk.update(step,1.f/60);}
  report<<"road walk ended in map "<<walk.level()<<" at "<<walk.player().pos.x<<','<<walk.player().pos.y<<'\n';
  check(crossed,"Walking the camp road crosses into the next map and reaches the junction");}
 for(int stage:{0,1,2,3,4,5,6,7,9,13,14,15,16}){try{auto scene=exclusionInspection(stage);check(!scene.dead(),"Zone view "+std::to_string(stage)+" stands clear and alive");}catch(const std::exception&e){report<<e.what()<<'\n';check(false,"Zone view "+std::to_string(stage)+" stands clear and alive");}}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}
