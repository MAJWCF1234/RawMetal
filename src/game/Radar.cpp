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
  // The wedge reaches as far as he sees: further on Caution or in Alert.
  contacts.push_back({npc.pose.position,npc.pose.yaw,t.ai.sightRange*(guardWary(actor)?1.15f:1.f),std::acos(std::clamp(t.ai.sightDot,-1.f,1.f)),guardState(actor),actor});
 }return contacts;
}
bool Game::testRadar(){
 std::ofstream report("radar-test.txt");int checks=0,failed=0;
 auto check=[&](bool ok,const char*text){++checks;failed+=!ok;report<<(ok?"PASS ":"FAIL ")<<text<<'\n';report.flush();};
 auto find=[](const std::vector<RadarContact>&list,size_t actor){auto it=std::find_if(list.begin(),list.end(),[&](const RadarContact&c){return c.actor==actor;});return it==list.end()?nullptr:&*it;};
 auto inside=[](Vec2 p){return p.x>=0&&p.y>=0&&p.x<24&&p.y<24;};
 // Three Cleanup men in a bare room: one with his back to the player, one six
 // metres past him looking away (in earshot of a choke), one in the far
 // corner looking west along the room.
 auto campaign=std::make_shared<CustomCampaign>();campaign->name="Radar validation";campaign->key=883031;
 auto map=std::make_shared<AuthoredMapData>();map->name="Radar test room";map->definition={{0,0},{4.5f,6.5f},0,Environment::Interior,false,8,.3f,42};
 AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
 TimedSequence timer;timer.timerState=stateId("radar_clock");timer.finishState=stateId("radar_done");timer.soundIntervalMs=0;timer.loop=true;map->timedSequences.push_back(timer);
 auto guard=[&](Vec2 at,float yaw,const std::string&name){ActorTrack t;t.timerState=timer.timerState;t.visual=ActorVisual::Cleanup;t.keys={{0,at,0,yaw},{30000,at,0,yaw}};t.scale=1.6f;t.health=90;
  t.damageState=stateId(name+"_damage");t.deadState=stateId(name+"_dead");t.ai.mode=ActorAiMode::Guard;t.ai.armed=true;t.ai.sightRange=12;t.ai.sightDot=.6f;t.ai.patrol={{at,0,5000}};map->actorTracks.push_back(t);};
 guard({5.5f,6.5f},kPi*.5f,"radar_back");guard({12.f,6.5f},kPi*.5f,"radar_east");guard({18.f,18.f},-kPi*.5f,"radar_corner");campaign->maps.push_back(map);
 Game g(campaign);g.update({},1.f/60);
 check(g.npcActors().size()==3&&g.radarContacts().empty(),"Before the CCTV patch the handset shows no guards");
 g.setState(CctvRadar,1);auto contacts=g.radarContacts();bool placed=contacts.size()==3;
 for(const auto&c:contacts){const auto&pose=g.npcActors()[c.actor].pose;placed&=length(c.position-pose.position)<.01f&&std::fabs(c.yaw-pose.yaw)<.001f&&std::fabs(c.range-12)<.001f&&std::fabs(c.cone-std::acos(.6f))<.001f&&c.state==GuardState::Patrol;}
 check(placed,"After the patch every guard shows where he stands and looks, with his sight range and cone, on patrol");
 // A man on Caution sees 15% further, and his wedge says so; the others keep theirs.
 {g.startGuardCaution(1);const auto wary=g.radarContacts();bool longer=wary.size()==3;
  for(const auto&c:wary)longer&=std::fabs(c.range-(c.actor==1?12*1.15f:12.f))<.001f;
  g.setState(g.guardCautionKey(1),0);check(longer&&g.radarContacts()[1].range==12,"A guard on Caution draws a wedge as long as he can now see");}
 // Step into the corner man's view for a moment, seven metres off: he
 // squints. Hold it until he half makes the player out, then get behind him;
 // he walks over to the spot and sweeps it. Nobody else can see either spot.
 g.m_player.pos={11.f,18.f};g.m_player.angle=0;
 for(int i=0;i<120&&g.guardState(2)==GuardState::Patrol;++i)g.update({},1.f/60);
 contacts=g.radarContacts();auto corner=find(contacts,2);check(g.guardState(2)==GuardState::Suspicious&&corner&&corner->state==GuardState::Suspicious,"A guard who glimpses the player shows as suspicious");
 const int recognize=g.npcActors()[2].track().ai.recognitionMs;
 for(int i=0;i<300&&g.guardState(2)==GuardState::Suspicious&&g.state(g.friendlyKey(2,"guard_recognition"))*2<recognize;++i)g.update({},1.f/60);
 bool unsure=g.guardState(2)==GuardState::Suspicious;g.m_player.pos={22.f,18.f};
 for(int i=0;i<1200&&g.guardState(2)!=GuardState::Search;++i)g.update({},1.f/60);
 contacts=g.radarContacts();corner=find(contacts,2);check(unsure&&g.guardState(2)==GuardState::Search&&corner&&corner->state==GuardState::Search,"A guard sweeping where he lost the player shows as searching");
 // Back to the first man, from behind, and choke him: the second hears it.
 {const auto pose=g.npcActors()[0].pose;Vec2 facing{std::sin(pose.yaw),std::cos(pose.yaw)};g.m_player.pos=pose.position-facing*.95f;g.m_player.angle=std::atan2(facing.y,facing.x);}
 InputState q;q.subdue=true;g.update(q,1.f/60);
 contacts=g.radarContacts();auto east=find(contacts,1);check(g.npcIncapacitated(0)&&g.guardState(1)==GuardState::Investigate&&east&&east->state==GuardState::Investigate,"A guard who hears the choke shows as investigating");
 for(int i=0;i<60;++i)g.update({},1.f/60);
 contacts=g.radarContacts();check(g.npcIncapacitated(0)&&!find(contacts,0)&&contacts.size()==2,"A knocked-out guard drops off the handset");
 // Stand in front of the second man and let him get a good look.
 {const auto pose=g.npcActors()[1].pose;Vec2 facing{std::sin(pose.yaw),std::cos(pose.yaw)};auto at=pose.position+facing*3.f;
  g.m_player.pos={std::clamp(at.x,1.f,23.f),std::clamp(at.y,1.f,23.f)};g.m_player.angle=std::atan2(pose.position.y-g.m_player.pos.y,pose.position.x-g.m_player.pos.x);g.m_player.z=0;}
 for(int i=0;i<600&&g.guardState(1)!=GuardState::Alert;++i)g.update({},1.f/60);
 contacts=g.radarContacts();east=find(contacts,1);check(g.guardState(1)==GuardState::Alert&&east&&east->state==GuardState::Alert,"A guard who has the player shows as alerted");
 g.damageFriendly(2,90,g.player().pos,{ShotKind::Player,0,0});for(int i=0;i<30;++i)g.update({},1.f/60);
 contacts=g.radarContacts();check(g.state("radar_corner_dead")&&!find(contacts,2),"A dead guard drops off the handset");
 {Game loaded(campaign);bool same=loaded.decodeSave(g.encodeSave())&&loaded.state(CctvRadar)!=0;auto after=loaded.radarContacts();same&=after.size()==contacts.size()&&!after.empty();
  for(size_t i=0;same&&i<after.size();++i)same&=after[i].actor==contacts[i].actor&&after[i].state==contacts[i].state&&length(after[i].position-contacts[i].position)<.01f;
  check(same,"The patch survives save/load: the same men, where they were, as they were");}
 // A guard who chases the player over a stitched seam shows on the far side's
 // handset only once he is across, and then in that chunk's own frame.
 {auto seam=std::make_shared<CustomCampaign>();seam->name="Radar seam validation";seam->key=883032;
  for(int level=0;level<2;++level){auto m=std::make_shared<AuthoredMapData>();m->name="Radar seam "+std::to_string(level);Vec2 origin=level?Vec2{24,0}:Vec2{};
   m->definition={origin,{18.5f,18.5f},0,Environment::Interior,false,8,.3f,100+level};AuthoredLayerData floor;for(auto&row:floor.rows)row=std::string(24,'.');m->layers.push_back(floor);m->openEast=level==0;m->openWest=level==1;
   if(level==0){TimedSequence clock;clock.timerState=stateId("radar_seam_clock");clock.finishState=stateId("radar_seam_done");clock.durationMs=60000;clock.finishAtMs=60000;clock.soundIntervalMs=0;clock.loop=true;m->timedSequences.push_back(clock);
    ActorTrack t;t.timerState=clock.timerState;t.keys={{0,{22.3f,6.5f},0,kPi*.5f},{60000,{22.3f,6.5f},0,kPi*.5f}};t.scale=1.6f;t.health=70;t.damageState=stateId("radar_seam_damage");t.deadState=stateId("radar_seam_dead");
    t.ai.mode=ActorAiMode::Guard;t.ai.patrol={{{22.3f,6.5f},0,1000}};m->actorTracks.push_back(t);}
   seam->maps.push_back(m);}
  Game s(seam);s.setState(CctvRadar,1);auto tick=[&]{s.updateStreaming(1.f/60);s.update({},1.f/60);};tick();
  bool home=s.level()==0&&s.radarContacts().size()==1;
  s.m_player.pos=Vec2{26.5f,6.5f}-s.chunkOffset(s.level());s.crossChunkBoundary();s.updateStreaming(0);
  bool across=s.level()==1&&s.npcActors()[0].level==0&&s.radarContacts().empty();
  for(int i=0;i<120&&s.guardState(0)!=GuardState::Alert;++i)tick();
  bool alerted=s.guardState(0)==GuardState::Alert;s.m_player.pos={18.5f,18.5f};
  bool hidden=true,local=true;int shown=0;
  for(int i=0;i<60*9;++i){tick();const auto&npc=s.npcActors()[0];auto list=s.radarContacts();
   if(npc.level!=s.level()){hidden&=list.empty();continue;}
   ++shown;local&=list.size()==1&&inside(list[0].position)&&length(list[0].position-npc.pose.position)<.001f;}
  report<<"seam: alerted "<<alerted<<", frames on the far side's handset "<<shown<<'\n';
  check(home&&across&&alerted&&hidden&&shown>0&&local,"A guard chasing over a seam shows on the far side only once he is across, in that chunk's frame");}
 // The real yard: its three men, nobody from the chunks either side, each
 // where the yard has him (the yard is not at the world origin).
 {auto yard=mapInspection({12.f,4.f},0,0,58);check(yard.radarContacts().empty(),"Security Yard: nothing on the handset before the patch");
  yard.setState(CctvRadar,1);auto seen=yard.radarContacts();bool local=true,framed=true;const RadarContact*assault=nullptr;
  for(const auto&c:seen){const auto&npc=yard.npcActors()[c.actor];local&=npc.level==58;framed&=inside(c.position);if(npc.homeLevel==58&&npc.track().ai.role==GuardSquadRole::Assault)assault=&c;}
  int elsewhere=0;for(const auto&npc:yard.npcActors())elsewhere+=npc.level!=58&&npc.track().ai.mode==ActorAiMode::Guard;
  auto origin=yard.chunkOffset(58);
  report<<"yard contacts "<<seen.size()<<", guards resident in other chunks "<<elsewhere<<", yard origin "<<origin.x<<','<<origin.y<<'\n';
  if(assault)report<<"yard assault man "<<assault->position.x<<','<<assault->position.y<<" yaw "<<assault->yaw<<'\n';
  check(seen.size()==3&&local,"Security Yard: the patch shows its three guards and only them");
  check(lengthSq(origin)>1&&framed&&assault&&length(assault->position-Vec2{12.6f,10.2f})<.05f&&std::fabs(std::remainder(assault->yaw,2*kPi))<.01f,"Security Yard: each man in the yard's own frame, the assault man at his first post looking south");}
 // The patch is made from Perimeter Ops, and only there; the prologue's
 // security desk does not make it.
 {int patches=0;const Terminal*patch=nullptr;World ops(57);for(const auto&t:ops.terminals())if(t.activateState==CctvRadar){++patches;patch=&t;}
  int prologue=0;World office(PrologueChunk);for(const auto&t:office.terminals())prologue+=t.activateState==CctvRadar;
  check(patches==1&&prologue==0,"One CCTV patch terminal in Perimeter Ops and none in the prologue");
  // The Ops man's last post is a step from the patch, facing it: choke him
  // there, and his body lies within dragging reach of the bench.
  if(patch){Game control;control.loadLevel(57,false);size_t man=control.npcActors().size();
   for(size_t i=0;i<control.npcActors().size();++i)if(control.npcActors()[i].homeLevel==57&&control.npcActors()[i].track().ai.squad==stateId("per_ops"))man=i;
   bool down=false;
   if(man<control.npcActors().size()){auto&npc=control.m_friendlyActors[man];const float floor=control.world().floorHeight(3.6f,12.3f);npc.pose.position={3.6f,12.3f};npc.pose.yaw=-kPi*.5f;npc.pose.z=floor;
    control.m_player.pos={4.55f,12.3f};control.m_player.z=floor;control.m_player.angle=kPi;control.restrainNpc(man,false);
    for(int i=0;i<180&&control.npcActors()[man].pose.clip!=4;++i)control.update({},1.f/60);
    down=control.npcIncapacitated(man)&&!control.npcHeld(man)&&control.npcActors()[man].pose.clip==4;}
   Vec2 front{std::sin(patch->yaw),std::cos(patch->yaw)};
   control.m_player.pos=patch->position+front*.8f;control.m_player.z=control.world().floorHeight(patch->position.x,patch->position.y)+patch->z;control.m_player.angle=std::atan2(-front.y,-front.x);
   auto hint=[&]{auto text=control.interactionHint();return std::string(text?text:"");};
   report<<"ops man down "<<down<<", body "<<control.nearbyBody()<<", terminal "<<control.nearbyTerminal()<<", hint "<<hint()<<'\n';
   bool crowded=down&&control.nearbyBody()==int(man)&&control.nearbyTerminal()>=0&&hint()=="E / PATCH CAMERAS TO HANDSET";
   InputState use{};use.use=true;control.updateInteraction(use,.01f);
   check(crowded&&control.state(CctvRadar)==1&&!control.draggingBody(),"Using the Perimeter Ops terminal makes the patch, with the Ops man choked at his post beside it");
   // Close the screen, turn to the body: now E drags him, and says so.
   control.updateInteraction({},.01f);control.updateInteraction(use,.01f);control.updateInteraction({},.01f);control.m_player.angle=0;
   bool offered=hint()=="E / DRAG BODY";control.updateInteraction(use,.01f);
   check(offered&&control.draggingBody()&&hint()=="E / DROP THE BODY","Facing away from the bench, E offers the body and drags it");}}
 report<<checks-failed<<'/'<<checks<<" checks passed\n";return failed==0;
}
}
