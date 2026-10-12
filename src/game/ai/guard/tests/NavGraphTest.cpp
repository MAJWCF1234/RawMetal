#include "../../../Game.h"
#include "../../Nodes.h"
#include "../../friendly/Mind.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::testNavGraph(){
 std::ofstream report("nav-graph-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
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
 {auto campaign=loadCustomCampaignFile(std::filesystem::path("custom maps")/"RawMetal-Horror-OpenWorld.txt");
  check(campaign&&!campaign->maps.empty()&&campaign->maps[0]->navHints.size()>=4,"Ashfall Guard Lab authors Path/Cover NAV_HINT markers on the playground chunk");
  check(campaign->maps.size()>1&&!campaign->maps[1]->creatureSpawns.empty(),"Ashfall Guard Lab authors a cargo-chunk creature for MaintainSchedule observation");
  bool escort=false;for(const auto&track:campaign->maps[0]->actorTracks)escort|=track.ai.mode==ActorAiMode::Escort;
  check(escort,"Ashfall Guard Lab authors a Facility escort for civilian schedule observation");
  Game g(campaign);const auto&graph=g.navGraph(0,1.6f);int authoredCover=0;
  for(const auto&n:graph.nodes)if(n.hint==ai::NodeHint::Cover&&(length(n.position-Vec2{17.4f,10.f})<.7f||length(n.position-Vec2{19.3f,8.f})<.7f||length(n.position-Vec2{20.f,14.9f})<.7f))++authoredCover;
  check(authoredCover>=2,"Playground NAV_HINT Cover markers merge into the live retained graph");
  AiTarget threat;threat.kind=ShotKind::Player;threat.level=0;threat.index=0;threat.position={16.7f,10.1f};threat.height=1.6f;threat.eye=1.42f;threat.faction=AiFaction::Player;
  g.rememberGuardTarget(0,threat);g.setState(g.friendlyKey(0,"guard_confirmed"),1);g.setState(g.friendlyKey(0,"guard_memory"),1);g.setState(g.friendlyKey(0,"guard_seen"),1);
  g.m_player.pos={16.7f,10.1f};Vec2 goal{};float feet=0;
  bool found=g.findGuardCoverFromEnemy(0,0,g.npcActors()[0].pose,{16.7f,10.1f,1.f},goal,feet);
  ActorPose sheltered=g.npcActors()[0].pose;sheltered.position=goal;sheltered.z=feet;
  check(found&&g.guardCovered(0,sheltered,g.guardTarget(0)),"Playground solo guard resolves FIND_COVER_FROM_ENEMY peer cover against the player start");
  size_t escortIndex=0;bool foundEscort=false;
  for(size_t i=0;i<g.npcActors().size();++i)if(g.npcActors()[i].track().ai.mode==ActorAiMode::Escort){escortIndex=i;foundEscort=true;break;}
  check(foundEscort,"Playground rebuild exposes the authored escort in the friendly actor list");
  if(foundEscort){CivilianMindView mind{g,escortIndex};mind.setFear(2000);mind.setThreat({21.5f,17.5f});
   check(mind.fear()==2000&&g.state(g.friendlyKey(escortIndex,"fear"))==2000,"Playground escort typed mind writes the same fear save keys");}
 }
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}
