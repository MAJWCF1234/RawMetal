#include "../../Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>

namespace retro {
namespace {
int voiceRole(const ActorTrack& track){
 switch(track.visual){
  case ActorVisual::Cleanup:return 1;
  case ActorVisual::CleanupEngineer:return 2;
  case ActorVisual::CleanupSharpshooter:return 3;
  case ActorVisual::CleanupHeavy:return 4;
  case ActorVisual::CleanupMedic:return 5;
  case ActorVisual::CleanupRusher:return 6;
  case ActorVisual::Worker:return track.ai.mode==ActorAiMode::Escort?7:0;
  default:return 0;
 }
}
int voicePriority(NpcVoiceCue cue){return cue==NpcVoiceCue::Down?3:cue==NpcVoiceCue::Hurt?2:cue==NpcVoiceCue::Patrol?0:1;}
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
 int choices[4],count=0;for(size_t i=0;i<NpcVoiceClips.size();++i)if(NpcVoiceClips[i].role==role&&NpcVoiceClips[i].cue==cue&&count<4)choices[count++]=int(i);
 if(!count)return false;
 int variant=std::max(0,get("voice_variant"));int clip=choices[variant%count];put("voice_variant",(variant+1)%10000);
 int duration=int(std::ceil(NpcVoiceClips[size_t(clip)].seconds*1000))+180;
 put("voice_lock",duration);put("voice_priority",priority);if(cue==NpcVoiceCue::Hurt)put("voice_hurt_wait",1800);
 setState(stateId("npc_voice_busy"),duration+350);
 // A single speaker cannot overlap its previous take, even within one frame.
 std::erase_if(m_sounds,[&](const auto&event){return event.speech>=0&&event.speaker==actor+1;});
 m_sounds.push_back({Sound::WorkerDying,position,.95f,1,true,22,npc.pose.z+track.scale*.85f,clip,actor+1,priority});return true;
}
void Game::updateNpcVoices(float dt){
 int ms=std::max(0,int(std::round(dt*1000)));setState(stateId("npc_voice_busy"),std::max(0,state("npc_voice_busy")-ms));
 for(size_t actor=0;actor<m_friendlyActors.size();++actor){const auto&npc=m_friendlyActors[actor];const auto&track=npc.track();
  if(!voiceRole(track)||!autonomousActor(track)||!chunkResident(npc.level))continue;
  auto get=[&](const char*field){return state(friendlyKey(actor,field));};auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
  for(const char*field:{"voice_lock","voice_hurt_wait","voice_patrol","voice_pending_ms"})put(field,std::max(0,get(field)-ms));
  if(state(track.deadState)||npcIncapacitated(actor)){put("voice_pending",0);continue;}
  auto pending=[&](NpcVoiceCue cue){int old=get("voice_pending")-1;if(old<0||voicePriority(cue)>=voicePriority(NpcVoiceCue(old))){put("voice_pending",int(cue)+1);put("voice_pending_ms",4000);}};
  if(!get("voice_initialized")){put("voice_initialized",1);put("voice_patrol",12000+int(actor*1703%13000));
   put("voice_mode",int(guardState(actor)));put("voice_hold",get("hold"));put("voice_engaged",get("engaged"));
   if(track.ai.mode==ActorAiMode::Guard&&guardState(actor)==GuardState::Alert)pending(NpcVoiceCue::Alert);
   if(track.ai.mode==ActorAiMode::Escort&&get("engaged"))pending(get("hold")?NpcVoiceCue::Wait:NpcVoiceCue::Follow);
  }
  if(track.ai.mode==ActorAiMode::Guard){
   int mode=int(guardState(actor));if(mode!=get("voice_mode")){
    if(mode==int(GuardState::Alert))pending(NpcVoiceCue::Alert);
    else if(mode==int(GuardState::Investigate))pending(NpcVoiceCue::Investigate);
    put("voice_mode",mode);
   }
   bool retreat=get("evac_level")!=0;if(retreat&&!get("voice_retreat"))pending(NpcVoiceCue::Retreat);put("voice_retreat",retreat);
   bool treating=get("support_ms")>0&&get("support_target")>0;
   if(treating&&!get("voice_treating"))pending(NpcVoiceCue::Treat);put("voice_treating",treating);
   int repairSlot=get("drone_repair_target")-1;bool repairing=false;
   if(repairSlot>=0)for(const auto&drone:engineerDrones())if(drone.slot==repairSlot&&drone.owner==int(actor)&&drone.repair>0){repairing=true;break;}
   if(repairing&&!get("voice_repairing"))pending(NpcVoiceCue::Repair);put("voice_repairing",repairing);
   bool pad=get("engineer_pad_stage")>0;if(pad&&!get("voice_pad"))pending(NpcVoiceCue::Support);put("voice_pad",pad);
   // Cancel stale tactical claims if the underlying task was interrupted.
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
  int request=get("voice_pending")-1;if(request>=0){if(!get("voice_pending_ms"))put("voice_pending",0);
   else if(request<=int(NpcVoiceCue::Danger)&&npcVoice(actor,NpcVoiceCue(request)))put("voice_pending",0);}
 }
}

bool Game::testNpcVoices(){
 std::ofstream out("npc-voice-test.txt");bool pass=true;auto check=[&](bool ok,const char*label){out<<label<<": "<<(ok?"PASS":"FAIL")<<'\n';pass&=ok;};
 Game game;game.loadLevel(14,false);game.updateStreaming(0);game.m_player.pos={13,19};game.m_player.z=-25;
 std::vector<size_t> detail;for(size_t i=0;i<game.npcActors().size();++i)if(game.npcActors()[i].homeLevel==14&&voiceRole(game.npcActors()[i].track()))detail.push_back(i);
 check(detail.size()==6,"Receiving uses six distinct skinned voice roles");
 for(auto actor:detail){game.m_sounds.clear();game.setState(stateId("npc_voice_busy"),0);game.setState(game.friendlyKey(actor,"voice_lock"),0);
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
 if(detail.size()>1){auto actor=detail[1];game.m_sounds.clear();game.setState(stateId("npc_voice_busy"),0);game.setState(game.friendlyKey(actor,"voice_lock"),0);
  check(game.npcVoice(actor,NpcVoiceCue::Investigate)&&game.aiStimuli().empty(),"Spoken investigation does not recursively alert the AI hearing system");
  game.setState(stateId("npc_voice_busy"),0);game.setState(game.friendlyKey(actor,"voice_lock"),0);game.m_player.pos={100,100};check(!game.npcVoice(actor,NpcVoiceCue::Patrol),"Distant actors do not reserve the local dialogue channel");
 }
 {auto escort=exclusionInspection(2);escort.setState(stateId("exclusion_relay_secured"),1);size_t witness=0;while(witness<escort.npcActors().size()&&voiceRole(escort.npcActors()[witness].track())!=7)++witness;
  check(witness<escort.npcActors().size(),"Exclusion witness has the hazmat escort voice");
  if(witness<escort.npcActors().size()){const auto&npc=escort.npcActors()[witness];escort.m_player.pos=npc.pose.position;escort.m_player.z=npc.pose.z;escort.setState(escort.friendlyKey(witness,"engaged"),0);escort.updateNpcVoices(0);
   auto expect=[&](NpcVoiceCue cue){escort.m_sounds.clear();escort.setState(stateId("npc_voice_busy"),0);escort.setState(escort.friendlyKey(witness,"voice_lock"),0);escort.updateNpcVoices(.016f);
    return std::any_of(escort.sounds().begin(),escort.sounds().end(),[&](const auto&e){return e.speaker==witness+1&&e.speech>=0&&NpcVoiceClips[size_t(e.speech)].cue==cue;});};
   escort.setState(escort.friendlyKey(witness,"engaged"),1);check(expect(NpcVoiceCue::Follow),"Witness acknowledges the actual follow state transition");
   escort.setState(escort.friendlyKey(witness,"hold"),1);check(expect(NpcVoiceCue::Wait),"Witness acknowledges the actual wait state transition");
   escort.setState(escort.friendlyKey(witness,"fear"),4000);check(expect(NpcVoiceCue::Danger),"Witness danger uses its own frightened masked take");
  }
 }
 return pass;
}
}
