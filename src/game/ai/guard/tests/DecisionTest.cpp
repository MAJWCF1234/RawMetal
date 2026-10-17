#include "../../../Game.h"
#include "../../DecisionClock.h"
#include "../../../../world/CustomCampaign.h"
#include <fstream>
namespace retro {
bool Game::testAiDecisionClock(){
 std::ofstream report("ai-decision-test.txt");bool passed=true;
 auto check=[&](bool ok,const char*name){report<<(ok?"PASS ":"FAIL ")<<name<<'\n';report.flush();passed&=ok;};
 for(int rate:{30,60,120,240}){
  ai::DecisionClock::State clock;int runs=0,elapsed=0;
  for(int frame=1;frame<=rate;++frame){int now=int(std::round(frame*1000./rate)),before=int(std::round((frame-1)*1000./rate));auto step=ai::DecisionClock::advance(clock,now-before,50,17,false,0);if(step.due){++runs;elapsed+=step.elapsed;}}
  check(runs>=20&&runs<=22&&elapsed+clock.elapsed==1000,"Decision rate and elapsed time follow simulation time at 30/60/120/240 Hz");
 }
 {ai::DecisionClock::State clock{40,0,1};auto step=ai::DecisionClock::advance(clock,8,50,0,true,0);check(step.due&&step.elapsed==8&&clock.remaining==32,"Urgent wake is immediate and preserves the periodic phase");}
 auto scene=[](){
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Decision clock validation";campaign->key=882747;
  auto map=std::make_shared<AuthoredMapData>();map->name="Clock patrol room";map->definition={{0,0},{22,22},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("decision_clock");timer.finishState=stateId("decision_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
  ActorTrack guard;guard.timerState=timer.timerState;guard.keys={{0,{5.5f,5.5f},0,kPi*.5f},{30000,{5.5f,5.5f},0,kPi*.5f}};
  guard.scale=1.6f;guard.health=70;guard.damageState=stateId("decision_damage");guard.deadState=stateId("decision_dead");guard.ai.mode=ActorAiMode::Guard;guard.ai.sightRange=4;guard.ai.armed=true;
  guard.ai.patrol={{{5.5f,5.5f},0,0},{{20.5f,5.5f},0,0}};map->actorTracks.push_back(guard);campaign->maps.push_back(map);return Game(campaign);
 };
 for(int rate:{60,120}){
  auto g=scene();int moved=0,skipped=0;ai::DecisionClock::resetStatistics();
  for(int frame=0;frame<rate;++frame){auto old=g.npcActors()[0].pose;g.update({},1.f/rate);if(frame>5&&lengthSq(g.npcActors()[0].pose.position-old.position)>.00000001f)++moved;if(!g.state(g.friendlyKey(0,"sense_due")))++skipped;}
  report<<"rate="<<rate<<" moving_ticks="<<moved<<" deferred_senses="<<skipped<<'\n';
  check(moved>=rate-8&&skipped>rate/2,"Patrol movement continues each tick while expensive senses are deferred");
  g.setState(g.friendlyKey(0,"weapon_recovery"),200);g.setState(g.friendlyKey(0,"sense_wait"),90);g.setState(g.friendlyKey(0,"plan_wait"),90);g.update({},1.f/rate);
  int recovery=g.state(g.friendlyKey(0,"weapon_recovery"));
  check(!g.state(g.friendlyKey(0,"sense_due"))&&recovery<200&&recovery>=200-int(std::ceil(1000.f/rate)),"Mechanical recovery advances every tick between sensing deadlines");
  g.setState(g.friendlyKey(0,"sense_wait"),90);g.setState(g.friendlyKey(0,"plan_wait"),90);
  g.damageFriendly(0,5,g.player().pos,{ShotKind::Player,0,0});g.update({},1.f/rate);
  check(g.state(g.friendlyKey(0,"sense_due"))&&g.state(g.friendlyKey(0,"plan_due"))&&g.state(g.friendlyKey(0,"guard_under_fire"))>0,"Damage wakes sensing and planning on the same simulation tick");
  Game loaded(g.m_customCampaign);bool restored=loaded.decodeSave(g.encodeSave());bool same=restored;
  if(restored)for(int frame=0;frame<rate;++frame){g.update({},1.f/rate);loaded.update({},1.f/rate);same&=lengthSq(g.npcActors()[0].pose.position-loaded.npcActors()[0].pose.position)<.00001f&&g.guardState(0)==loaded.guardState(0);for(const char*field:{"sense_wait","plan_wait","guard_recognition","task_ms","weapon_recovery"})same&=g.state(g.friendlyKey(0,field))==loaded.state(loaded.friendlyKey(0,field));}
  check(same,"Save restores decision phase and continues movement, recognition and mechanical clocks");
  auto close=scene();close.update({},1.f/rate);close.setState(close.friendlyKey(0,"sense_wait"),90);auto pose=close.npcActors()[0].pose;close.m_player.pos=pose.position+Vec2{std::sin(pose.yaw),std::cos(pose.yaw)}*2;close.update({},1.f/rate);
  check(close.state(close.friendlyKey(0,"sense_due"))&&close.guardState(0)==GuardState::Suspicious,"A close hostile entering view wakes recognition immediately");
  auto creature=scene();creature.update({},1.f/rate);creature.setState(creature.friendlyKey(0,"sense_wait"),90);auto guard=creature.npcActors()[0].pose;Enemy enemy;enemy.pos=enemy.home=enemy.lastKnown=guard.position+Vec2{std::sin(guard.yaw),std::cos(guard.yaw)}*2;creature.m_enemies={enemy};creature.update({},1.f/rate);
  check(creature.state(creature.friendlyKey(0,"sense_due"))&&creature.guardTarget(0).kind==ShotKind::Enemy,"A close hostile creature also wakes target selection immediately");
 }
 return passed;
}
}
