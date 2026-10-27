#include "../../Game.h"
#include "../../../world/CustomCampaign.h"
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
 // The door ripped open is the one line nothing may swallow, not even his own
 // "let's see what's in here" still in his mouth as the leaves swing.
 if(cue==NpcVoiceCue::Down||cue==NpcVoiceCue::Found)return 3;
 // Locker events are called once, as they happen: another guard's chatter on
 // the channel must not drop them, or the player hides on a lie of silence.
 if(cue==NpcVoiceCue::Hurt||cue==NpcVoiceCue::AllyDown||cue==NpcVoiceCue::Witness||cue==NpcVoiceCue::CheckLockers)return 2;
 if(cue==NpcVoiceCue::Patrol)return 0;
 return 1;
}
bool sharedTactical(NpcVoiceCue cue){return cue==NpcVoiceCue::Alert||cue==NpcVoiceCue::Investigate||cue==NpcVoiceCue::Retreat;}
bool channelHold(NpcVoiceCue cue){
 return sharedTactical(cue)||cue==NpcVoiceCue::AllyDown||cue==NpcVoiceCue::Treat||cue==NpcVoiceCue::Repair||
  cue==NpcVoiceCue::Support||cue==NpcVoiceCue::Deploy||cue==NpcVoiceCue::Follow||cue==NpcVoiceCue::Wait||cue==NpcVoiceCue::Danger||cue==NpcVoiceCue::GiveUp;
}
StateId cueCooldownId(NpcVoiceCue cue){
 if(cue==NpcVoiceCue::Alert)return stateId("npc_voice_cd_alert");
 if(cue==NpcVoiceCue::Investigate)return stateId("npc_voice_cd_investigate");
 if(cue==NpcVoiceCue::Retreat)return stateId("npc_voice_cd_retreat");
 if(cue==NpcVoiceCue::AllyDown)return stateId("npc_voice_cd_allydown");
 if(cue==NpcVoiceCue::Witness)return stateId("npc_voice_cd_witness");
 if(cue==NpcVoiceCue::CheckLockers)return stateId("npc_voice_cd_lockers");
 if(cue==NpcVoiceCue::Found)return stateId("npc_voice_cd_found");
 if(cue==NpcVoiceCue::GiveUp)return stateId("npc_voice_cd_giveup");
 return 0;
}
constexpr int AlertFloorMs=1500;
int cueCooldownMs(NpcVoiceCue cue){
 if(cue==NpcVoiceCue::Alert)return 10000; // per squad; the shared channel holds AlertFloorMs
 if(cue==NpcVoiceCue::Investigate)return 8000;
 if(cue==NpcVoiceCue::Retreat)return 12000;
 if(cue==NpcVoiceCue::AllyDown)return 9000;
 // Two guards watching one climb-in say it once; a squad opening a row of
 // lockers orders it once; one player is pulled out once.
 if(cue==NpcVoiceCue::Witness)return 4000;
 if(cue==NpcVoiceCue::CheckLockers)return 8000;
 if(cue==NpcVoiceCue::Found)return 3000;
 // Searches of one squad end a few seconds apart: one of them calls it off.
 if(cue==NpcVoiceCue::GiveUp)return 15000;
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
 if(cue==NpcVoiceCue::Alert&&get("voice_cd_alert")>0)return false;
 int choices[4],count=0;for(size_t i=0;i<NpcVoiceClips.size();++i)if(NpcVoiceClips[i].role==role&&NpcVoiceClips[i].cue==cue&&count<4)choices[count++]=int(i);
 if(!count)return false;
 int variant=std::max(0,get("voice_variant"));int clip=choices[variant%count];put("voice_variant",(variant+1)%10000);
 int duration=int(std::ceil(NpcVoiceClips[size_t(clip)].seconds*1000))+180;
 put("voice_lock",duration);put("voice_priority",priority);if(cue==NpcVoiceCue::Hurt)put("voice_hurt_wait",1800);
 setState(stateId("npc_voice_busy"),duration+350);
 // Contact is called once per squad (a man without one is his own) every ten
 // seconds; the shared floor only keeps two squads from calling over each other.
 if(StateId cooldown=cueCooldownId(cue))setState(cooldown,cue==NpcVoiceCue::Alert?AlertFloorMs:cueCooldownMs(cue));
 if(cue==NpcVoiceCue::Alert)for(size_t other=0;other<m_friendlyActors.size();++other)if(other==actor||(track.ai.squad&&m_friendlyActors[other].track().ai.squad==track.ai.squad))setState(friendlyKey(other,"voice_cd_alert"),cueCooldownMs(cue));
 // A single speaker cannot overlap its previous take, even within one frame.
 std::erase_if(m_sounds,[&](const auto&event){return event.speech>=0&&event.speaker==actor+1;});
 m_sounds.push_back({Sound::WorkerDying,position,.95f,1,true,22,npc.pose.z+track.scale*.85f,clip,actor+1,priority});return true;
}
void Game::updateNpcVoices(float dt){
 int ms=std::max(0,int(std::round(dt*1000)));setState(stateId("npc_voice_busy"),std::max(0,state("npc_voice_busy")-ms));
 for(StateId id:{stateId("npc_voice_cd_alert"),stateId("npc_voice_cd_investigate"),stateId("npc_voice_cd_retreat"),stateId("npc_voice_cd_allydown"),stateId("npc_voice_reply_ms"),
  stateId("npc_voice_cd_witness"),stateId("npc_voice_cd_lockers"),stateId("npc_voice_cd_found"),stateId("npc_voice_cd_giveup")})
  setState(id,std::max(0,state(id)-ms));
 for(size_t actor=0;actor<m_friendlyActors.size();++actor)if(int cd=state(friendlyKey(actor,"voice_cd_alert")))setState(friendlyKey(actor,"voice_cd_alert"),std::max(0,cd-ms));
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
   if(cue==NpcVoiceCue::Alert&&get("voice_cd_alert")>0)return;
   // His own squad called contact in the last ten seconds: he answers it.
   if(cue==NpcVoiceCue::Investigate&&get("voice_cd_alert")>0){
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
   // What this hunt was after, kept past the frame the mind forgets it: 1 a
   // glimpse of the player he never made out, 2 the player confirmed (seen,
   // reported by the squad or blamed for a body) - that search ends on Caution.
   if(mode!=int(GuardState::Patrol)&&get("guard_target_kind")==int(ShotKind::Player))put("voice_hunt",std::max(get("voice_hunt"),get("guard_confirmed")?2:1));
   if(mode!=previous){
    if(mode==int(GuardState::Alert))claimShared(NpcVoiceCue::Alert);
    else if(mode==int(GuardState::Investigate))claimShared(NpcVoiceCue::Investigate);
    else if(mode==int(GuardState::Search)){
     // Lost-contact search gets one Investigate bark. Investigate→Search is quiet —
     // they already announced the check.
     if(previous==int(GuardState::Alert)||previous==int(GuardState::Suspicious))claimShared(NpcVoiceCue::Investigate);
    }
    else if(mode==int(GuardState::Patrol)&&previous==int(GuardState::Search)&&get("voice_hunt")){
     // Calling off a search for the player: one voice per radio net, from a
     // guard the player can hear (a distant claim would gag a near squadmate).
     bool taken=state(stateId("npc_voice_cd_giveup"))>0||lengthSq(npc.pose.position+chunkOffset(npc.level)-chunkOffset(m_level)-m_player.pos)+std::pow(npc.pose.z-m_player.z,2)>22*22;
     for(size_t other=0;other<m_friendlyActors.size()&&!taken;++other)taken=other!=actor&&sameRadio(actor,other)&&pendingCue(other)==int(NpcVoiceCue::GiveUp);
     if(!taken){pending(NpcVoiceCue::GiveUp,5000);if(pendingCue(actor)==int(NpcVoiceCue::GiveUp))put("voice_giveup",get("voice_hunt"));}
    }
    put("voice_mode",mode);
   }
   if(mode==int(GuardState::Patrol))put("voice_hunt",0);
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
   // Back on the hunt before he got the words out: "lost him" would be a lie.
   if(request==int(NpcVoiceCue::GiveUp)&&mode!=int(GuardState::Patrol))put("voice_pending",0);
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
   else if(request<=int(NpcVoiceCue::GiveUp)){
    // Calling it off he says what he knew: take 0 shrugs off a doubt, take 1
    // admits a confirmed intruder got away (his squad is on Caution).
    if(cue==NpcVoiceCue::GiveUp)put("voice_variant",std::max(0,get("voice_giveup")-1));
    if(npcVoice(actor,cue)){put("voice_pending",0);put("voice_reply_wait",0);if(cue==NpcVoiceCue::GiveUp)put("voice_patrol",std::max(get("voice_patrol"),15000));}
   }
  }
 }
}

