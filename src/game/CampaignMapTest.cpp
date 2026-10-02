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
 // Drive the real procedure through E and movement. State injection used to
 // conceal a sealed lower landing and controls with no visible confirmation.
 Game lift;lift.loadLevel(11,false);lift.m_enemies.clear();
 auto moveTo=[&](Game& g,int level,Vec2 point){Vec2 target=g.chunkOffset(level)+point;InputState walking{};walking.forward=true;
  for(int frame=0;frame<2400;++frame){Vec2 delta=target-(g.player().pos+g.chunkOffset(g.level()));if(length(delta)<.18f){for(int i=0;i<30;++i)g.update({},1.f/120);return true;}g.m_player.angle=std::atan2(delta.y,delta.x);g.update(walking,1.f/120);}
  out<<"Movement stalled: map "<<g.level()<<" at "<<g.player().pos.x<<','<<g.player().pos.y<<','<<g.player().z<<" toward "<<point.x<<','<<point.y<<'\n';return false;
 };
 auto pressUse=[&](Game& g){InputState key{};key.use=true;g.updateInteraction({},.01f);if(g.logTime()>0){g.updateInteraction(key,.01f);g.updateInteraction({},.01f);}g.updateInteraction(key,.01f);g.updateInteraction({},.01f);};
 if(!moveTo(lift,11,{2,2})||!moveTo(lift,11,{2,16.1f})||!moveTo(lift,11,{3.5f,16.1f})){check(false,"Walk west personnel route to brake console");return false;}
 lift.m_player.angle=kPi*.5f;pressUse(lift);check(lift.state("freight_brake"),"E releases brake from actual west-side console");
 if(!moveTo(lift,11,{2,16.1f})||!moveTo(lift,11,{2,21.5f})||!moveTo(lift,11,{12,21.5f})||!moveTo(lift,12,{12,2})){check(false,"Walk machinery route into auxiliary lift bay");return false;}
 if(!moveTo(lift,12,{6.5f,4.7f})){check(false,"Reach local control");return false;}
 lift.m_player.angle=kPi*.5f;pressUse(lift);check(lift.state("freight_local"),"E selects local control after brake release");
 if(!moveTo(lift,12,{12,4.7f})){check(false,"Reach call control");return false;}
 lift.m_player.angle=kPi*.5f;pressUse(lift);check(lift.state("freight_call"),"E calls platform through real control interaction");
 for(int frame=0;frame<1000;++frame)lift.update({},1.f/120);
 check(lift.state("freight_platform_arrived")&&std::fabs(lift.world().cargoLiftHeight()+12)<.02f,"Called platform reaches upper landing during gameplay");
 if(!moveTo(lift,12,{18,4.7f})){check(false,"Reach release control");return false;}
 lift.m_player.angle=kPi*.5f;pressUse(lift);check(lift.state("freight_release"),"E releases cage interlock after arrival");
 if(!moveTo(lift,12,{20.5f,4.7f})||!moveTo(lift,12,{20.5f,7.8f})||!moveTo(lift,12,{12,7.8f})){check(false,"Reach cage entry");return false;}
 for(int frame=0;frame<160;++frame)lift.update({},1.f/120);
 check(lift.world().doors().front().open>.99f,"Manual release actually opens cage gate without a second hidden interaction");
 if(!moveTo(lift,12,{12,11.3f})){check(false,"Walk through released cage door onto platform");return false;}
 for(int frame=0;frame<300;++frame)lift.update({},1.f/120);
 float mid=lift.world().cargoLiftHeight();Game restored;check(restored.decodeSave(lift.encodeSave())&&std::fabs(restored.world().cargoLiftHeight()-mid)<.01f,"Save/load preserves real passenger descent");
 for(int frame=0;frame<1000;++frame)restored.update({},1.f/120);
 check(restored.state("freight_descended")&&std::fabs(restored.player().z+19)<.02f,"Cage carries standing player to lower landing in normal gameplay");
 check(moveTo(restored,12,{12,18}),"Standing player walks off cage through lower threshold without jumping");
 // Above a tall doorway, the header must be shared visual/collision geometry.
 for(int level:{10,11,12,15}){Game frame;frame.loadLevel(level,false);for(const auto& door:frame.world().doors())if(door.entry||door.transfer){float base=frame.world().floorHeight((door.left+door.right)*.5f,door.y)+door.z;
  check(!frame.hullFits({(door.left+door.right)*.5f,door.y},base+3.2f,Player::StandingHeight),"Upper doorway header cannot be used as an unintended drop or bypass");}}
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
   TimedSequence motion;motion.timerState=stateId("demo_motion");motion.finishState=stateId("demo_cycle");motion.durationMs=4000;motion.finishAtMs=4000;motion.soundIntervalMs=0;motion.loop=true;map->timedSequences.push_back(motion);
   ActorTrack platform;platform.timerState=motion.timerState;platform.visual=ActorVisual::Cargo;platform.scale=1;platform.keys={{0,{16,16},1,0},{2000,{20,16},1,0},{4000,{16,16},1,0}};platform.loop=true;platform.platform=true;platform.footprint={2,2};map->actorTracks.push_back(platform);
   for(float x:{18.f,12.f}){ActorTrack target;target.timerState=motion.timerState;target.visual=ActorVisual::Wasp;target.scale=2;target.health=34;target.damageState=stateId(x>15?"far_damage":"near_damage");target.deadState=stateId(x>15?"far_dead":"near_dead");target.keys={{0,{x,6},0,0},{4000,{x,6},0,0}};map->actorTracks.push_back(target);}
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
 Game rider(campaign);rider.setState(stateId("demo_motion"),1);rider.updateMechanisms(0);rider.m_player.pos={16,16};rider.m_player.z=1;rider.m_player.grounded=true;
 for(int i=0;i<60;++i)rider.updateMechanisms(1.f/60);
 check(rider.player().pos.x>17.8f&&rider.player().pos.x<18.2f&&std::fabs(rider.player().z-1)<.01f,"Horizontal authored platform carries its passenger using shared collision");
 check(std::fabs(rider.world().supportBelow(rider.player().pos.x,16,1)-1)<.01f&&!rider.world().fits(rider.player().pos.x,16,.85f,.1f),"Platform visible surface and solid underside agree with collision");
 Game riderSave(campaign);check(riderSave.decodeSave(rider.encodeSave())&&std::fabs(riderSave.world().actorPose(0).position.x-rider.world().actorPose(0).position.x)<.001f,"Save/load restores platform and actor pose without restarting timeline");
 for(int i=0;i<240;++i)rider.updateMechanisms(1.f/60);
 check(rider.state("demo_motion")<=4000&&rider.state("demo_cycle"),"Looping machinery wraps safely and persists cycle completion");
 ActorTrack poseTrack;poseTrack.keys={{0,{2,3},0,3.1f,0,0},{1000,{4,5},2,-3.1f,0,1},{2000,{4,5},2,-3.1f,4,1}};
 auto halfway=sampleActor(poseTrack,500),end=sampleActor(poseTrack,9000);
 check(std::fabs(halfway.position.x-3)<.001f&&std::fabs(halfway.z-1)<.001f&&std::fabs(halfway.phase-.5f)<.001f&&halfway.yaw>3.1f,"Actor sampler interpolates position, skeletal phase and shortest yaw path");
 check(end.clip==4&&end.phase==1&&end.position.x==4,"Non-looping death pose remains present after sequence completion");
 Game targets(campaign);targets.m_enemies.clear();targets.m_player.pos={8,6};targets.m_player.z=0;targets.m_player.angle=0;targets.m_player.pitch=0;targets.shoot();
 check(targets.state("near_dead")&&!targets.state("far_damage"),"Actor hits select the nearest visible target rather than declaration order");
 Game targetSave(campaign);check(targetSave.decodeSave(targets.encodeSave())&&targetSave.world().actorPose(2).clip==4&&std::fabs(targetSave.world().actorPose(2).position.x-12)<.001f,"Killed authored actor preserves its death position and pose through save/load");
 auto invalid=std::make_shared<AuthoredMapData>(*campaign->maps[0]);invalid->actorTracks[0].keys[1].timeMs=0;bool rejected=false;try{World bad(0,invalid);}catch(const std::exception&){rejected=true;}check(rejected,"Malformed actor key ordering is rejected during campaign validation");
 Game receiving;receiving.loadLevel(13,false);receiving.m_enemies.clear();receiving.m_player.pos={12,2};receiving.updateMechanisms(1.f/60);check(!receiving.state("receiving_worker_encounter_ms"),"Worker encounter waits for the actual overlook rather than starting behind the entrance");receiving.m_player.pos={7.6f,9.5f};receiving.m_player.z=-19;for(int i=0;i<400;++i)receiving.updateMechanisms(1.f/60);
 Game receivingSave;check(receivingSave.decodeSave(receiving.encodeSave())&&receivingSave.state("receiving_worker_encounter_ms")==receiving.state("receiving_worker_encounter_ms"),"First worker encounter resumes from a saved skeletal timeline");
 receivingSave.loadLevel(14,false);for(int i=0;i<1200;++i)receivingSave.updateMechanisms(1.f/60);
 check(receivingSave.state("receiving_worker_encounter_complete"),"Receiving worker timeline continues across resident floor-level chunks");
 return pass;
}
}
