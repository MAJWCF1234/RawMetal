#include "../../Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace retro {
namespace {
int voiceRole(const ActorTrack& track){
 if(track.voice>0)return track.voice;
 switch(track.visual){
  case ActorVisual::Cleanup:return 1;
  case ActorVisual::CleanupEngineer:return 2;
  case ActorVisual::CleanupSharpshooter:return 3;
  case ActorVisual::CleanupHeavy:return 4;
  case ActorVisual::CleanupMedic:return 5;
  case ActorVisual::CleanupRusher:return 6;
  case ActorVisual::Worker:case ActorVisual::Scientist:case ActorVisual::SeniorScientist:case ActorVisual::LabTech:
   return track.ai.mode==ActorAiMode::Escort?7:0;
  // The escort bank is a man's voice: a woman stays silent unless authored one.
  case ActorVisual::FemaleScientist:case ActorVisual::FemaleDoctor:return 0;
  default:return 0;
 }
}
int voicePriority(NpcVoiceCue cue){
 if(cue==NpcVoiceCue::Down)return 3;
 if(cue==NpcVoiceCue::Hurt||cue==NpcVoiceCue::AllyDown)return 2;
 if(cue==NpcVoiceCue::Patrol)return 0;
 return 1;
}
bool sharedTactical(NpcVoiceCue cue){return cue==NpcVoiceCue::Alert||cue==NpcVoiceCue::Investigate||cue==NpcVoiceCue::Retreat;}
bool channelHold(NpcVoiceCue cue){
 return sharedTactical(cue)||cue==NpcVoiceCue::AllyDown||cue==NpcVoiceCue::Treat||cue==NpcVoiceCue::Repair||
  cue==NpcVoiceCue::Support||cue==NpcVoiceCue::Deploy||cue==NpcVoiceCue::Follow||cue==NpcVoiceCue::Wait||cue==NpcVoiceCue::Danger;
}
StateId cueCooldownId(NpcVoiceCue cue){
 if(cue==NpcVoiceCue::Alert)return stateId("npc_voice_cd_alert");
 if(cue==NpcVoiceCue::Investigate)return stateId("npc_voice_cd_investigate");
 if(cue==NpcVoiceCue::Retreat)return stateId("npc_voice_cd_retreat");
 if(cue==NpcVoiceCue::AllyDown)return stateId("npc_voice_cd_allydown");
 return 0;
}
int cueCooldownMs(NpcVoiceCue cue){
 if(cue==NpcVoiceCue::Alert)return 10000;
 if(cue==NpcVoiceCue::Investigate)return 8000;
 if(cue==NpcVoiceCue::Retreat)return 12000;
 if(cue==NpcVoiceCue::AllyDown)return 9000;
 return 0;
}
}

