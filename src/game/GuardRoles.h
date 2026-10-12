#pragma once
#include "../world/ScriptDefinition.h"
namespace retro {
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
constexpr bool guardHandgun(const ActorAiConfig& ai){return ai.weapon==2||ai.role==GuardSquadRole::Medic;}
}
