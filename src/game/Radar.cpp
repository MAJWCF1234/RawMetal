#include "Game.h"
#include "../world/CustomCampaign.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace retro {
// The yard and gatehouse cameras, patched through to the handset from
// Perimeter Ops: every Cleanup man up and about in this chunk, where he stands,
// which way he looks and what he makes of the player. Nothing before the patch.
std::vector<RadarContact> Game::radarContacts()const{
 std::vector<RadarContact> contacts;if(!state(CctvRadar))return contacts;
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];const auto&t=npc.track();
  if(npc.level!=m_level||t.ai.mode!=ActorAiMode::Guard||!autonomousActor(t)||npc.pose.clip==4||(t.deadState&&state(t.deadState))||!guardHostileToPlayer(actor))continue;
  contacts.push_back({npc.pose.position,npc.pose.yaw,t.ai.sightRange,std::acos(std::clamp(t.ai.sightDot,-1.f,1.f)),guardState(actor),actor});
 }return contacts;
}
bool Game::testRadar(){
 std::ofstream report("radar-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 auto find=[](const std::vector<RadarContact>&list,size_t actor){auto it=std::find_if(list.begin(),list.end(),[&](const RadarContact&c){return c.actor==actor;});return it==list.end()?nullptr:&*it;};
 // Three Cleanup men in a bare room: one with his back to the player, one
 // across the room looking away, one in the far corner looking away.
 auto campaign=std::make_shared<CustomCampaign>();campaign->name="Radar validation";campaign->key=883016;
 auto map=std::make_shared<AuthoredMapData>();map->name="Radar test room";map->definition={{0,0},{4.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
 AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
 TimedSequence timer;timer.timerState=stateId("radar_clock");timer.finishState=stateId("radar_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
 auto guard=[&](Vec2 at,float yaw,const std::string&name){ActorTrack t;t.timerState=timer.timerState;t.visual=ActorVisual::Cleanup;t.keys={{0,at,0,yaw},{30000,at,0,yaw}};t.scale=1.6f;t.health=90;
  t.damageState=stateId(name+"_damage");t.deadState=stateId(name+"_dead");t.ai.mode=ActorAiMode::Guard;t.ai.armed=true;t.ai.sightRange=12;t.ai.sightDot=.6f;t.ai.patrol={{at,0,5000}};map->actorTracks.push_back(t);};
 guard({5.5f,6.5f},kPi*.5f,"radar_back");guard({18.f,6.5f},kPi*.5f,"radar_east");guard({18.f,18.f},0,"radar_corner");campaign->maps.push_back(map);
 Game g(campaign);g.update({},1.f/60);
 check(g.npcActors().size()==3&&g.radarContacts().empty(),"Before the CCTV patch the handset shows no guards");
 g.setState(CctvRadar,1);auto contacts=g.radarContacts();bool placed=contacts.size()==3;
 for(const auto&c:contacts){const auto&pose=g.npcActors()[c.actor].pose;placed&=length(c.position-pose.position)<.01f&&std::fabs(c.yaw-pose.yaw)<.001f&&std::fabs(c.range-12)<.001f&&std::fabs(c.cone-std::acos(.6f))<.001f&&c.state==GuardState::Patrol;}
 check(placed,"After the patch every guard shows where he stands and looks, with his sight range and cone, on patrol");
 InputState q;q.subdue=true;g.update(q,1.f/60);for(int i=0;i<60;++i)g.update({},1.f/60);
 contacts=g.radarContacts();check(g.npcIncapacitated(0)&&!find(contacts,0)&&contacts.size()==2,"A knocked-out guard drops off the handset");
 // Walk round in front of the east man and let him get a good look.
 g.m_player.pos={21.f,6.5f};g.m_player.angle=kPi;g.m_player.z=0;
 for(int i=0;i<600&&g.guardState(1)!=GuardState::Alert;++i)g.update({},1.f/60);
 contacts=g.radarContacts();auto east=find(contacts,1);check(g.guardState(1)==GuardState::Alert&&east&&east->state==GuardState::Alert,"A guard who has the player shows as alerted");
 g.setState(g.friendlyKey(2,"guard_mode"),int(GuardState::Investigate));contacts=g.radarContacts();auto corner=find(contacts,2);check(corner&&corner->state==GuardState::Investigate,"A guard checking a noise shows as investigating");
 g.damageFriendly(2,90,g.player().pos,{ShotKind::Player,0,0});for(int i=0;i<30;++i)g.update({},1.f/60);
 contacts=g.radarContacts();check(g.state("radar_corner_dead")&&!find(contacts,2),"A dead guard drops off the handset");
 Game loaded(campaign);check(loaded.decodeSave(g.encodeSave())&&loaded.state(CctvRadar)&&loaded.radarContacts().size()==contacts.size(),"The patch survives save/load");
 // The real yard: its three men, nobody from the chunks either side.
 {auto yard=mapInspection({12.f,4.f},0,0,58);check(yard.radarContacts().empty(),"Security Yard: nothing on the handset before the patch");
  yard.setState(CctvRadar,1);auto seen=yard.radarContacts();bool local=true;for(const auto&c:seen)local&=yard.npcActors()[c.actor].level==58;
  int elsewhere=0;for(const auto&npc:yard.npcActors())elsewhere+=npc.level!=58&&npc.track().ai.mode==ActorAiMode::Guard;
  report<<"yard contacts "<<seen.size()<<", guards resident in other chunks "<<elsewhere<<'\n';
  check(seen.size()==3&&local,"Security Yard: the patch shows its three guards and only them");}
 // The patch is made from Perimeter Ops, and only there; the prologue's
 // security desk does not make it.
 {int patches=0;const Terminal*patch=nullptr;World ops(57);for(const auto&t:ops.terminals())if(t.activateState==CctvRadar){++patches;patch=&t;}
  int prologue=0;World office(PrologueChunk);for(const auto&t:office.terminals())prologue+=t.activateState==CctvRadar;
  check(patches==1&&prologue==0,"One CCTV patch terminal in Perimeter Ops and none in the prologue");
  if(patch){Game control;control.loadLevel(57,false);Vec2 front{std::sin(patch->yaw),std::cos(patch->yaw)};
   control.m_player.pos=patch->position+front*.8f;control.m_player.z=control.world().floorHeight(patch->position.x,patch->position.y)+patch->z;control.m_player.angle=std::atan2(-front.y,-front.x);
   InputState use{};use.use=true;control.updateInteraction(use,.01f);check(control.state(CctvRadar)==1,"Using the Perimeter Ops terminal makes the patch");}}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}