bool Game::npcHasVoice(size_t actor)const{return actor<m_friendlyActors.size()&&voiceRole(m_friendlyActors[actor].track())!=0;}
bool Game::npcVoice(size_t actor,NpcVoiceCue cue){
 if(actor>=m_friendlyActors.size())return false;const auto&npc=m_friendlyActors[actor];const auto&track=npc.track();
 int role=voiceRole(track);if(!role||!autonomousActor(track)||!chunkResident(npc.level)||state(track.deadState)||npcIncapacitated(actor))return false;
 Vec2 position=npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level);
 if(lengthSq(position-m_player.pos)+std::pow(npc.pose.z-m_player.z,2)>22*22)return false;
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
 int priority=voicePriority(cue);
 if(cue!=NpcVoiceCue::Down&&((get("voice_lock")&&get("voice_priority")>=priority)||(priority<2&&state("npc_voice_busy"))||(cue==NpcVoiceCue::Hurt&&get("voice_hurt_wait"))))return false;
 // AllyDown reserves its cooldown at claim time so multi-kills cannot queue
 // several reports; the elected speaker must still be allowed to flush.
 if(StateId cooldown=cueCooldownId(cue);cooldown&&state(cooldown)>0&&cue!=NpcVoiceCue::Hurt&&cue!=NpcVoiceCue::Down&&cue!=NpcVoiceCue::AllyDown)return false;
 int choices[4],count=0;for(size_t i=0;i<NpcVoiceClips.size();++i)if(NpcVoiceClips[i].role==role&&NpcVoiceClips[i].cue==cue&&count<4)choices[count++]=int(i);
 if(!count)return false;
 int variant=std::max(0,get("voice_variant"));int clip=choices[variant%count];put("voice_variant",(variant+1)%10000);
 int duration=int(std::ceil(NpcVoiceClips[size_t(clip)].seconds*1000))+180;
 put("voice_lock",duration);put("voice_priority",priority);if(cue==NpcVoiceCue::Hurt)put("voice_hurt_wait",1800);
 setState(stateId("npc_voice_busy"),duration+350);
 if(StateId cooldown=cueCooldownId(cue))setState(cooldown,cueCooldownMs(cue));
 // A single speaker cannot overlap its previous take, even within one frame.
 std::erase_if(m_sounds,[&](const auto&event){return event.speech>=0&&event.speaker==actor+1;});
 m_sounds.push_back({Sound::WorkerDying,position,.95f,1,true,22,npc.pose.z+track.scale*.85f,clip,actor+1,priority});return true;
}
void Game::updateNpcVoices(float dt){
 int ms=std::max(0,int(std::round(dt*1000)));setState(stateId("npc_voice_busy"),std::max(0,state("npc_voice_busy")-ms));
 for(StateId id:{stateId("npc_voice_cd_alert"),stateId("npc_voice_cd_investigate"),stateId("npc_voice_cd_retreat"),stateId("npc_voice_cd_allydown"),stateId("npc_voice_reply_ms")})
  setState(id,std::max(0,state(id)-ms));
 auto sameRadio=[&](size_t a,size_t b){
  const auto&left=m_friendlyActors[a].track(),&right=m_friendlyActors[b].track();
  if(left.ai.squad&&left.ai.squad==right.ai.squad)return true;
  return left.ai.mode==ActorAiMode::Guard&&right.ai.mode==ActorAiMode::Guard&&left.ai.faction==right.ai.faction;
 };
 auto pendingCue=[&](size_t actor){return state(friendlyKey(actor,"voice_pending"))-1;};
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];const auto&track=npc.track();
  if(!voiceRole(track)||!autonomousActor(track)||!chunkResident(npc.level))continue;
  auto get=[&](const char*field){return state(friendlyKey(actor,field));};auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
  for(const char*field:{"voice_lock","voice_hurt_wait","voice_patrol","voice_pending_ms","voice_reply_wait"})put(field,std::max(0,get(field)-ms));
  if(state(track.deadState)||npcIncapacitated(actor)){put("voice_pending",0);put("voice_reply_wait",0);continue;}
  auto pending=[&](NpcVoiceCue cue,int holdMs=4000){int old=get("voice_pending")-1;if(old<0||voicePriority(cue)>=voicePriority(NpcVoiceCue(old))){put("voice_pending",int(cue)+1);put("voice_pending_ms",holdMs);put("voice_reply_wait",0);}};
  // Shared tactical calls elect one radio voice. Everyone else stays quiet so a
  // squad contact sounds like chatter, not a roll call down the actor list.
  auto claimShared=[&](NpcVoiceCue cue){
   if(StateId cooldown=cueCooldownId(cue);cooldown&&state(cooldown)>0)return;
   if(cue==NpcVoiceCue::Investigate&&state(stateId("npc_voice_cd_alert"))>0){
    // At most one delayed "copy" after someone already called contact.
    if(state(stateId("npc_voice_reply_ms"))>0)return;
    for(size_t other=0;other<m_friendlyActors.size();++other)if(other!=actor&&sameRadio(actor,other)&&pendingCue(other)==int(NpcVoiceCue::Investigate))return;
    int wait=1100+int(actor*419%1100);pending(NpcVoiceCue::Investigate,4500);put("voice_reply_wait",wait);setState(stateId("npc_voice_reply_ms"),wait+2800);return;
   }
   bool spotter=cue==NpcVoiceCue::Alert&&(get("guard_seen")||guardState(actor)==GuardState::Alert);
   for(size_t other=0;other<m_friendlyActors.size();++other){
    if(other==actor||!sameRadio(actor,other))continue;int claim=pendingCue(other);
    if(claim<0||!sharedTactical(NpcVoiceCue(claim)))continue;
    if(cue==NpcVoiceCue::Alert&&claim==int(NpcVoiceCue::Alert)){
     if(!spotter||state(friendlyKey(other,"guard_seen")))return;
     setState(friendlyKey(other,"voice_pending"),0);setState(friendlyKey(other,"voice_reply_wait"),0);continue;
    }
    if(cue==NpcVoiceCue::Alert&&claim==int(NpcVoiceCue::Investigate)){
     if(state(stateId("npc_voice_reply_ms"))<=0){int wait=1100+int(other*419%1100);setState(friendlyKey(other,"voice_reply_wait"),wait);setState(stateId("npc_voice_reply_ms"),wait+2800);}
     else{setState(friendlyKey(other,"voice_pending"),0);setState(friendlyKey(other,"voice_reply_wait"),0);}
     continue;
    }
    if(claim==int(cue))return;
    if(cue==NpcVoiceCue::Investigate&&claim==int(NpcVoiceCue::Alert)){
     if(state(stateId("npc_voice_reply_ms"))>0)return;
     int wait=1100+int(actor*419%1100);pending(NpcVoiceCue::Investigate,4500);put("voice_reply_wait",wait);setState(stateId("npc_voice_reply_ms"),wait+2800);return;
    }
   }
   pending(cue,cue==NpcVoiceCue::Alert?2800:3600);
  };
  if(!get("voice_initialized")){put("voice_initialized",1);put("voice_patrol",12000+int(actor*1703%13000));
   put("voice_mode",int(guardState(actor)));put("voice_hold",get("hold"));put("voice_engaged",get("engaged"));
   // Seed current mode quietly. Spawning already-alerted squads must not roll-call.
   if(track.ai.mode==ActorAiMode::Escort&&get("engaged"))pending(get("hold")?NpcVoiceCue::Wait:NpcVoiceCue::Follow);
  }
  if(track.ai.mode==ActorAiMode::Guard){
   int mode=int(guardState(actor));int previous=get("voice_mode");
   if(mode!=previous){
    if(mode==int(GuardState::Alert))claimShared(NpcVoiceCue::Alert);
    else if(mode==int(GuardState::Investigate))claimShared(NpcVoiceCue::Investigate);
    else if(mode==int(GuardState::Search)){
     // Lost-contact search gets one Investigate bark. Investigate→Search is quiet —
     // they already announced the check.
     if(previous==int(GuardState::Alert)||previous==int(GuardState::Suspicious))claimShared(NpcVoiceCue::Investigate);
    }
    put("voice_mode",mode);
   }
   bool retreat=get("evac_level")!=0;if(retreat&&!get("voice_retreat"))claimShared(NpcVoiceCue::Retreat);put("voice_retreat",retreat);
   bool treating=get("support_ms")>0&&get("support_target")>0;
   if(treating&&!get("voice_treating"))pending(NpcVoiceCue::Treat);put("voice_treating",treating);
   int repairSlot=get("drone_repair_target")-1;bool repairing=false;
   if(repairSlot>=0)for(const auto&drone:engineerDrones())if(drone.slot==repairSlot&&drone.owner==int(actor)&&drone.repair>0){repairing=true;break;}
   if(repairing&&!get("voice_repairing"))pending(NpcVoiceCue::Repair);put("voice_repairing",repairing);
   bool pad=get("engineer_pad_stage")>0;if(pad&&!get("voice_pad"))pending(NpcVoiceCue::Support);put("voice_pad",pad);
   // Cancel stale work claims if the underlying task was interrupted.
   int request=get("voice_pending")-1;
   if((request==int(NpcVoiceCue::Treat)&&!treating)||(request==int(NpcVoiceCue::Repair)&&!repairing)||(request==int(NpcVoiceCue::Support)&&!pad))put("voice_pending",0);
   if(request==int(NpcVoiceCue::Deploy)){bool live=false;for(const auto&drone:engineerDrones())if(drone.owner==int(actor)&&drone.slot==get("voice_drone")-1){live=true;break;}if(!live)put("voice_pending",0);}
   if(!retreat&&mode==int(GuardState::Patrol)&&!get("walking")&&!get("voice_patrol")&&!get("voice_pending")){
    if(npcVoice(actor,NpcVoiceCue::Patrol))put("voice_patrol",22000+int(actor*2903%12000));
   }
  }else if(track.ai.mode==ActorAiMode::Escort){
   bool engaged=get("engaged")!=0,hold=get("hold")!=0;
   if(engaged&&(!get("voice_engaged")||hold!=(get("voice_hold")!=0)))pending(hold?NpcVoiceCue::Wait:NpcVoiceCue::Follow);
   if(get("fear")>0&&!get("voice_fear"))pending(NpcVoiceCue::Danger);
   put("voice_fear",get("fear")>0);put("voice_hold",hold);put("voice_engaged",engaged);
  }
  int request=get("voice_pending")-1;if(request>=0){
   auto cue=NpcVoiceCue(request);
   // Keep real claims alive while the global channel is busy so a delayed copy
   // or AllyDown does not expire mid-sentence from someone else.
   if(channelHold(cue)&&state("npc_voice_busy")&&!get("voice_reply_wait")&&get("voice_pending_ms")<1800)put("voice_pending_ms",1800);
   if(!get("voice_pending_ms")){put("voice_pending",0);put("voice_reply_wait",0);}
   else if(get("voice_reply_wait")>0){}
   else if(request<=int(NpcVoiceCue::Danger)&&npcVoice(actor,cue)){put("voice_pending",0);put("voice_reply_wait",0);}
  }
 }
}

