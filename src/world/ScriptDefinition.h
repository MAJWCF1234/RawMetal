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
// A saved, trigger-started timeline shared by main and custom campaigns.
struct TimedSequence {
 StateId timerState=0,finishState=0;
 float x1=0,y1=0,x2=24,y2=24,bottom=-100,top=100;
 int durationMs=30000,finishAtMs=16000,soundIntervalMs=1800,soundUntilMs=16000;
 Sound tickSound=Sound::Metal1;float gain=.5f,pitch=.65f;
};
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
