#include "Game.h"
#include "AIPerception.h"
#include <fstream>
#include "../world/CustomCampaign.h"
namespace retro {
int Game::npcMilliseconds(size_t actor,float dt,std::string_view field){auto key=friendlyKey(actor,field);int micros=state(key)+int(std::round(double(dt)*1000000));setState(key,micros%1000);return micros/1000;}
void Game::validateGuardState(size_t actor)const{
 auto get=[&](const char*field){return state(friendlyKey(actor,field));};const auto&config=m_friendlyActors[actor].track().ai;
 if(get("guard_fraction")<0||get("guard_fraction")>=1000||get("weapon_fraction")<0||get("weapon_fraction")>=1000)throw std::runtime_error("Invalid saved guard clock fraction");
 if(get("guard_step")<0||get("guard_step")>=1000||get("guard_bearing")< -31416||get("guard_bearing")>31416)throw std::runtime_error("Invalid saved guard motion");
 if(get("guard_cover")<0||get("guard_cover")>1||get("guard_cover_wait")<0||get("guard_cover_wait")>1000||get("guard_cover_x")<0||get("guard_cover_x")>24000||get("guard_cover_y")<0||get("guard_cover_y")>24000||get("guard_cover_z")< -100000||get("guard_cover_z")>100000)throw std::runtime_error("Invalid saved guard repositioning");
 if(get("guard_cover")&&(get("guard_cover_x")<350||get("guard_cover_x")>23650||get("guard_cover_y")<350||get("guard_cover_y")>23650))throw std::runtime_error("Invalid saved guard cover goal");
 if(get("weapon_aim")<0||get("weapon_aim")>config.aimMs||get("weapon_recovery")<0||get("weapon_recovery")>config.recoveryMs||get("weapon_reload")<0||get("weapon_reload")>1800||get("weapon_rounds")<0||get("weapon_rounds")>weaponDef(WeaponId::Shotgun).cycle.tubeCapacity||get("weapon_raise")<0||get("weapon_raise")>1000||get("weapon_flash")<0||get("weapon_flash")>80||get("weapon_recoil")<0||get("weapon_recoil")>220)throw std::runtime_error("Invalid saved guard weapon state");
 if(get("guard_mode")<0||get("guard_mode")>4||get("guard_point")<0||get("guard_point")>=int(config.patrol.size())||get("guard_pause")<0||get("guard_pause")>60000||get("guard_recognition")<0||get("guard_recognition")>config.recognitionMs||get("guard_search")<0||get("guard_search")>config.searchMs||get("guard_travel")<0||get("guard_travel")>8000||get("guard_z")< -100000||get("guard_z")>100000)throw std::runtime_error("Invalid saved guard awareness");
}
void Game::updateGuardFlags(){
 std::vector<StateValue> flags;
 for(size_t i=0;i<m_friendlyActors.size();++i){const auto&npc=m_friendlyActors[i];const auto&track=npc.track();if(track.ai.mode!=ActorAiMode::Guard||!track.ai.alertState)continue;
  auto it=std::lower_bound(flags.begin(),flags.end(),track.ai.alertState,[](const auto&v,StateId id){return v.id<id;});if(it==flags.end()||it->id!=track.ai.alertState)it=flags.insert(it,{track.ai.alertState,0});
  if((!track.ai.enableState||state(track.ai.enableState))&&npc.pose.clip!=4&&(!track.deadState||!state(track.deadState))&&guardState(i)==GuardState::Alert)it->value=1;
 }for(const auto&flag:flags)setState(flag.id,flag.value);
}
void Game::guardIntent(int level,size_t actor,const ActorTrack& track,ActorPose& pose,float dt,const std::vector<AiStimulus>& stimuli,Vec2& goal,float& goalZ,bool& wantsMove){
 const auto&config=track.ai;auto get=[&](const char*field){return state(friendlyKey(actor,field));};auto put=[&](const char*field,int value){setState(friendlyKey(actor,field),value);};
 auto mode=GuardState(get("guard_mode"));int ms=npcMilliseconds(actor,dt,"guard_fraction"),recognition=get("guard_recognition"),point=get("guard_point");
 auto shift=chunkOffset(level)-chunkOffset(m_level),globalOrigin=chunkOffset(level);auto observer=pose.position+shift;
 float eye=pose.z+track.scale*.85f;Vec2 forward{std::sin(pose.yaw),std::cos(pose.yaw)},toPlayer=m_player.pos-observer;
 float dz=m_player.z+m_player.eye-eye;
 bool visible=!dead()&&lengthSq(toPlayer)+dz*dz<config.sightRange*config.sightRange&&dot(normalized(toPlayer),forward)>config.sightDot&&soundRayClear(observer,eye,m_player.pos,m_player.z+m_player.eye);
 auto remember=[&](Vec2 global,float feet){put("guard_x",int(std::round(global.x*1000)));put("guard_y",int(std::round(global.y*1000)));put("guard_z",int(std::round(feet*1000)));put("guard_memory",1);put("guard_travel",0);put("guard_search",0);};
 if(visible){remember(m_player.pos+chunkOffset(m_level),m_player.z);recognition=std::min(config.recognitionMs,recognition+ms);mode=recognition>=config.recognitionMs?GuardState::Alert:GuardState::Suspicious;}
 else{
  recognition=0;if(mode==GuardState::Suspicious)mode=GuardState::Patrol;else if(mode==GuardState::Alert){mode=GuardState::Investigate;put("guard_travel",0);}
  const AiStimulus*heard=nullptr;float strongest=0;
  for(const auto&noise:stimuli){float height=noise.z-eye;if(lengthSq(noise.source-observer)+height*height>=noise.radius*noise.radius)continue;
   if(noise.npc==int(actor))continue;
   float strength=aiSoundStrength(observer,eye,noise.source,noise.z,noise.radius,noise.gain,noise.loud,soundRayClear(observer,eye,noise.source,noise.z));if(strength>strongest){strongest=strength;heard=&noise;}}
  if(heard){auto local=heard->source;const auto&world=worldAt(local);remember(heard->source+chunkOffset(m_level),world.supportBelow(local.x,local.y,heard->z));mode=GuardState::Investigate;}
 }
 int damage=track.damageState?state(track.damageState):0;if(damage>get("guard_damage")&&get("hit_source")){
  Vec2 hit{get("hit_x")*.001f,get("hit_y")*.001f};remember(hit+globalOrigin,friendlyWorld(level).supportBelow(hit.x,hit.y,pose.z+track.scale));recognition=config.recognitionMs;mode=GuardState::Alert;
 }put("guard_damage",damage);put("hit_source",0);put("guard_recognition",recognition);
 Vec2 remembered{get("guard_x")*.001f-globalOrigin.x,get("guard_y")*.001f-globalOrigin.y};float rememberedZ=get("guard_z")*.001f;
 auto face=[&](Vec2 target){auto toward=target-pose.position;if(lengthSq(toward)>.001f)pose.yaw+=std::clamp(std::remainder(std::atan2(toward.x,toward.y)-pose.yaw,2*kPi),-4.f*dt,4.f*dt);};
 if(mode==GuardState::Suspicious){face(remembered);wantsMove=false;}
 if(mode==GuardState::Alert||mode==GuardState::Investigate){goal={std::clamp(remembered.x,.35f,23.65f),std::clamp(remembered.y,.35f,23.65f)};goalZ=rememberedZ;
  float distance=length(pose.position-goal);wantsMove=distance>(mode==GuardState::Alert?2.f:.35f)||std::fabs(pose.z-goalZ)>.3f;
  if(mode==GuardState::Alert&&config.armed){wantsMove=std::hypot(distance,pose.z-goalZ)>config.attackRange*.85f;face(remembered);
   if(visible&&!wantsMove){Point3 target{m_player.pos.x-shift.x,m_player.pos.y-shift.y,m_player.z+m_player.hullHeight()*.6f};guardReposition(level,actor,track,pose,target,ms,goal,goalZ,wantsMove);}
   else{put("guard_cover",0);put("guard_cover_wait",0);}
  }
  if(!wantsMove&&!(mode==GuardState::Alert&&config.armed))face(remembered);
  if(mode==GuardState::Investigate){int travel=std::min(8000,get("guard_travel")+ms);put("guard_travel",travel);
   if(!wantsMove||travel>=8000){mode=GuardState::Search;put("guard_search",config.searchMs);put("guard_bearing",int(std::round(std::remainder(pose.yaw,2*kPi)*10000)));wantsMove=false;}}
 }
 if(mode==GuardState::Search){int remaining=std::max(0,get("guard_search")-ms);put("guard_search",remaining);pose.yaw=get("guard_bearing")*.0001f+std::sin((config.searchMs-remaining)*.001f*2.4f)*1.2f;wantsMove=false;
  if(remaining==0){mode=GuardState::Patrol;put("guard_arrived",0);put("guard_pause",0);put("repath",0);}}
 if(mode==GuardState::Patrol){auto home=chunkOffset(m_friendlyActors[actor].homeLevel)-globalOrigin;const auto&patrol=config.patrol.at(size_t(point));goal=patrol.position+home;goalZ=patrol.z;
  bool arrived=lengthSq(goal-pose.position)<.25f*.25f&&std::fabs(goalZ-pose.z)<.3f;
  if(arrived){int pause=get("guard_pause");if(!get("guard_arrived")){put("guard_arrived",1);pause=patrol.pauseMs;}
   pause=std::max(0,pause-ms);put("guard_pause",pause);if(pause==0){point=(point+1)%int(config.patrol.size());put("guard_point",point);put("guard_arrived",0);goal=config.patrol[size_t(point)].position+home;goalZ=config.patrol[size_t(point)].z;}
  }wantsMove=!arrived||!get("guard_arrived");
 }
 if(mode!=GuardState::Alert){put("guard_cover",0);put("guard_cover_wait",0);}
 put("guard_mode",int(mode));
}
}
