#pragma once
#include "WorldDefinition.h"
#include "../audio/Sound.h"
#include <cstdint>
#include <string_view>
#include <string>
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
 bool loop=false;
 int sightActor=-1;float sightDistance=30;
};
struct QuestItemStack {StateId id=0;int count=0;};
// Shared, saved actor and machinery timelines. Keys use absolute map elevation.
enum class ActorVisual { Worker, Wasp, Huntsman, Cargo };
struct ActorKey {int timeMs=0;Vec2 position{};float z=0,yaw=0;int clip=0;float phase=0;int lookAtActor=-1;};
enum class ActorAiMode { Scripted, Civilian, Escort };
struct ActorAiConfig {ActorAiMode mode=ActorAiMode::Scripted;StateId enableState=0;float speed=1.4f,followDistance=2.f,dangerRange=6.f;};
struct ActorTrack {
 StateId timerState=0;ActorVisual visual=ActorVisual::Worker;
 float scale=1.7f;bool loop=false,tool=false;std::vector<ActorKey> keys;
 bool platform=false;Vec2 footprint{1,1};float thickness=.2f;
 float suspensionTop=-999;
 StateId damageState=0,deadState=0;int health=0;
 std::vector<ActorKey> idleKeys;int idleUntilMs=0,approachUntilMs=0;
 ActorAiConfig ai{};
};
struct SequenceCue {
 StateId timerState=0;int timeMs=0;Sound sound=Sound::Metal1;
 Vec2 position{};float gain=1,pitch=1;std::string caption;
};
struct ActorPose {Vec2 position{};float z=0,yaw=0;int clip=0;float phase=0;int lookAtActor=-1;};
inline StateId actorTrackState(StateId timer,size_t index,std::string_view field){return stateId("actor_track/"+std::to_string(timer)+"/"+std::to_string(index)+"/"+std::string(field));}
inline StateId actorPositionState(StateId dead,int axis){return stateId("actor_position/"+std::to_string(dead)+"/"+std::to_string(axis));}
inline StateId actorAiState(int level,size_t actor,StateId timer,std::string_view field){
 auto text="npc/"+std::to_string(level)+"/"+std::to_string(actor)+"/"+std::to_string(timer)+"/"+std::string(field);StateId hash=2166136261u;
 for(unsigned char c:text)hash=(hash^c)*16777619u;
 return hash|0x80000000u; // Internal NPC keys cannot collide with authored stateId flags.
}
ActorPose sampleActor(const ActorTrack& track,int ageMs);
ActorPose sampleActorKeys(const std::vector<ActorKey>& keys,int ageMs,bool loop=false);
struct ScriptAction {
 enum class Type {SetState,SetObjective,GiveItem,TakeItem,OpenDoor,CloseDoor,ReleaseControl,PlaySound,SpawnEnemy,Shake,Checkpoint,CompleteCampaign};
 Type type=Type::SetState;StateId id=0;int value=0,index=0;CreatureKind enemyKind=CreatureKind::Huntsman;Vec2 position{};float z=-999,amount=0;Sound sound=Sound::Exit;
};
struct ScriptEvent {
 StateId id=0;int level=0;float x1=0,y1=0,x2=0,y2=0,bottom=-100,top=100;StateId requireState=0;int requireValue=1;bool requireEnemiesClear=false,once=true;
 std::vector<ScriptAction> actions;
};
}
