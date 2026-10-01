#include "Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <queue>
#include <unordered_set>
namespace retro {
bool Game::testCampaignMaps(){
 std::ofstream out("freight-district-test.txt");bool pass=true;
 auto check=[&](bool ok,const char* name){out<<name<<": "<<(ok?"PASS":"FAIL")<<'\n';pass&=ok;};
 check(CampaignChunkCount==32,"Eight freight areas allocated across 22 additional stitched chunks");
 int mutants=0;for(int level=10;level<32;++level){Game area;area.loadLevel(level,false);
  check(area.world().definition().musicCue==MusicCue::Freight,"Freight section has dedicated authored soundtrack");
  for(const auto&e:area.enemies())if(e.kind==Enemy::Kind::Mutant){++mutants;check(e.maxHp==180&&std::fabs(e.bodyTop()-e.bodyBottom()-1.85f)<.001f,"Mutated human has authored health and standing hull");check(std::fabs(area.groundHeight(e.pos,e.z+.2f)-e.z)<.02f,"Mutated human stands on authored floor");}
 }
 check(mutants==2,"Exactly two mutated humans across the new freight section");
 {Game original;original.loadLevel(17,false);auto it=std::find_if(original.m_enemies.begin(),original.m_enemies.end(),[](const Enemy&e){return e.kind==Enemy::Kind::Mutant;});
  if(it!=original.m_enemies.end()){it->hp=73;Game restored;bool saved=restored.decodeSave(original.encodeSave());auto found=std::find_if(restored.enemies().begin(),restored.enemies().end(),[](const Enemy&e){return e.kind==Enemy::Kind::Mutant;});check(saved&&found!=restored.enemies().end()&&found->hp==73,"Mutated human damage survives save/load");}
 }
 Game dispatch;dispatch.loadLevel(9,false);dispatch.m_enemies.clear();dispatch.m_player.pos={21.5f,22.f};dispatch.m_player.z=-12;dispatch.m_player.angle=kPi*.5f;dispatch.m_player.grounded=true;
 InputState through{};through.use=true;dispatch.updateInteraction(through,.01f);through={};
 for(int frame=0;frame<160;++frame)dispatch.update(through,1.f/120);
 through.forward=true;for(int frame=0;frame<100;++frame)dispatch.update(through,1.f/120);
 check(dispatch.level()==10&&std::fabs(dispatch.player().z+12)<.03f,"Waste Handling dispatch door opens through E and walking reaches Freight Access");
 dispatch.m_player.angle=-kPi*.5f;for(int frame=0;frame<100;++frame)dispatch.update(through,1.f/120);
 check(dispatch.level()==9&&std::fabs(dispatch.player().z+12)<.03f,"Freight Access permits walking back into Waste Handling");
 for(int level=10;level<32;++level){Game game;game.loadLevel(level,false);game.updateStreaming(0);
  out<<"MAP "<<level<<" / "<<CampaignMapNames[level]<<'\n';
  check(game.world().campaign()&&!game.world().custom(),"Freight geometry keeps native campaign identity");
  check(game.hullFits(game.player().pos,game.player().z,Player::StandingHeight),"Spawn has standing clearance");
  for(const auto& enemy:game.enemies())check(game.hullFits(enemy.pos,enemy.bodyBottom(),enemy.bodyTop()-enemy.bodyBottom()),"Authored enemy has clearance");
  for(const auto& light:game.world().lights())check(light.z+.13f<game.world().clearanceAbove(light.position.x,light.position.y,light.z)+.01f,"Lamp fits under its support");
  for(int d=0;d<int(game.world().doors().size());++d)game.m_world.openDoor(d);
  game.m_world.updateDoors(4);
  if(level==12)game.m_world.setCargoLiftHeight(-12);
  struct Node{int x,y;float z;};std::queue<Node> queue;std::unordered_set<long long> visited;
  auto key=[](Node n){return (static_cast<long long>(std::lround((n.z+100)*1000))<<16)|(n.y*192+n.x);};
  Node start{int(game.player().pos.x*8),int(game.player().pos.y*8),game.player().z};queue.push(start);visited.insert(key(start));
  std::vector<bool> controlsReached(game.world().terminals().size(),false);
  float low=start.z,high=start.z;std::vector<float> nearest(controlsReached.size(),999.f);
  while(!queue.empty()){
   auto a=queue.front();queue.pop();Vec2 p{(a.x+.5f)*.125f,(a.y+.5f)*.125f};
   low=std::min(low,a.z);high=std::max(high,a.z);
   for(size_t t=0;t<controlsReached.size();++t){const auto& terminal=game.world().terminals()[t];
    float z=game.world().floorHeight(terminal.position.x,terminal.position.y)+terminal.z;
    nearest[t]=std::min(nearest[t],length(p-terminal.position)+std::fabs(a.z-z));
    if(length(p-terminal.position)<1.6f&&std::fabs(a.z-z)<.3f)controlsReached[t]=true;
   }
   for(auto delta:{Vec2{-1,0},Vec2{1,0},Vec2{0,-1},Vec2{0,1}}){
    Node b{a.x+int(delta.x),a.y+int(delta.y),a.z};if(b.x<2||b.x>=190||b.y<2||b.y>=190)continue;
    Vec2 q{(b.x+.5f)*.125f,(b.y+.5f)*.125f};b.z=game.groundHeight(q,a.z+.215f);
    if(std::fabs(b.z-a.z)>.3f||!game.hullFits(q,b.z,Player::StandingHeight)||!visited.insert(key(b)).second)continue;queue.push(b);
   }
  }
  out<<"Route visited "<<visited.size()<<" nodes; elevation "<<low<<" to "<<high<<'\n';
  if(level==17||level==18||level==19||level==28)check(high>=((level==17||level==28)?-19.f:level==18?-13.f:-7.f)-.02f,"Stair route reaches its upper exit landing");
  if(level==22)check(low<=-24.98f,"Transfer stair route reaches the sorting floor");
  for(size_t t=0;t<controlsReached.size();++t){out<<game.world().terminals()[t].title<<" (nearest "<<nearest[t]<<"): ";check(controlsReached[t],"Control is approachable from the chunk entry with the standing player hull");}
 }
 Game lift;lift.loadLevel(12,false);lift.m_enemies.clear();
 const auto& controls=lift.world().terminals();
 check(controls.size()==3&&controls[0].requireState==stateId("freight_brake")&&controls[1].requireState==stateId("freight_local")&&controls[2].requireState==stateId("freight_platform_arrived"),"Local / call / door controls enforce the freight procedure");
 lift.setState(stateId("freight_brake"),1);lift.setState(stateId("freight_local"),1);lift.setState(stateId("freight_call"),1);
 for(int frame=0;frame<480;++frame)lift.updateMechanisms(1.f/60);
 check(lift.state(stateId("freight_platform_arrived"))&&std::fabs(lift.world().cargoLiftHeight()+12)<.02f,"Called freight platform reaches the upper landing");
 lift.m_player.pos={12,12};lift.m_player.z=-12;lift.setState(stateId("freight_release"),1);
 for(int frame=0;frame<240;++frame)lift.updateMechanisms(1.f/60);
 float mid=lift.world().cargoLiftHeight();Game restored;check(restored.decodeSave(lift.encodeSave())&&std::fabs(restored.world().cargoLiftHeight()-mid)<.01f,"Save/load preserves a freight lift in motion");
 for(int frame=0;frame<240;++frame)lift.updateMechanisms(1.f/60);
 check(lift.state(stateId("freight_descended"))&&std::fabs(lift.player().z+19)<.02f,"Cage carries a standing player down and unlocks receiving");
 Game warehouse;warehouse.loadLevel(16,false);warehouse.m_enemies.clear();warehouse.updateStreaming(0);
 check(warehouse.chunkResident(17)&&warehouse.chunkResident(18)&&warehouse.chunkResident(19),"All four parts of the 48m warehouse are resident together");
 warehouse.m_player.pos={24.05f,12};warehouse.crossChunkBoundary();check(warehouse.level()==17&&std::fabs(warehouse.player().pos.x-.05f)<.01f,"East seam enters the adjacent warehouse chunk using world coordinates");
 struct Crossing{int level;Vec2 start;float z,angle;int destination;bool crouch=false;};
 for(const auto& c:std::array<Crossing,5>{{{16,{23,12},-25,0,17},{17,{12,23},-19,kPi*.5f,18},{18,{1,12},-13,kPi,19},{13,{23,12},-25,0,14},{14,{12,23},-25,kPi*.5f,15}}}){
  Game walk;walk.loadLevel(c.level,false);walk.m_enemies.clear();walk.m_player.pos=c.start;walk.m_player.z=c.z;walk.m_player.angle=c.angle;walk.m_player.grounded=true;
  InputState input{};input.forward=true;for(int frame=0;frame<60;++frame)walk.update(input,1.f/120);
  out<<"Seam "<<c.level<<" -> "<<c.destination<<": ";check(walk.level()==c.destination&&std::fabs(walk.player().z-c.z)<.03f,"Actual movement crosses the hall seam without a collision or floor-height jump");
 }
 return pass;
}
bool Game::testMechanisms(){
 std::ofstream out("mechanisms-test.txt");bool pass=true;
 auto check=[&](bool ok,const char* label){out<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';pass&=ok;};
 auto campaign=std::make_shared<CustomCampaign>();campaign->name="Shared mechanism test";campaign->key=12345;
 for(int i=0;i<3;++i){auto map=std::make_shared<AuthoredMapData>();map->name="Test hall";map->definition={{float(i*24),0},{3,3},0,Environment::Interior,false,8,.3f,i<2?42:43};
  map->openEast=i<2;map->openWest=i>0;AuthoredLayerData layer;for(auto& row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  if(i==0){map->cargoLift={8,8,12,12,0,3,1,stateId("demo_call"),stateId("demo_release"),stateId("demo_position"),stateId("demo_down"),stateId("demo_arrived"),stateId("demo_descended")};
   TimedSequence sequence;sequence.timerState=stateId("demo_timer");sequence.finishState=stateId("demo_finished");sequence.durationMs=1000;sequence.finishAtMs=500;sequence.soundIntervalMs=0;map->timedSequences.push_back(sequence);
  }campaign->maps.push_back(map);
 }
 Game game(campaign);check(game.chunkResident(1)&&!game.chunkResident(2),"Custom campaign uses authored residency groups without campaign map IDs");
 game.setState(stateId("demo_call"),1);for(int i=0;i<240;++i)game.updateMechanisms(1.f/60);
 check(game.state("demo_arrived")&&std::fabs(game.world().cargoLiftHeight()-3)<.01f,"Custom lift reaches upper landing using its own state names");
 check(game.state("demo_finished")&&game.state("demo_timer")==1000,"Custom timeline triggers and completes using its own state names");
 game.m_player.pos={10,10};game.m_player.z=3;game.setState(stateId("demo_release"),1);for(int i=0;i<60;++i)game.updateMechanisms(1.f/60);
 float mid=game.world().cargoLiftHeight();Game restored(campaign);check(restored.decodeSave(game.encodeSave())&&std::fabs(restored.world().cargoLiftHeight()-mid)<.01f,"Custom moving lift preserves position through save/load");
 for(int i=0;i<180;++i)restored.updateMechanisms(1.f/60);
 check(restored.state("demo_descended")&&std::fabs(restored.player().z)<.01f,"Shared lift carries custom-campaign passenger to the lower landing");
 check(!game.state("freight_call")&&!game.state("freight_worker_dead"),"Engine mechanism updates create no campaign-specific state");
 return pass;
}
}