bool Game::testNpcVoices(){
 std::ofstream out("npc-voice-test.txt");bool pass=true;auto check=[&](bool ok,const char*label){out<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';pass&=ok;};
 Game game;game.loadLevel(14,false);game.updateStreaming(0);game.m_player.pos={13,19};game.m_player.z=-25;
 std::vector<size_t> detail;for(size_t i=0;i<game.npcActors().size();++i)if(game.npcActors()[i].homeLevel==14&&voiceRole(game.npcActors()[i].track()))detail.push_back(i);
 check(detail.size()==6,"Receiving uses six distinct skinned voice roles");
 for(auto actor:detail){game.m_sounds.clear();game.setState(stateId("npc_voice_busy"),0);game.setState(game.friendlyKey(actor,"voice_lock"),0);game.setState(stateId("npc_voice_cd_alert"),0);for(size_t i=0;i<game.npcActors().size();++i)game.setState(game.friendlyKey(i,"voice_cd_alert"),0);
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
 // Lockers. Every hostile voice has two takes of each locker cue, short enough
 // to land on the moment (GiveUp's two takes are chosen by what he knew).
 {bool covered=true;for(int role:{1,2,3,4,5,6,8,9})for(auto cue:{NpcVoiceCue::Witness,NpcVoiceCue::CheckLockers,NpcVoiceCue::Found,NpcVoiceCue::GiveUp}){int count=0;
   for(const auto&clip:NpcVoiceClips)if(clip.role==role&&clip.cue==cue){++count;covered&=clip.seconds>.5f&&clip.seconds<(cue==NpcVoiceCue::Found?2.8f:cue==NpcVoiceCue::GiveUp?4.8f:3.8f);}
   if(count!=2){covered=false;out<<"  role "<<role<<" cue "<<int(cue)<<" has "<<count<<" takes\n";}}
  check(covered,"Every guard voice has two takes of Witness, CheckLockers, Found and GiveUp of a sane length");}
 Game lockers;lockers.loadLevel(14,false);lockers.updateStreaming(0);lockers.m_player.pos={13,19};lockers.m_player.z=-25;
 std::vector<size_t> guards;for(size_t i=0;i<lockers.npcActors().size();++i)if(lockers.npcActors()[i].homeLevel==14&&voiceRole(lockers.npcActors()[i].track())&&lockers.npcActors()[i].track().ai.mode==ActorAiMode::Guard)guards.push_back(i);
 check(guards.size()>=3,"Receiving has a guard squad for the locker voice tests");
 if(guards.size()>=3){
  auto spoken=[&](NpcVoiceCue cue){int n=0;for(const auto&e:lockers.sounds())n+=e.speech>=0&&NpcVoiceClips[size_t(e.speech)].cue==cue;return n;};
  auto quiet=[&](){lockers.m_sounds.clear();lockers.setState(stateId("npc_voice_busy"),0);for(auto g:guards){lockers.setState(lockers.friendlyKey(g,"voice_lock"),0);lockers.setState(lockers.friendlyKey(g,"voice_pending"),0);lockers.setState(lockers.friendlyKey(g,"voice_patrol"),60000);}
   for(const char*cd:{"npc_voice_cd_alert","npc_voice_cd_investigate","npc_voice_cd_witness","npc_voice_cd_lockers","npc_voice_cd_found","npc_voice_cd_giveup","npc_voice_reply_ms"})lockers.setState(stateId(cd),0);};
  size_t a=guards[0],b=guards[1];quiet();
  check(lockers.npcVoice(a,NpcVoiceCue::Witness)&&!lockers.npcVoice(b,NpcVoiceCue::Witness)&&spoken(NpcVoiceCue::Witness)==1,"Two guards who watch one climb-in say it once");
  check(!lockers.npcVoice(a,NpcVoiceCue::Patrol),"Chatter cannot talk over the witness line");
  quiet();lockers.npcVoice(b,NpcVoiceCue::Investigate);
  check(lockers.npcVoice(a,NpcVoiceCue::CheckLockers),"A squadmate's bark on the channel does not swallow the order to check the lockers");
  check(lockers.npcVoice(a,NpcVoiceCue::Found)&&spoken(NpcVoiceCue::Found)==1&&spoken(NpcVoiceCue::CheckLockers)==0,"Ripping the door open cuts his own unfinished mutter, never overlaps it");
  check(!lockers.npcVoice(b,NpcVoiceCue::Found)&&!lockers.npcVoice(b,NpcVoiceCue::CheckLockers),"The pull and the order are said once per event, not echoed by the squad");
  // A search for the player ends: one voice calls it off, in the take that fits what he knew.
  auto hunt=[&](int kind,int confirmed,int busy=0){quiet();for(auto g:guards){lockers.setState(lockers.friendlyKey(g,"voice_initialized"),1);lockers.setState(lockers.friendlyKey(g,"voice_mode"),int(GuardState::Search));
    lockers.setState(lockers.friendlyKey(g,"guard_mode"),int(GuardState::Search));lockers.setState(lockers.friendlyKey(g,"guard_target_kind"),kind);lockers.setState(lockers.friendlyKey(g,"guard_confirmed"),confirmed);lockers.setState(lockers.friendlyKey(g,"voice_hunt"),0);}
   lockers.updateNpcVoices(.016f);lockers.setState(stateId("npc_voice_busy"),busy);for(auto g:guards){lockers.setState(lockers.friendlyKey(g,"guard_mode"),int(GuardState::Patrol));lockers.setState(lockers.friendlyKey(g,"guard_target_kind"),0);lockers.setState(lockers.friendlyKey(g,"guard_confirmed"),0);}
   lockers.updateNpcVoices(.016f);int claims=0;for(auto g:guards)claims+=lockers.state(lockers.friendlyKey(g,"voice_pending"))-1==int(NpcVoiceCue::GiveUp);return claims+spoken(NpcVoiceCue::GiveUp);};
  auto flush=[&](){for(int i=0;i<120;++i)lockers.updateNpcVoices(.05f);int take=-1;
   for(const auto&e:lockers.sounds())if(e.speech>=0&&NpcVoiceClips[size_t(e.speech)].cue==NpcVoiceCue::GiveUp){int role=NpcVoiceClips[size_t(e.speech)].role;take=0;for(int i=0;i<e.speech;++i)take+=NpcVoiceClips[size_t(i)].role==role&&NpcVoiceClips[size_t(i)].cue==NpcVoiceCue::GiveUp;}
   return take;};
  check(hunt(int(ShotKind::Player),1)==1,"A squad ending its search for the player elects one GiveUp, not a roll call");
  int take=flush();check(take==1&&spoken(NpcVoiceCue::GiveUp)==1,"After a confirmed intruder he says he lost him (the squad is on Caution), once");
  for(auto g:guards){lockers.setState(lockers.friendlyKey(g,"guard_mode"),int(GuardState::Search));lockers.setState(lockers.friendlyKey(g,"guard_target_kind"),int(ShotKind::Player));}
  lockers.updateNpcVoices(.016f);for(auto g:guards)lockers.setState(lockers.friendlyKey(g,"guard_mode"),int(GuardState::Patrol));lockers.m_sounds.clear();lockers.updateNpcVoices(.016f);
  int late=0;for(auto g:guards)late+=lockers.state(lockers.friendlyKey(g,"voice_pending"))-1==int(NpcVoiceCue::GiveUp);
  check(late==0,"A squadmate whose search ends seconds later stays quiet");
  check(hunt(int(ShotKind::Player),0)==1&&flush()==0,"A glimpse he never made out is shrugged off with the doubtful take");
  check(hunt(int(ShotKind::Enemy),1)==0&&flush()<0,"A search for a creature does not end on a line about the player");
  // A fresh glimpse (Suspicious claims no bark of its own) before the words are out.
  check(hunt(int(ShotKind::Player),1,5000)==1&&spoken(NpcVoiceCue::GiveUp)==0,"With the channel busy the GiveUp claim waits its turn");for(auto g:guards)lockers.setState(lockers.friendlyKey(g,"guard_mode"),int(GuardState::Suspicious));
  lockers.updateNpcVoices(.016f);lockers.setState(stateId("npc_voice_busy"),0);lockers.updateNpcVoices(.016f);
  int stale=0;for(auto g:guards)stale+=lockers.state(lockers.friendlyKey(g,"voice_pending"))-1==int(NpcVoiceCue::GiveUp);
  check(stale==0&&spoken(NpcVoiceCue::GiveUp)==0,"Back on the hunt before he spoke, he does not say he lost him");
  lockers.setState(stateId("npc_voice_cd_giveup"),7000);lockers.setState(lockers.friendlyKey(a,"voice_hunt"),2);
  Game restored;check(restored.decodeSave(lockers.encodeSave())&&restored.state(stateId("npc_voice_cd_giveup"))==7000&&restored.state(restored.friendlyKey(a,"voice_hunt"))==2,"A search's quarry and the GiveUp cooldown survive save/load");
 }
 // End to end through the real guard mind (the Caution room): he makes the
 // player out, loses him behind the wall, searches and gives up. One of the
 // pair says so, once, as he turns back to patrol, in the "lost him" take.
 {auto map=std::make_shared<AuthoredMapData>();map->name="Voice room";map->definition={{0,0},{18.5f,18.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);map->structures.push_back({12,14,24,14.2f,0,8});
  TimedSequence timer;timer.timerState=stateId("voice_room_clock");timer.finishState=stateId("voice_room_done");timer.soundIntervalMs=0;map->timedSequences.push_back(timer);
  for(int i=0;i<2;++i){Vec2 at=i?Vec2{2.5f,20.5f}:Vec2{6.5f,6.5f};float yaw=i?-kPi*.5f:kPi*.5f;ActorTrack t;t.timerState=timer.timerState;t.keys={{0,at,0,yaw},{30000,at,0,yaw}};t.scale=1.6f;t.health=70;t.voice=1;
   t.damageState=stateId("voice_room_damage_"+std::to_string(i));t.deadState=stateId("voice_room_dead_"+std::to_string(i));t.ai.mode=ActorAiMode::Guard;t.ai.alertState=stateId("voice_room_alert");t.ai.sightRange=11;
   t.ai.squad=stateId("voice_room_squad");t.ai.patrol={{at,0,1000}};map->actorTracks.push_back(t);}
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Voice validation";campaign->key=731906;campaign->maps.push_back(map);Game g(campaign);
  g.m_player.pos={8.5f,6.5f};int seen=0;while(seen<600&&!g.state(g.friendlyKey(0,"guard_confirmed"))){g.update({},1.f/60);++seen;}
  g.m_player.pos={18.5f,18.5f};int giveUps=0,take=-1,spokenAt=-1,gaveUpAt=-1;
  for(int i=0;i<60*40;++i){g.m_sounds.clear();g.update({},1.f/60);if(gaveUpAt<0&&g.guardCaution(0))gaveUpAt=i;
   for(const auto&e:g.sounds())if(e.speech>=0&&NpcVoiceClips[size_t(e.speech)].cue==NpcVoiceCue::GiveUp){++giveUps;if(spokenAt<0)spokenAt=i;take=0;for(int c=0;c<e.speech;++c)take+=NpcVoiceClips[size_t(c)].role==1&&NpcVoiceClips[size_t(c)].cue==NpcVoiceCue::GiveUp;}}
  out<<"  confirmed after "<<seen<<" frames, search given up at "<<gaveUpAt<<", GiveUp spoken at "<<spokenAt<<" ("<<giveUps<<" times, take "<<take<<")"<<'\n';
  check(seen<600&&gaveUpAt>0&&giveUps==1&&take==1&&spokenAt>=gaveUpAt&&spokenAt-gaveUpAt<45,"Through the real guard mind a lost player is called off once, as the search ends, in the take that admits it");}
 // Contact per squad: a second squad's "!" is heard 1.5 s after the first; the
 // first squad's own second call waits out its ten seconds.
 {auto map=std::make_shared<AuthoredMapData>();map->name="Two squads";map->definition={{0,0},{12.5f,12.5f},0,Environment::Interior,false,8,.3f,42};
  AuthoredLayerData layer;for(auto&row:layer.rows)row=std::string(24,'.');map->layers.push_back(layer);
  TimedSequence timer;timer.timerState=stateId("squads_clock");timer.finishState=stateId("squads_done");timer.soundIntervalMs=0;map->timedSequences.push_back(timer);
  for(int i=0;i<4;++i){Vec2 at{6.5f+i*2.f,8.5f};ActorTrack t;t.timerState=timer.timerState;t.keys={{0,at,0,0},{30000,at,0,0}};t.scale=1.6f;t.health=70;t.voice=1+i%2;
   t.damageState=stateId("squads_damage_"+std::to_string(i));t.deadState=stateId("squads_dead_"+std::to_string(i));t.ai.mode=ActorAiMode::Guard;t.ai.patrol={{at,0,1000}};
   if(i<3)t.ai.squad=stateId(i<2?"squads_a":"squads_b");map->actorTracks.push_back(t);}
  auto campaign=std::make_shared<CustomCampaign>();campaign->name="Squad voice validation";campaign->key=731907;campaign->maps.push_back(map);Game g(campaign);g.m_player.pos={12.5f,12.5f};
  auto call=[&](size_t a){g.setState(stateId("npc_voice_busy"),0);g.setState(g.friendlyKey(a,"voice_lock"),0);return g.npcVoice(a,NpcVoiceCue::Alert);};
  auto wait=[&](float seconds){for(int i=0;i<int(seconds*20);++i){g.m_sounds.clear();g.updateNpcVoices(.05f);}};
  const bool first=call(0),floor=!call(2);wait(1.6f);const bool other=call(2);wait(1.6f);const bool own=!call(1),lone=call(3);wait(7.f);const bool later=call(1);
  out<<"  squads: first "<<first<<" floor "<<floor<<" other "<<other<<" own "<<own<<" lone "<<lone<<" later "<<later<<'\n';
  check(first&&floor&&other&&own&&lone&&later,"Contact is called per squad: another squad's (or a lone guard's) is heard 1.5 s later, the same squad's waits ten seconds");}
 return pass;
}
}
