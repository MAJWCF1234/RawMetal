#pragma once
#include "../../../world/ScriptDefinition.h"
#include <algorithm>
#include <cstdint>
namespace retro {
// Shared guard combat action clocks and squad role profiles. New roles belong
// here as data; new behaviors plug into Mind/Support rather than forking the FSM.
enum class GuardAction {None,Fire,Bash,Reload};
enum GuardActionEvent : unsigned {GuardActionStart=1,GuardActionWork=2,GuardActionChamber=4,GuardActionComplete=8};
constexpr int guardActionDuration(GuardAction action){return action==GuardAction::Fire?220:action==GuardAction::Bash?520:action==GuardAction::Reload?1800:0;}
constexpr unsigned guardActionMask(GuardAction action,int elapsed){
 if(action==GuardAction::None)return 0;
 unsigned mask=GuardActionStart;
 if(elapsed>=(action==GuardAction::Fire?120:action==GuardAction::Bash?300:600))mask|=GuardActionWork;
 if(action==GuardAction::Reload&&elapsed>=1200)mask|=GuardActionChamber;
 if(elapsed>=guardActionDuration(action))mask|=GuardActionComplete;
 return mask;
}
// Persist consumed alongside elapsed. A jump crosses each event exactly once,
// and restoring the same phase never replays an earlier sound or damage event.
inline unsigned consumeGuardAction(GuardAction action,int elapsed,unsigned& consumed){auto passed=guardActionMask(action,elapsed);auto events=passed&~consumed;consumed|=passed;return events;}
inline bool validGuardAction(GuardAction action,int elapsed,unsigned consumed){return int(action)>=0&&int(action)<=3&&elapsed>=0&&elapsed<=guardActionDuration(action)&&consumed==guardActionMask(action,elapsed);}

struct GuardRoleProfile {const char* name;float advanceFraction,minimumDistance;bool flanks,suppresses;int treatments;};
constexpr GuardRoleProfile guardRoleProfile(GuardSquadRole role){
 switch(role){
 case GuardSquadRole::Assault:return {"Assault",.65f,0,true,false,0};
 case GuardSquadRole::Engineer:return {"Engineer",.75f,0,false,false,0};
 case GuardSquadRole::Sharpshooter:return {"Sharpshooter",.95f,4,false,false,0};
 case GuardSquadRole::Heavy:return {"Heavy",.70f,0,false,true,0};
 case GuardSquadRole::Medic:return {"Medic",.85f,0,false,false,2};
 case GuardSquadRole::Rusher:return {"Rusher",.12f,0,true,false,0};
 default:return {"Independent",.85f,0,true,false,0};
 }
}
constexpr bool guardSuppressionEnabled(const ActorAiConfig& ai){return ai.suppress||guardRoleProfile(ai.role).suppresses;}
constexpr bool guardHandgun(const ActorAiConfig& ai){return ai.weapon==2||ai.role==GuardSquadRole::Medic||ai.role==GuardSquadRole::Engineer;}
}
