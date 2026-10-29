#include "../../../Game.h"
#include "../../Nodes.h"
#include "../../WalkQueryCache.h"
#include "../../friendly/Mind.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
#include "../../../../core/FrameProfile.h"
namespace retro {
bool Game::testNavGraph(){
 std::ofstream report("nav-graph-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 {
  ai::NodeGraph graph;for(int y=0;y<11;++y)for(int x=0;x<11;++x)graph.nodes.push_back({{1.5f+x*2,1.5f+y*2},0,ai::NodeHint::Path});
  for(int i=0;i<121;++i){auto&node=graph.nodes[size_t(i)];node.firstLink=int(graph.links.size());int x=i%11,y=i/11;
   for(auto d:{Vec2{1,0},Vec2{0,1},Vec2{-1,0},Vec2{0,-1}}){int nx=x+int(d.x),ny=y+int(d.y);if(nx>=0&&nx<11&&ny>=0&&ny<11)graph.links.push_back({ny*11+nx,2});}node.linkCount=int(graph.links.size())-node.firstLink;}
  int probes=0;auto walk=[&](Vec2 from,float feet,Vec2 to,float,float*end){++probes;if(end)*end=feet;return length(to-from)<=2.01f;};
  auto hop=ai::graphWaypoint(graph,{1.5f,1.5f},0,{21.5f,21.5f},0,1.6f,walk);
  check(length(hop-Vec2{3.5f,1.5f})<.001f&&probes<=28,"A 121-node route certifies its path with at most 28 live sweeps instead of sweeping the explored graph");
 }
 {
  ai::NodeGraph graph;graph.nodes={{{1,1},0,ai::NodeHint::Path,0,2},{{3,1},0,ai::NodeHint::Path,2,1},{{1,3},0,ai::NodeHint::Path,3,1},{{3,3},0,ai::NodeHint::Path,4,0}};graph.links={{1,2},{2,2},{3,2},{3,2}};
  bool closed=false;auto walk=[&](Vec2 from,float feet,Vec2 to,float,float*end){if(end)*end=feet;
   if(std::fabs(from.x-to.x)>.01f&&std::fabs(from.y-to.y)>.01f&&length(to-from)>.3f)return false;
   return !closed||!(std::fabs(from.x-3)<.01f&&std::fabs(to.x-3)<.01f&&from.y<2&&to.y>2);};
  auto open=ai::graphWaypoint(graph,{1,1},0,{3,3},0,1.6f,walk);closed=true;auto shut=ai::graphWaypoint(graph,{1,1},0,{3,3},0,1.6f,walk);
  check(length(open-Vec2{3,1})<.001f&&length(shut-Vec2{1,3})<.001f,"Closing a live edge reroutes the next query without retaining stale collision validation");
  closed=false;auto corner=ai::graphWaypoint(graph,{.8f,.8f},0,{3,3},0,1.6f,walk);
  check(length(corner-Vec2{1,1})<.001f,"An off-centre actor reaches its connector before a blocked diagonal to the next hop");
 }
 auto scene=[](bool wall){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Nav graph validation";campaign->key=882760;
  auto map=std::make_shared<AuthoredMapData>();map->name="Cover lattice room";map->definition={{0,0},{6.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("nav_clock");timer.finishState=stateId("nav_done");timer.durationMs=60000;timer.finishAtMs=60000;timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{4.5f,6.5f},0,kPi*.5f},{60000,{4.5f,6.5f},0,kPi*.5f}};guard.scale=1.6f;guard.health=100;
  guard.damageState=stateId("nav_damage");guard.deadState=stateId("nav_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.faction=AiFaction::Raider;guard.ai.armed=true;guard.ai.patrol={{{4.5f,6.5f},0,1000}};
  map->actorTracks.push_back(guard);
  if(wall)map->structures.push_back({8,4,8.25f,9,0,1.4f});
  campaign->maps.push_back(map);return Game(campaign);
 };
 // Slice boundaries must not alter pair order, feet, hints or link order.
 for(size_t budget:{size_t(1),size_t(17),size_t(256)}){
  auto g=scene(true);auto reference=g.navGraph(0,1.6f);
  g.m_levelNav={};g.m_levelNavReady=false;int slices=0;
  while(!g.advanceNavGraph(0,1.6f,budget))++slices;
  check(slices>0&&ai::sameNodeGraph(reference,g.m_levelNav),"Resumable pair budgets produce the exact synchronous graph");
  check(!g.world().doorPlanActive(),"Each graph slice releases its door overlay");
 }
 {auto seed=scene(true);auto campaign=std::make_shared<CustomCampaign>(*seed.m_customCampaign);
  auto first=std::make_shared<AuthoredMapData>(*campaign->maps[0]);first->definition.residencyGroup=0;
  auto second=std::make_shared<AuthoredMapData>(*first);second->definition.origin={24,0};second->definition.residencyGroup=1;second->actorTracks.clear();
  campaign->maps={first,second};Game g(campaign);auto reference=g.navGraph(0,1.6f);
  // Cross the seam, move beyond the residency safety bubble, then return.
  g.m_player.pos={24.1f,12};g.crossChunkBoundary();g.m_player.pos={12,12};g.updateStreaming(0);
  check(g.level()==1&&!g.chunkResident(0),"Leaving a guard chunk releases its geometry residency");
  g.m_player.pos={-.1f,12};g.crossChunkBoundary();auto before=perf::counts[perf::NavGraphReused];auto built=perf::counts[perf::NavGraphBuilt];
  const auto&retained=g.navGraph(0,1.6f);
  check(g.level()==0&&ai::sameNodeGraph(reference,retained)&&perf::counts[perf::NavGraphReused]>before&&perf::counts[perf::NavGraphBuilt]==built,
   "Reentering an unchanged guard chunk reuses its identical retained graph without rebuilding");
  g.navGraph(0,.8f);check(g.m_levelNav.height==.8f,"A different caller hull synchronously rebuilds the graph");
 }
 {auto open=scene(false),blocked=scene(true);float end=0;
  ai::WalkQueryCache::Scope outer(open.m_world,open.m_world.m_mapData);
  bool clear=open.friendlyWalkSegment(0,{6.5f,6.5f},0,{10.5f,6.5f},1.6f,&end);
  bool nested=false;{ai::WalkQueryCache::Scope inner(blocked.m_world,blocked.m_world.m_mapData);
   nested=blocked.friendlyWalkSegment(0,{6.5f,6.5f},0,{10.5f,6.5f},1.6f,&end);}
  check(clear&&!nested&&open.friendlyWalkSegment(0,{6.5f,6.5f},0,{10.5f,6.5f},1.6f,&end),"Nested worlds cannot reuse another world's live route memo and restore the outer scope");
 }
 {auto seed=scene(false);auto map=std::make_shared<AuthoredMapData>(*seed.m_customCampaign->maps[0]);
  map->doors.push_back({5,8,6.5f});map->cargoLift={10,10,13,13,0,2,1,stateId("cache_call"),stateId("cache_release"),stateId("cache_position"),stateId("cache_down"),stateId("cache_arrived"),stateId("cache_descended")};map->layers[0].rows[6][10]='B';
  map->structures.push_back({4,14,8,18,1.1f,1.2f});
  ActorTrack platform;platform.timerState=map->timedSequences[0].timerState;platform.platform=true;platform.footprint={2,2};platform.keys={{0,{16,11},0,0},{60000,{16,11},0,0}};map->actorTracks.push_back(platform);
  auto campaign=std::make_shared<CustomCampaign>(*seed.m_customCampaign);campaign->maps[0]=map;Game g(campaign);
  ai::WalkQueryCache cache;int probes=0;auto live=[&](Vec2 a,float z,Vec2 b,float h,float*end){++probes;return g.friendlyWalkSegment(0,a,z,b,h,end);};
  auto query=[&](Vec2 a,Vec2 b,float h=1.6f){cache.begin(g.m_world,g.m_world.m_mapData);float end=-123;return cache.walk(a,0,b,h,&end,live);};
  bool closed=query({6.5f,5.5f},{6.5f,7.5f});int before=probes;bool repeat=query({6.5f,5.5f},{6.5f,7.5f});
  check(!closed&&!repeat&&probes==before,"An unchanged blocked route reuses its exact collision result");
  g.m_world.setDoor(0,1,false);bool open=query({6.5f,5.5f},{6.5f,7.5f});g.m_world.setDoor(0,0,false);
  check(open&&!query({6.5f,5.5f},{6.5f,7.5f}),"Opening and closing a door invalidate retained route probes");
  bool low=query({10.5f,11.5f},{12.5f,11.5f});g.m_world.setCargoLiftHeight(1);bool raised=query({10.5f,11.5f},{12.5f,11.5f});g.m_world.setCargoLiftHeight(0);
  check(low&&!raised&&query({10.5f,11.5f},{12.5f,11.5f}),"Cargo lift motion invalidates both successful and failed route probes");
  size_t index=map->actorTracks.size()-1;auto pose=g.m_world.actorPose(index);pose.position={16,11};pose.z=0;g.m_world.setActorPose(index,pose);
  low=query({15.5f,11},{16.5f,11});pose.z=1;g.m_world.setActorPose(index,pose);raised=query({15.5f,11},{16.5f,11});pose.z=0;g.m_world.setActorPose(index,pose);
  check(low&&!raised&&query({15.5f,11},{16.5f,11}),"Moving platform collision invalidates retained route probes");
  pose.z=.2f;g.m_world.setActorPose(index,pose);cache.begin(g.m_world,g.m_world.m_mapData);float firstFeet=-123,repeatFeet=-456;
  bool first=cache.walk({15.5f,11},0,{16.5f,11},1.6f,&firstFeet,live);before=probes;
  bool again=cache.walk({15.5f,11},0,{16.5f,11},1.6f,&repeatFeet,live);
  check(first&&again&&firstFeet==.2f&&std::bit_cast<uint32_t>(firstFeet)==std::bit_cast<uint32_t>(repeatFeet)&&probes==before,"A retained successful sweep restores the exact nonzero destination support height");
  bool blocked=query({9.5f,6.5f},{11.5f,6.5f});g.m_world.destroyTile(10,6);
  check(!blocked&&query({9.5f,6.5f},{11.5f,6.5f}),"Destroying cover immediately opens the retained route query");
  check(!query({4.5f,16},{7.5f,16})&&query({4.5f,16},{7.5f,16},.8f),"Standing and crouching hulls have distinct exact route keys");
  before=probes;query({.1f,5},{1,5});query({.1f,5},{1,5});check(probes==before+2,"Seam-adjacent sweeps always retain live neighbour and permission validation");
  cache.begin(g.m_world,g.m_world.m_mapData,false);float end=0;before=probes;cache.walk({4,4},0,{5,4},1.6f,&end,live);cache.walk({4,4},0,{5,4},1.6f,&end,live);
  check(probes==before+2,"The uncached reference bypasses retained route probes");
  auto other=scene(false);cache.begin(other.m_world,other.m_world.m_mapData);before=probes;
  bool otherClear=cache.walk({4.5f,16},0,{7.5f,16},1.6f,&end,[&](Vec2 a,float z,Vec2 b,float h,float*result){++probes;return other.friendlyWalkSegment(0,a,z,b,h,result);});
  check(otherClear&&probes==before+1,"A different world and authored definition cannot reuse another world's route result");
 }
 {auto seed=scene(false);auto map=std::make_shared<AuthoredMapData>(*seed.m_customCampaign->maps[0]);map->definition.lift=true;World world(0,map);ai::WalkQueryCache cache;
  auto fits=[&](Vec2 at){cache.begin(world,world.m_mapData);float end=0;return cache.walk(at,0,at,1.6f,&end,[&](Vec2 p,float z,Vec2,float h,float*out){*out=z;return world.fits(p.x,p.y,z,h);});};
  bool northReady=fits({12,10.05f});world.m_liftPhase=World::LiftPhase::Crashed;world.m_liftTimer=3.64f;bool southShut=fits({12,13.95f});
  check(northReady&&!fits({12,10.05f})&&!southShut,"Changing lift phase invalidates the cab entrance collision result");
  world.m_liftTimer=3.65f;check(fits({12,13.95f}),"The crashed lift exit timer threshold invalidates the closed exit probe");
  world.m_liftHeight=1;check(!fits({12,12}),"Lift elevation invalidates retained cab floor and ceiling collision");
 }
 {auto g=scene(false);const auto&graph=g.navGraph(0,1.6f);int cover=0;for(const auto&n:graph.nodes)cover+=n.hint==ai::NodeHint::Cover;
  check(!graph.empty()&&cover==0,"An empty interior still builds a walkable lattice without inventing cover hints");
 }
 {auto g=scene(true);const auto&graph=g.navGraph(0,1.6f);int cover=0;for(const auto&n:graph.nodes)cover+=n.hint==ai::NodeHint::Cover;
  check(cover>=2&&!graph.links.empty(),"A solid waist-high wall publishes cover hints and connected links");
  AiTarget threat;threat.kind=ShotKind::Player;threat.level=0;threat.index=0;threat.position={12.5f,6.5f};threat.height=1.6f;threat.eye=1.42f;threat.faction=AiFaction::Player;
  g.rememberGuardTarget(0,threat);g.setState(g.friendlyKey(0,"guard_confirmed"),1);g.setState(g.friendlyKey(0,"guard_memory"),1);g.setState(g.friendlyKey(0,"guard_seen"),1);
  g.setState(g.friendlyKey(0,"guard_x"),12500);g.setState(g.friendlyKey(0,"guard_y"),6500);g.setState(g.friendlyKey(0,"guard_z"),0);
  g.m_player.pos={12.5f,6.5f};g.m_friendlyActors[0].pose.position={4.5f,6.5f};g.m_friendlyActors[0].pose.z=0;
  Vec2 goal{};float feet=0;bool found=g.findGuardGraphCover(0,0,g.npcActors()[0].pose,{12.5f,6.5f,1.0f},goal,feet);
  ActorPose sheltered=g.npcActors()[0].pose;sheltered.position=goal;sheltered.z=feet;
  check(found&&goal.x+.2f<8.f&&g.guardCovered(0,sheltered,g.guardTarget(0)),
   "Graph cover selects a retained west-side hint that physically occludes the threat");
  auto hop=g.guardGraphWaypoint(0,{4.5f,4.5f},0,{4.5f,8.5f},0,1.6f);
  check(length(hop-Vec2{4.5f,4.5f})>.2f,"Graph routing yields a live hop when a connected path exists around local geometry");
 }
 {auto g=scene(true);g.navGraph(0,1.6f);g.m_levelNavReady=true;int before=int(g.m_levelNav.nodes.size());
  g.loadLevel(0,false);check(!g.m_levelNavReady,"Loading a chunk invalidates the retained navigation graph");
  const auto&graph=g.navGraph(0,1.6f);check(g.m_levelNavReady&&int(graph.nodes.size())==before,"The graph rebuilds on demand after invalidation");
 }
 {auto g=scene(false);auto map=std::make_shared<AuthoredMapData>(*g.m_customCampaign->maps[0]);
  map->navHints.push_back({{10.5f,10.5f},0,1});map->navHints.push_back({{12.5f,10.5f},0,0});
  auto campaign=std::make_shared<CustomCampaign>(*g.m_customCampaign);campaign->maps[0]=map;Game hinted(campaign);
  const auto&graph=hinted.navGraph(0,1.6f);int cover=0;bool path=false,nearCover=false;
  for(const auto&n:graph.nodes){cover+=n.hint==ai::NodeHint::Cover;if(n.hint==ai::NodeHint::Path&&length(n.position-Vec2{12.5f,10.5f})<.6f)path=true;if(n.hint==ai::NodeHint::Cover&&length(n.position-Vec2{10.5f,10.5f})<.6f)nearCover=true;}
  check(nearCover&&path&&cover>=1,"Authored Path and Cover hints merge into the retained navigation graph without inventing walls");
 }
 {std::string records="--- CUSTOM_CAMPAIGN_DATA_START ---\nCAMPAIGN|Nav hint authoring|0\nMAP|0|Room|0|0|6.5|6.5|0|0|8|0.3||0|0|0|0|0\nLAYER|0|0|Floor|0|0\n";
  for(int y=0;y<24;++y)records+="ROW|0|0|"+std::to_string(y)+"|"+std::string(24,'.')+"\n";
  records+="NAV_HINT|0|10.5|10.5|0|1\nNAV_HINT|0|12.5|10.5|0|0\n--- CUSTOM_CAMPAIGN_DATA_END ---\n";
  std::ofstream file("nav-hint-authoring-fixture.txt");file<<records;file.close();
  try{auto authored=loadCustomCampaignFile("nav-hint-authoring-fixture.txt");const auto&hints=authored->maps[0]->navHints;
   check(hints.size()==2&&hints[0].hint==1&&hints[1].hint==0&&std::fabs(hints[0].position.x-10.5f)<.001f,"NAV_HINT records parse Path and Cover markers into AuthoredMapData");
   Game g(authored);const auto&graph=g.navGraph(0,1.6f);bool cover=false;
   for(const auto&n:graph.nodes)if(n.hint==ai::NodeHint::Cover&&length(n.position-Vec2{10.5f,10.5f})<.6f)cover=true;
   check(cover,"Parsed NAV_HINT Cover markers appear on the live retained graph");
  }catch(const std::exception&){check(false,"NAV_HINT records parse Path and Cover markers into AuthoredMapData");check(false,"Parsed NAV_HINT Cover markers appear on the live retained graph");}
 }
 {auto g=scene(true);
  AiTarget threat;threat.kind=ShotKind::Player;threat.level=0;threat.index=0;threat.position={12.5f,6.5f};threat.height=1.6f;threat.eye=1.42f;threat.faction=AiFaction::Player;
  g.rememberGuardTarget(0,threat);g.setState(g.friendlyKey(0,"guard_confirmed"),1);g.setState(g.friendlyKey(0,"guard_memory"),1);g.setState(g.friendlyKey(0,"guard_seen"),1);
  g.setState(g.friendlyKey(0,"guard_x"),12500);g.setState(g.friendlyKey(0,"guard_y"),6500);g.setState(g.friendlyKey(0,"guard_z"),0);
  g.m_player.pos={12.5f,6.5f};g.m_friendlyActors[0].pose.position={4.5f,6.5f};g.m_friendlyActors[0].pose.z=0;
  const auto&pose=g.npcActors()[0].pose;Point3 aim{12.5f,6.5f,1.f};Vec2 goal{};float feet=0;
  bool composed=g.findGuardCoverFromEnemy(0,0,pose,aim,goal,feet);
  ActorPose sheltered=pose;sheltered.position=goal;sheltered.z=feet;
  check(composed&&g.guardCovered(0,sheltered,g.guardTarget(0)),"FIND_COVER_FROM_ENEMY peer composes node then lateral cover that occludes the threat");
  const auto&graph=g.navGraph(0,1.6f);
  auto walk=[&](Vec2 from,float fromFeet,Vec2 to,float hull,float* endFeet){return g.friendlyWalkSegment(0,from,fromFeet,to,hull,endFeet);};
  auto covered=[&](Vec2 position,float z){ActorPose trial=pose;trial.position=position;trial.z=z;auto facing=Vec2{12.5f,6.5f}-position;if(lengthSq(facing)>.001f)trial.yaw=std::atan2(facing.x,facing.y);return g.guardCovered(0,trial,g.guardTarget(0));};
  auto reserved=[&](Vec2 position,float z){return g.guardPositionReserved(0,position,z);};
  Vec2 nearGoal{},farGoal{},originGoal{},soundGoal{};float nearFeet=0,farFeet=0,originFeet=0,soundFeet=0;
  bool near=ai::findNearNodeCover(graph,pose.position,pose.z,{12.5f,6.5f},1.6f,walk,covered,reserved,nearGoal,nearFeet,3.5f);
  bool far=ai::findFarNodeCover(graph,pose.position,pose.z,{12.5f,6.5f},1.6f,walk,covered,reserved,farGoal,farFeet,2.5f,8.f);
  bool origin=ai::findCoverFromOrigin(graph,pose.position,pose.z,1.6f,walk,covered,reserved,originGoal,originFeet);
  bool sound=ai::findCoverFromSound(graph,pose.position,pose.z,{12.5f,6.5f},1.6f,walk,covered,reserved,soundGoal,soundFeet,0.f,8.f);
  check(near&&length(nearGoal-pose.position)<=3.5f,"FIND_NEAR_NODE_COVER peer rejects cover beyond the near range");
  check(far&&length(farGoal-pose.position)>=2.5f,"FIND_FAR_NODE_COVER peer rejects cover closer than the minimum range");
  check(origin||sound,"FIND_COVER_FROM_ORIGIN and FIND_COVER_FROM_BEST_SOUND peers resolve against retained Cover hints");
  auto&mutablePose=g.m_friendlyActors[0].pose;auto ctx=g.makeGuardCtx(0,0,g.npcActors()[0].track(),mutablePose);
  ctx.setCoverGoal({5.5f,6.5f},0,2);ctx.setLastKnown({12.5f,6.5f},0);ctx.setConfirmed(true);
  check(ctx.hasCoverGoal()&&ctx.coverMode()==2&&length(ctx.coverGoal()-Vec2{5.5f,6.5f})<.01f&&g.state(g.friendlyKey(0,"guard_cover"))==1,
   "Typed GuardCtx cover accessors write the same save keys as the legacy string fields");
  check(ctx.confirmed()&&length(ctx.lastKnown()-Vec2{12.5f,6.5f})<.01f&&g.state(g.friendlyKey(0,"guard_confirmed"))==1,
   "Typed GuardCtx awareness accessors preserve save-compatible state keys");
 }
 {auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Ashfall-Exclusion-Zone.txt");
  // The open zone: the military checkpoint is the guarded map with authored cover.
  Game g(campaign);
  size_t assault=0;bool foundAssault=false;
  // The checkpoint's Assault: other squads in the zone have one too.
  for(size_t i=0;i<g.npcActors().size();++i)if(g.npcActors()[i].track().ai.role==GuardSquadRole::Assault&&g.npcActors()[i].track().ai.squad==stateId("zone_military")){assault=i;foundAssault=true;break;}
  check(foundAssault,"Exclusion Zone rebuild exposes the authored Assault");
  int post=foundAssault?g.npcActors()[assault].homeLevel:0;
  check(campaign&&campaign->maps.size()>size_t(post)&&campaign->maps[size_t(post)]->navHints.size()>=4,"Exclusion Zone authors Path/Cover NAV_HINT markers at the guarded checkpoint");
  bool creatures=false;for(const auto&map:campaign->maps)creatures|=!map->creatureSpawns.empty();
  check(creatures,"Exclusion Zone authors creatures for MaintainSchedule observation");
  bool escort=false;for(const auto&map:campaign->maps)for(const auto&track:map->actorTracks)escort|=track.ai.mode==ActorAiMode::Escort;
  check(escort,"Exclusion Zone authors an escort for civilian schedule observation");
  const auto&graph=g.navGraph(post,1.6f);int authoredCover=0;
  for(const auto&hint:campaign->maps[size_t(post)]->navHints)if(hint.hint){
   for(const auto&n:graph.nodes)if(n.hint==ai::NodeHint::Cover&&length(n.position-hint.position)<.7f){++authoredCover;break;}
  }
  check(authoredCover>=2,"Exclusion Zone NAV_HINT Cover markers merge into the live retained graph");
  check(foundAssault&&g.npcActors()[assault].track().ai.armed,"Exclusion Zone Assault is an armed checkpoint Guard");
  size_t escortIndex=0;bool foundEscort=false;
  for(size_t i=0;i<g.npcActors().size();++i)if(g.npcActors()[i].track().ai.mode==ActorAiMode::Escort){escortIndex=i;foundEscort=true;break;}
  check(foundEscort,"Exclusion Zone rebuild exposes the authored escort in the friendly actor list");
  if(foundEscort){CivilianMindView mind{g,escortIndex};mind.setFear(2000);mind.setThreat({20.5f,20.5f});
   check(mind.fear()==2000&&g.state(g.friendlyKey(escortIndex,"fear"))==2000,"Exclusion Zone escort typed mind writes the same fear save keys");}
 }
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}
