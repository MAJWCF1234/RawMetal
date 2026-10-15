#include "../../../Game.h"
#include "../Schedule.h"
#include "../../../../world/CustomCampaign.h"
#include <array>
#include <fstream>
#include <limits>
namespace retro {
bool Game::testGuardPatient(){
 std::ofstream report("guard-patient-test.txt");bool passed=true;int checks=0;
 auto check=[&](bool ok,std::string_view label){report<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';report.flush();passed&=ok;++checks;};
 auto scene=[](bool reverse,auto edit,bool opponent=true){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Squad patient rendezvous validation";campaign->key=882749;
  auto map=std::make_shared<AuthoredMapData>();map->name="Patient teamwork room";map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData floor;for(auto&row:floor.rows)row=std::string(24,'.');map->layers.push_back(floor);
  TimedSequence clock;clock.timerState=stateId("patient_clock");clock.finishState=stateId("patient_done");clock.durationMs=60000;clock.finishAtMs=60000;clock.soundIntervalMs=0;clock.loop=true;map->timedSequences.push_back(clock);
  ActorTrack prototype;prototype.timerState=clock.timerState;prototype.scale=1.6f;prototype.health=90;
  prototype.ai.mode=ActorAiMode::Guard;prototype.ai.armed=true;prototype.ai.faction=AiFaction::Raider;prototype.ai.squad=stateId("patient_team");prototype.ai.attackDamage=1;
  auto patient=prototype;patient.visual=ActorVisual::CleanupRusher;patient.damageState=stateId("patient_damage");patient.deadState=stateId("patient_dead");patient.ai.role=GuardSquadRole::Rusher;patient.ai.speed=2.1f;
  patient.keys={{0,{9.5f,6.5f},0,kPi*.5f},{60000,{9.5f,6.5f},0,kPi*.5f}};patient.ai.patrol={{{9.5f,6.5f},0,1000}};
  auto medic=prototype;medic.visual=ActorVisual::CleanupMedic;medic.damageState=stateId("patient_medic_damage");medic.deadState=stateId("patient_medic_dead");medic.ai.role=GuardSquadRole::Medic;medic.ai.speed=1;medic.ai.enableState=stateId("patient_medic_enabled");
  medic.keys={{0,{6.5f,6.5f},0,kPi*.5f},{60000,{6.5f,6.5f},0,kPi*.5f}};medic.ai.patrol={{{6.5f,6.5f},0,1000}};
  map->actorTracks=reverse?std::vector<ActorTrack>{patient,medic}:std::vector<ActorTrack>{medic,patient};
  if(opponent){auto target=prototype;target.ai={};target.ai.faction=AiFaction::Facility;target.visual=ActorVisual::Worker;target.health=5000;target.damageState=stateId("patient_opponent_damage");target.deadState=stateId("patient_opponent_dead");
   target.keys={{0,{14.5f,6.5f},0,0},{60000,{14.5f,6.5f},0,0}};map->actorTracks.push_back(target);}
  edit(*map);campaign->maps.push_back(map);Game g(campaign);g.setState(stateId("patient_medic_enabled"),1);return g;
 };
 auto tick=[](Game&g,int frames,int rate=60){for(int frame=0;frame<frames;++frame)g.update({},1.f/rate);};
 auto ids=[](const Game&g){std::array<size_t,2> result{};for(size_t actor=0;actor<g.npcActors().size();++actor){const auto&t=g.npcActors()[actor].track();
   if(t.damageState==stateId("patient_damage"))result[0]=actor;if(t.damageState==stateId("patient_medic_damage"))result[1]=actor;}return result;};
 auto scheduled=[](Game&g,size_t actor,GuardSchedule schedule){return g.state(g.friendlyKey(actor,"schedule"))==int(schedule);};
 auto safePose=[](Game&g,size_t actor){const auto&npc=g.npcActors()[actor];auto p=npc.pose.position;return p.x>=.35f&&p.x<=23.65f&&p.y>=.35f&&p.y<=23.65f&&std::fabs(npc.pose.z)<.001f&&g.friendlyWorld(npc.level).fits(p.x,p.y,npc.pose.z,npc.track().scale);};
 auto wound=[&](Game&g){size_t patient=ids(g)[0];g.setState(stateId("patient_damage"),45);
  // This is an acknowledged existing wound. Fresh-hit defense is tested
  // separately and must not mask the moving patient's rendezvous decision.
  g.setState(g.friendlyKey(patient,"guard_damage"),45);
 };
 auto start=[&](Game&g,int rate){size_t patient=ids(g)[0];int frame=0;while(frame++<rate*5&&g.npcActors()[patient].pose.clip!=1)tick(g,1,rate);
  bool moving=g.npcActors()[patient].pose.clip==1&&g.state(g.friendlyKey(patient,"guard_confirmed"));wound(g);return moving;
 };
 auto awaitMove=[&](Game&g,int rate){auto pair=ids(g);for(int frame=0;frame<rate*8;++frame){tick(g,1,rate);if(scheduled(g,pair[0],GuardSchedule::Patient)&&g.state(g.friendlyKey(pair[0],"guard_patient_move"))&&g.npcActors()[pair[0]].pose.clip==1)return true;}return false;};
 auto awaitTreat=[&](Game&g,int rate,int minimum=600){size_t medic=ids(g)[1];for(int frame=0;frame<rate*15;++frame){tick(g,1,rate);if(g.state(g.friendlyKey(medic,"support_ms"))>=minimum)return true;}return false;};
 for(int rate:{60,120})for(bool reverse:{false,true}){
  auto g=scene(reverse,[](auto&){});auto pair=ids(g);size_t patient=pair[0],medic=pair[1];check(start(g,rate),"A real Rusher starts ordinary forward movement before its half-health wound is offered to a slower Medic");
  auto before=g.npcActors()[patient].pose.position;bool withdrew=false,owned=false,held=false,safe=true,bounded=true,goalFixed=true;float retreat=0;Vec2 rendezvous{};bool captured=false;
  for(int frame=0;frame<rate*20&&g.state("patient_damage")==45;++frame){auto oldPatient=g.npcActors()[patient].pose.position,oldMedic=g.npcActors()[medic].pose.position;tick(g,1,rate);
   safe&=safePose(g,patient)&&safePose(g,medic)&&length(g.npcActors()[patient].pose.position-oldPatient)<=2.1f/rate+.003f;
   bounded&=g.m_npcNavBudget>=0&&g.m_npcNavBudget<=4;retreat=std::max(retreat,before.x-g.npcActors()[patient].pose.position.x);
   if(scheduled(g,patient,GuardSchedule::Patient)){owned|=g.state(g.friendlyKey(patient,"task_subject"))==int(medic)+1&&g.guardFlankSide(patient)==0&&!g.guardTaskAllows(patient,GuardTask::Throw);
    if(g.state(g.friendlyKey(patient,"guard_patient_move"))){withdrew=true;Vec2 goal{g.state(g.friendlyKey(patient,"task_goal_x"))*.001f,g.state(g.friendlyKey(patient,"task_goal_y"))*.001f};
     if(!captured){rendezvous=goal;captured=true;}else goalFixed&=length(goal-rendezvous)<.1f;
     safe&=length(g.npcActors()[medic].pose.position-oldMedic)<.025f;}
   }
   if(g.state(g.friendlyKey(medic,"support_ms")))held|=scheduled(g,patient,GuardSchedule::Patient)&&!g.state(g.friendlyKey(patient,"guard_patient_move"))&&g.npcActors()[patient].pose.clip==0&&
    length(g.npcActors()[patient].pose.position-g.npcActors()[medic].pose.position)<=1.35f&&g.guardTaskAllows(medic,GuardTask::Treat);
  }
  report<<"patient Hz="<<rate<<" reversed="<<reverse<<" withdrawal="<<retreat<<" damage="<<g.state("patient_damage")<<" charges="<<g.state(g.friendlyKey(medic,"support_used"))<<'\n';
  check(withdrew&&owned&&retreat>.5f&&goalFixed,"The moving wounded Rusher owns a reciprocal Patient task and withdraws to one fixed rear rendezvous instead of outrunning its Medic");
  check(held&&safe&&bounded,"The recipient holds within physical treatment reach while the Medic owns the kit and all movement preserves hulls and navigation budgets");
  check(g.state("patient_damage")==15&&g.state(g.friendlyKey(medic,"support_used"))==1&&!g.state(g.friendlyKey(patient,"support_used"))&&!g.state(g.friendlyKey(patient,"drone_used")),"One real treatment heals thirty damage with one Medic charge and no receiver resource spend at both rates and actor orders");
  auto healed=g.npcActors()[patient].pose.position;tick(g,rate*3,rate);
  check(!scheduled(g,patient,GuardSchedule::Patient)&&!g.state(g.friendlyKey(patient,"guard_patient_move"))&&g.npcActors()[patient].track().ai.role==GuardSquadRole::Rusher&&g.guardFlankSide(patient)==1&&
   g.npcActors()[patient].pose.position.x>healed.x+.25f&&g.state(g.friendlyKey(medic,"support_used"))==1,"After healing the patient releases rendezvous ownership and resumes its original Rusher advance/flank without another charge");
 }
 for(bool reverse:{false,true})for(bool treatment:{false,true}){
  auto g=scene(reverse,[](auto&){});auto pair=ids(g);start(g,60);bool reached=treatment?awaitTreat(g,60):awaitMove(g,60);
  check(reached,"Save fixture reaches an actual reciprocal moving or treating patient phase through normal updates");Game restored(g.m_customCampaign);bool decoded=restored.decodeSave(g.encodeSave());bool same=decoded;
  for(size_t actor:pair)for(const char*f:{"schedule","schedule_step","task","task_status","task_subject","task_goal_x","task_goal_y","task_goal_z","guard_patient_move","support_target","support_ms","support_used"})same&=g.state(g.friendlyKey(actor,f))==restored.state(restored.friendlyKey(actor,f));
  check(same&&scheduled(restored,pair[0],GuardSchedule::Patient),"Save restores the fixed patient goal, reciprocal Medic subject and kit phase even when the provider is authored later");
  tick(g,60*18);if(decoded)tick(restored,60*18);
  check(decoded&&g.state("patient_damage")==15&&restored.state("patient_damage")==15&&g.state(g.friendlyKey(pair[1],"support_used"))==1&&restored.state(restored.friendlyKey(pair[1],"support_used"))==1&&
   !scheduled(g,pair[0],GuardSchedule::Patient)&&!scheduled(restored,pair[0],GuardSchedule::Patient),"Saved and uninterrupted rendezvous finish one identical finite heal and release both jobs without replay");
 }
 for(int rate:{60,120}){
  auto g=scene(false,[](auto&map){for(auto&actor:map.actorTracks)if(actor.damageState==stateId("patient_damage")){actor.ai.role=GuardSquadRole::Default;actor.ai.armed=false;
    for(auto&key:actor.keys)key.position={10.5f,6.5f};actor.ai.patrol={{{10.5f,6.5f},0,60000}};}map.structures.push_back({8,5,8.2f,8,0,1});},false);
  auto pair=ids(g);wound(g);float moved=0,detour=0;bool safe=true;for(int frame=0;frame<rate*26&&g.state("patient_damage")==45;++frame){tick(g,1,rate);
   moved=std::max(moved,length(g.npcActors()[pair[0]].pose.position-Vec2{10.5f,6.5f}));detour=std::max(detour,std::fabs(g.npcActors()[pair[1]].pose.position.y-6.5f));safe&=safePose(g,pair[0])&&safePose(g,pair[1]);}
  check(moved<.02f&&detour>.4f&&safe&&!g.state(g.friendlyKey(pair[0],"guard_patient_move"))&&g.state("patient_damage")==15&&g.state(g.friendlyKey(pair[1],"support_used"))==1,
   "A stationary legacy patient stays at its authored position while the Medic retains its real low-cover detour and single physical treatment");
 }
 {auto g=scene(false,[](auto&){});auto pair=ids(g);start(g,60);check(awaitTreat(g,60,2300),"Danger interruption fixture approaches the real paired treatment completion event");
  auto position=g.npcActors()[pair[0]].pose.position;g.spawnTacticalGrenade({position.x,position.y,.1f},{0,0,0},{ShotKind::Player,0,0});tick(g,1);
  check(scheduled(g,pair[0],GuardSchedule::Danger)&&scheduled(g,pair[1],GuardSchedule::Danger)&&!g.state(g.friendlyKey(pair[0],"guard_patient_move"))&&
   !g.state(g.friendlyKey(pair[1],"support_ms"))&&!g.state(g.friendlyKey(pair[1],"support_used"))&&g.state("patient_damage")==45,"A real close grenade interrupts both patient and provider before the near-complete heal or finite charge is committed");
 }
 for(bool reverse:{false,true}){auto g=scene(reverse,[](auto&){});auto pair=ids(g);start(g,60);bool ready=awaitTreat(g,60,2360);
  check(ready,"Fresh-recipient-hit fixture reaches an actual paired treatment close to its finite heal event in either actor order");
  // The fifty-millisecond real update crosses the remaining kit boundary.
  // With a Medic authored first, an unguarded heal would reduce 63 damage
  // to 33 before the recipient senses its new eighteen-point hit.
  int before=g.state(g.friendlyKey(pair[1],"support_ms"));g.damageFriendly(pair[0],18,{14.5f,6.5f},{ShotKind::Actor,0,2});g.update({},.05f);
  unsigned bits=unsigned(g.state(g.friendlyKey(pair[0],"task_conditions")));report<<"recipient pending hit reversed="<<reverse<<" before="<<before<<" damage="<<g.state("patient_damage")
   <<" hurt="<<g.state(g.friendlyKey(pair[0],"guard_hurt_amount"))<<" bits="<<bits<<" used="<<g.state(g.friendlyKey(pair[1],"support_used"))<<'\n';
  check(ready&&g.state("patient_damage")==63&&!g.state(g.friendlyKey(pair[1],"support_used"))&&!g.state(g.friendlyKey(pair[1],"support_ms"))&&
   !g.state(g.friendlyKey(pair[1],"support_target"))&&(bits&GuardHeavyDamage)&&(bits&GuardHurt)&&g.state(g.friendlyKey(pair[0],"guard_hurt_amount"))==18&&
   scheduled(g,pair[0],GuardSchedule::Cover)&&!g.state(g.friendlyKey(pair[0],"guard_patient_move")),
   "A pending fresh heavy hit on the receiver blocks the Medic before healing can erase that damage cue, releases their claim and preserves defense independent of authoring order");
 }
 for(int invalid:{0,1,2}){
  auto g=scene(false,[](auto&){});auto pair=ids(g);start(g,60);check(awaitMove(g,60),"Provider interruption fixture starts a real active patient rendezvous");
  if(invalid==0)g.damageFriendly(pair[1],200,{14.5f,6.5f},{ShotKind::Actor,0,2});else if(invalid==1)g.setState(stateId("patient_medic_enabled"),0);else g.setState(g.friendlyKey(pair[1],"support_cooldown"),6000);
  tick(g,2);check(!scheduled(g,pair[0],GuardSchedule::Patient)&&!g.state(g.friendlyKey(pair[0],"guard_patient_move"))&&!g.state(g.friendlyKey(pair[1],"support_target"))&&
   !g.state(g.friendlyKey(pair[1],"support_used"))&&g.state("patient_damage")==45,"A dead, disabled or unavailable provider releases reciprocal patient movement without spending a treatment charge");
  Game restored(g.m_customCampaign);check(restored.decodeSave(g.encodeSave()),"Released provider/patient ownership preserves a valid save after interruption");
 }
 for(int invalid:{0,1,2}){
  auto g=scene(false,[&](auto&map){for(auto&actor:map.actorTracks)if(actor.ai.role==GuardSquadRole::Medic){if(invalid==0)actor.ai.squad=stateId("patient_other_team");if(invalid==1)actor.ai.faction=AiFaction::Neutral;}});auto pair=ids(g);
  if(invalid==2)g.setState(g.friendlyKey(pair[1],"support_used"),2);start(g,60);bool assigned=false;for(int frame=0;frame<60*10;++frame){tick(g,1);assigned|=scheduled(g,pair[0],GuardSchedule::Patient);}
  check(!assigned&&g.state("patient_damage")==45&&g.state(g.friendlyKey(pair[1],"support_used"))==(invalid==2?2:0),"A provider from another squad/faction or with exhausted stock cannot acquire or heal this patient");
 }
 {auto g=scene(false,[](auto&map){for(auto&actor:map.actorTracks){for(auto&key:actor.keys){std::swap(key.position.x,key.position.y);key.yaw=0;}for(auto&point:actor.ai.patrol)std::swap(point.position.x,point.position.y);}
   Door gate{};gate.left=6;gate.right=7;gate.y=8;gate.open=1;gate.opening=true;gate.requireState=stateId("patient_gate_permission");map.doors.push_back(gate);
  });g.setState(stateId("patient_gate_permission"),1);auto pair=ids(g);start(g,60);check(awaitMove(g,60),"Paused-route fixture begins a real patient withdrawal through an initially open permitted door");
  g.setState(stateId("patient_gate_permission"),0);g.m_world.setDoor(0,0,false);bool released=false,safe=true;
  for(int frame=0;frame<240;++frame){tick(g,1);released|=!scheduled(g,pair[0],GuardSchedule::Patient)&&!g.state(g.friendlyKey(pair[0],"guard_patient_move"));
   safe&=safePose(g,pair[0])&&!g.world().doorBlocks(g.npcActors()[pair[0]].pose.position.x,g.npcActors()[pair[0]].pose.position.y,g.npcActors()[pair[0]].pose.z,1.6f);}
  check(released&&safe&&!g.world().doors()[0].opening&&g.world().doors()[0].open==0&&g.state("patient_damage")==45&&!g.state(g.friendlyKey(pair[1],"support_used")),
   "Closing and locking the active rendezvous path releases the pair within a bounded wait and never heals or walks through the solid gate");
 }
 {auto g=scene(false,[](auto&map){auto second=map.actorTracks[0];second.damageState=stateId("patient_second_medic_damage");second.deadState=stateId("patient_second_medic_dead");
   for(auto&key:second.keys)key.position={6.5f,8.5f};second.ai.patrol={{{6.5f,8.5f},0,1000}};map.actorTracks.push_back(second);
  });auto pair=ids(g);start(g,60);bool unique=true;for(int frame=0;frame<60*18;++frame){tick(g,1);int claims=0;for(size_t actor=0;actor<g.npcActors().size();++actor)if(g.npcActors()[actor].track().ai.role==GuardSquadRole::Medic)
    claims+=g.state(g.friendlyKey(actor,"support_target"))==int(pair[0])+1;unique&=claims<=1;}
  int used=0;for(size_t actor=0;actor<g.npcActors().size();++actor)if(g.npcActors()[actor].track().ai.role==GuardSquadRole::Medic)used+=g.state(g.friendlyKey(actor,"support_used"));
  check(unique&&used==1&&g.state("patient_damage")==15,"Two eligible Medics never double-reserve the same recipient or multiply its single thirty-point treatment");
 }
 for(const char*field:{"guard_patient_move","task_subject","task_goal_x","task_goal_y","task_goal_z"}){
  auto g=scene(false,[](auto&){});auto pair=ids(g);start(g,60);awaitMove(g,60);g.setState(g.friendlyKey(pair[0],field),std::numeric_limits<int>::max());Game restored(g.m_customCampaign);
  check(!restored.decodeSave(g.encodeSave()),"Malformed saved patient movement, provider identity or fixed rendezvous coordinates are rejected safely");
 }
 report<<"checks="<<checks<<" result="<<(passed?"PASS":"FAIL")<<'\n';return passed;
}
}