bool Game::testNpcVoices(){
 std::ofstream out("npc-voice-test.txt");bool pass=true;auto check=[&](bool ok,const char*label){out<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';pass&=ok;};
 Game game;game.loadLevel(14,false);game.updateStreaming(0);game.m_player.pos={13,19};game.m_player.z=-25;
 std::vector<size_t> detail;for(size_t i=0;i<game.npcActors().size();++i)if(game.npcActors()[i].homeLevel==14&&voiceRole(game.npcActors()[i].track()))detail.push_back(i);
 check(detail.size()==6,"Receiving uses six distinct skinned voice roles");
 for(auto actor:detail){game.m_sounds.clear();game.setState(stateId("npc_voice_busy"),0);game.setState(game.friendlyKey(actor,"voice_lock"),0);game.setState(stateId("npc_voice_cd_alert"),0);
  bool spoke=game.npcVoice(actor,NpcVoiceCue::Alert);int role=voiceRole(game.npcActors()[actor].track());
  check(spoke&&!game.sounds().empty()&&NpcVoiceClips[size_t(game.sounds().back().speech)].role==role,"Alert selects the actor's actual skinned voice");
  check(!game.npcVoice(actor,NpcVoiceCue::Patrol),"Patrol cannot interrupt an active tactical call");
  check(game.npcVoice(actor,NpcVoiceCue::Hurt)&&game.sounds().size()==1,"Injury replaces the same speaker instead of overlapping");
  check(!game.npcVoice(actor,NpcVoiceCue::Hurt),"Rapid damage cannot spam injury takes");
 }
 if(!detail.empty()){size_t actor=detail.front();game.m_sounds.clear();game.damageFriendly(actor,1000,game.player().pos,{ShotKind::Player,14,0});
  bool dying=std::any_of(game.sounds().begin(),game.sounds().end(),[](const auto&e){return e.speech>=0&&NpcVoiceClips[size_t(e.speech)].cue==NpcVoiceCue::Down;});
  check(dying,"A lethal hit emits the dying line before the actor becomes dead");check(!game.npcVoice(actor,NpcVoiceCue::Alert),"Dead speakers cannot emit tactical dialogue");
 }
 Game saved;check(saved.decodeSave(game.encodeSave()),"Voice cooldown and event history survive save/load");
 if(!detail.empty())check(saved.state(saved.friendlyKey(detail.back(),"voice_variant"))==game.state(game.friendlyKey(detail.back(),"voice_variant")),"Reload preserves variant selection");
 if(detail.size()>1){auto actor=detail[1];game.m_sounds.clear();game.setState(stateId("npc_voice_busy"),0);game.setState(game.friendlyKey(actor,"voice_lock"),0);game.setState(stateId("npc_voice_cd_investigate"),0);
  check(game.npcVoice(actor,NpcVoiceCue::Investigate)&&game.aiStimuli().empty(),"Spoken investigation does not recursively alert the AI hearing system");
  game.setState(stateId("npc_voice_busy"),0);game.setState(game.friendlyKey(actor,"voice_lock"),0);game.m_player.pos={100,100};check(!game.npcVoice(actor,NpcVoiceCue::Patrol),"Distant actors do not reserve the local dialogue channel");
 }
 if(detail.size()>2){
  Game radio;radio.loadLevel(14,false);radio.updateStreaming(0);radio.m_player.pos={13,19};radio.m_player.z=-25;
  std::vector<size_t> squad;for(size_t i=0;i<radio.npcActors().size();++i)if(radio.npcActors()[i].homeLevel==14&&voiceRole(radio.npcActors()[i].track())&&radio.npcActors()[i].track().ai.mode==ActorAiMode::Guard)squad.push_back(i);
  check(squad.size()>=3,"Receiving has a multi-guard radio net for chatter tests");
  if(squad.size()>=3){
   for(auto actor:squad){radio.setState(radio.friendlyKey(actor,"voice_initialized"),1);radio.setState(radio.friendlyKey(actor,"voice_mode"),int(GuardState::Patrol));radio.setState(radio.friendlyKey(actor,"voice_pending"),0);radio.setState(radio.friendlyKey(actor,"guard_mode"),int(GuardState::Investigate));}
   radio.setState(radio.friendlyKey(squad[0],"guard_mode"),int(GuardState::Alert));radio.setState(radio.friendlyKey(squad[0],"guard_seen"),1);
   radio.setState(stateId("npc_voice_busy"),0);radio.setState(stateId("npc_voice_cd_alert"),0);radio.setState(stateId("npc_voice_cd_investigate"),0);radio.setState(stateId("npc_voice_reply_ms"),0);
   radio.m_sounds.clear();radio.updateNpcVoices(.016f);
   int pendingAlert=0,pendingInvestigate=0,spokenAlert=0;for(auto actor:squad){int cue=radio.state(radio.friendlyKey(actor,"voice_pending"))-1;pendingAlert+=cue==int(NpcVoiceCue::Alert);pendingInvestigate+=cue==int(NpcVoiceCue::Investigate);}
   for(const auto&event:radio.sounds())if(event.speech>=0&&NpcVoiceClips[size_t(event.speech)].cue==NpcVoiceCue::Alert)++spokenAlert;
   check(pendingAlert+spokenAlert==1,"Only one squadmate claims the contact call");
   check(pendingInvestigate<=1,"At most one delayed copy answers an investigation fan-out");
   for(int i=0;i<500&&(pendingAlert||pendingInvestigate||radio.state(stateId("npc_voice_busy")));++i){radio.updateNpcVoices(.05f);
    pendingAlert=0;pendingInvestigate=0;for(auto actor:squad){int cue=radio.state(radio.friendlyKey(actor,"voice_pending"))-1;pendingAlert+=cue==int(NpcVoiceCue::Alert);pendingInvestigate+=cue==int(NpcVoiceCue::Investigate);}
   }
   int spoken=0;for(const auto&event:radio.sounds())if(event.speech>=0)++spoken;
   check(spoken>=1&&spoken<=2,"Squad contact resolves to a call and optional reply, not a roll call");

   // Lost-contact Search announces once; Investigate→Search stays quiet.
   Game search;search.loadLevel(14,false);search.updateStreaming(0);search.m_player.pos={13,19};search.m_player.z=-25;
   for(auto actor:squad){search.setState(search.friendlyKey(actor,"voice_initialized"),1);search.setState(search.friendlyKey(actor,"voice_mode"),int(GuardState::Alert));search.setState(search.friendlyKey(actor,"voice_pending"),0);search.setState(search.friendlyKey(actor,"guard_mode"),int(GuardState::Alert));}
   search.setState(stateId("npc_voice_busy"),0);search.setState(stateId("npc_voice_cd_alert"),0);search.setState(stateId("npc_voice_cd_investigate"),0);search.setState(stateId("npc_voice_reply_ms"),0);
   for(auto actor:squad)search.setState(search.friendlyKey(actor,"guard_mode"),int(GuardState::Search));
   search.m_sounds.clear();search.updateNpcVoices(.016f);
   int searchClaims=0;for(auto actor:squad)searchClaims+=search.state(search.friendlyKey(actor,"voice_pending"))-1==int(NpcVoiceCue::Investigate);
   for(const auto&event:search.sounds())if(event.speech>=0&&NpcVoiceClips[size_t(event.speech)].cue==NpcVoiceCue::Investigate)++searchClaims;
   check(searchClaims==1,"Lost-contact Search elects one Investigate bark");
   for(auto actor:squad){search.setState(search.friendlyKey(actor,"voice_mode"),int(GuardState::Investigate));search.setState(search.friendlyKey(actor,"voice_pending"),0);search.setState(search.friendlyKey(actor,"guard_mode"),int(GuardState::Investigate));}
   search.setState(stateId("npc_voice_busy"),0);search.setState(stateId("npc_voice_cd_investigate"),0);search.setState(stateId("npc_voice_reply_ms"),0);
   for(auto actor:squad)search.setState(search.friendlyKey(actor,"guard_mode"),int(GuardState::Search));
   search.m_sounds.clear();search.updateNpcVoices(.016f);
   int quiet=0;for(auto actor:squad)quiet+=search.state(search.friendlyKey(actor,"voice_pending"))!=0;quiet+=int(search.sounds().size());
   check(quiet==0,"Investigate-to-Search does not spam a second check call");

   // AllyDown: one speaker, cooldown blocks a second casualty bark.
   Game fallen;fallen.loadLevel(14,false);fallen.updateStreaming(0);fallen.m_player.pos={13,19};fallen.m_player.z=-25;
   auto living=[&](size_t actor){return actor<fallen.npcActors().size()&&!fallen.state(fallen.npcActors()[actor].track().deadState)&&!fallen.npcIncapacitated(actor);};
   size_t victim=squad[0];
   fallen.setState(stateId("npc_voice_cd_allydown"),0);fallen.setState(stateId("npc_voice_busy"),0);
   for(auto actor:squad)fallen.setState(fallen.friendlyKey(actor,"voice_pending"),0);
   fallen.damageFriendly(victim,1000,fallen.player().pos,{ShotKind::Player,14,0});
   size_t claimed=SIZE_MAX;int allyPending=0;
   for(auto actor:squad)if(actor!=victim&&fallen.state(fallen.friendlyKey(actor,"voice_pending"))-1==int(NpcVoiceCue::AllyDown)){claimed=actor;++allyPending;}
   check(claimed!=SIZE_MAX,"AllyDown test has a living squad reporter");
   check(allyPending==1,"Only one squadmate claims AllyDown");
   if(claimed!=SIZE_MAX){
    check(fallen.state(stateId("npc_voice_cd_allydown"))>0,"AllyDown claim arms the squad cooldown immediately");
    fallen.setState(stateId("npc_voice_busy"),0);fallen.setState(fallen.friendlyKey(claimed,"voice_lock"),0);fallen.setState(fallen.friendlyKey(claimed,"voice_hurt_wait"),0);
    fallen.m_sounds.clear();
    for(int i=0;i<40&&fallen.state(fallen.friendlyKey(claimed,"voice_pending"));++i)fallen.updateNpcVoices(.05f);
    bool allySpoken=std::any_of(fallen.sounds().begin(),fallen.sounds().end(),[](const auto&e){return e.speech>=0&&NpcVoiceClips[size_t(e.speech)].cue==NpcVoiceCue::AllyDown;});
    check(allySpoken,"AllyDown claim flushes into a spoken casualty line");
    size_t second=SIZE_MAX;for(auto actor:squad)if(actor!=victim&&actor!=claimed&&living(actor)){second=actor;break;}
    if(second!=SIZE_MAX){
     for(auto actor:squad)fallen.setState(fallen.friendlyKey(actor,"voice_pending"),0);
     fallen.damageFriendly(second,1000,fallen.player().pos,{ShotKind::Player,14,0});
     int blocked=0;for(auto actor:squad)blocked+=fallen.state(fallen.friendlyKey(actor,"voice_pending"))-1==int(NpcVoiceCue::AllyDown);
     check(blocked==0,"AllyDown cooldown blocks a second casualty call");
    }
   }
  }
 }
 {auto escort=exclusionInspection(0);size_t witness=0;while(witness<escort.npcActors().size()&&(escort.npcActors()[witness].track().ai.mode!=ActorAiMode::Escort||!voiceRole(escort.npcActors()[witness].track())))++witness;
  check(witness<escort.npcActors().size(),"The zone's camp guide has the escort voice");
  if(witness<escort.npcActors().size()){const auto&npc=escort.npcActors()[witness];escort.m_player.pos=npc.pose.position;escort.m_player.z=npc.pose.z;escort.setState(escort.friendlyKey(witness,"engaged"),0);escort.updateNpcVoices(0);
   auto expect=[&](NpcVoiceCue cue){escort.m_sounds.clear();escort.setState(stateId("npc_voice_busy"),0);escort.setState(escort.friendlyKey(witness,"voice_lock"),0);escort.updateNpcVoices(.016f);
    return std::any_of(escort.sounds().begin(),escort.sounds().end(),[&](const auto&e){return e.speaker==witness+1&&e.speech>=0&&NpcVoiceClips[size_t(e.speech)].cue==cue;});};
   escort.setState(escort.friendlyKey(witness,"engaged"),1);check(expect(NpcVoiceCue::Follow),"Witness acknowledges the actual follow state transition");
   escort.setState(escort.friendlyKey(witness,"hold"),1);check(expect(NpcVoiceCue::Wait),"Witness acknowledges the actual wait state transition");
   escort.setState(escort.friendlyKey(witness,"fear"),4000);check(expect(NpcVoiceCue::Danger),"The guide warns of danger in his own voice");
  }
 }
 return pass;
}
}
