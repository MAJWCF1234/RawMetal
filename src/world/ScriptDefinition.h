#pragma once
#include "WorldDefinition.h"
#include "../audio/Sound.h"
#include <cstdint>
#include <string_view>
#include <vector>
namespace retro {
using StateId=std::uint32_t;
constexpr StateId stateId(std::string_view value){StateId hash=2166136261u;for(char c:value){hash^=static_cast<unsigned char>(c);hash*=16777619u;}hash&=0x7fffffu;return hash?hash:1u;}
enum class ObjectiveStatus { Hidden, Active, Complete, Failed };
struct StateValue {StateId id=0;int value=0;};
struct QuestItemStack {StateId id=0;int count=0;};
struct ScriptAction {
 enum class Type {SetState,SetObjective,GiveItem,TakeItem,OpenDoor,CloseDoor,ReleaseControl,PlaySound,SpawnEnemy,Shake,Checkpoint,CompleteCampaign};
 Type type=Type::SetState;StateId id=0;int value=0,index=0;CreatureKind enemyKind=CreatureKind::Huntsman;Vec2 position{};float z=-999,amount=0;Sound sound=Sound::Exit;
};
struct ScriptEvent {
 StateId id=0;int level=0;float x1=0,y1=0,x2=0,y2=0,bottom=-100,top=100;StateId requireState=0;int requireValue=1;bool requireEnemiesClear=false,once=true;
 std::vector<ScriptAction> actions;
};
}
