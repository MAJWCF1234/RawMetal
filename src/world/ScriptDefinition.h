#pragma once
#include "WorldDefinition.h"
#include "../audio/Sound.h"
#include <cstdint>
#include <string_view>
#include <string>
#include <vector>
#include <array>
#include <charconv>
#include <cstring>
#include <limits>
namespace retro {
using StateId=std::uint32_t;
constexpr StateId stateId(std::string_view value){StateId hash=2166136261u;for(char c:value){hash^=static_cast<unsigned char>(c);hash*=16777619u;}hash&=0x7fffffu;return hash?hash:1u;}
enum class ObjectiveStatus { Hidden, Active, Complete, Failed };
struct StateValue {StateId id=0;int value=0;};
struct ObjectiveText {StateId id=0;std::string text;};
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
enum class ActorVisual { Worker, Wasp, Huntsman, Cargo, Cleanup, CleanupEngineer, CleanupSharpshooter, CleanupHeavy, CleanupMedic, CleanupRusher };
constexpr bool humanActorVisual(ActorVisual visual){return visual==ActorVisual::Worker||(int(visual)>=4&&int(visual)<=9);}
struct ActorKey {int timeMs=0;Vec2 position{};float z=0,yaw=0;int clip=0;float phase=0;int lookAtActor=-1;};
enum class ActorAiMode { Scripted, Civilian, Escort, Guard=3 };
enum class AiFaction { Unspecified, Player, Facility, Creature, Raider, Neutral };
enum class GuardSquadRole { Default, Assault, Engineer, Sharpshooter, Heavy, Medic, Rusher };
inline AiFaction actorFaction(const struct ActorTrack& track);
inline bool factionsHostile(AiFaction a,AiFaction b){
 if(a==b||a==AiFaction::Neutral||b==AiFaction::Neutral)return false;
 if(a==AiFaction::Player||a==AiFaction::Facility)return b==AiFaction::Creature||b==AiFaction::Raider;
 return a==AiFaction::Creature||a==AiFaction::Raider;
}
// look: the bearing a guard faces while paused at this post (yaw radians).
// Unset (NaN), it keeps the bearing it arrived on. Either way a guard on a
// multi-point route scans slowly either side of it while it waits.
struct ActorPatrolPoint {Vec2 position{};float z=0;int pauseMs=1000;float look=std::numeric_limits<float>::quiet_NaN();};
struct ActorAiConfig {
 ActorAiMode mode=ActorAiMode::Scripted;StateId enableState=0;float speed=1.4f,followDistance=2.f,dangerRange=6.f;
 float sightRange=11,sightDot=.65f;int recognitionMs=600,searchMs=4000;StateId alertState=0;
 std::vector<ActorPatrolPoint> patrol;
 bool armed=false;int aimMs=650,recoveryMs=1000;float attackRange=9,attackDamage=18;
 AiFaction faction=AiFaction::Unspecified;StateId squad=0;
 bool suppress=false;int grenades=0;
 GuardSquadRole role=GuardSquadRole::Default;
 int weapon=1;
};
struct ActorTrack {
 StateId timerState=0;ActorVisual visual=ActorVisual::Worker;
 float scale=1.7f;bool loop=false,tool=false;std::vector<ActorKey> keys;
 bool platform=false;Vec2 footprint{1,1};float thickness=.2f;
 float suspensionTop=-999;
 StateId damageState=0,deadState=0;int health=0;
 std::vector<ActorKey> idleKeys;int idleUntilMs=0,approachUntilMs=0;
 ActorAiConfig ai{};
};
inline AiFaction actorFaction(const ActorTrack& track){
 if(track.ai.faction!=AiFaction::Unspecified)return track.ai.faction;
 if(track.ai.mode==ActorAiMode::Guard)return AiFaction::Raider;
 if(track.visual==ActorVisual::Wasp||track.visual==ActorVisual::Huntsman)return AiFaction::Creature;
 return humanActorVisual(track.visual)?AiFaction::Facility:AiFaction::Neutral;
}
struct SequenceCue {
 StateId timerState=0;int timeMs=0;Sound sound=Sound::Metal1;
 Vec2 position{};float gain=1,pitch=1;std::string caption;float z=std::numeric_limits<float>::quiet_NaN();
};
struct ActorPose {Vec2 position{};float z=0,yaw=0;int clip=0;float phase=0;int lookAtActor=-1;};
inline StateId actorTrackState(StateId timer,size_t index,std::string_view field){return stateId("actor_track/"+std::to_string(timer)+"/"+std::to_string(index)+"/"+std::string(field));}
inline StateId actorPositionState(StateId dead,int axis){return stateId("actor_position/"+std::to_string(dead)+"/"+std::to_string(axis));}
inline StateId actorAiState(int level,size_t actor,StateId timer,std::string_view field){
 // Pure IDs only: neither cache retains Game pointers, state values or views
 // into caller-owned text. Validate complete suffix bytes even at one address.
 struct Prefix {size_t actor=0;int level=0;StateId timer=0,hash=0;bool ready=false;};
 static thread_local std::array<Prefix,128> prefixes{};
 uint32_t slot=uint32_t(actor)*0x9e3779b9u^uint32_t(level)*0x85ebca6bu^timer;slot^=slot>>16;
 auto&prefix=prefixes[slot&(prefixes.size()-1)];
 if(!prefix.ready||prefix.actor!=actor||prefix.level!=level||prefix.timer!=timer){
  StateId hash=2166136261u;auto append=[&](std::string_view text){for(unsigned char c:text)hash=(hash^c)*16777619u;};
  auto number=[&](auto value){char buffer[32];auto result=std::to_chars(buffer,buffer+sizeof(buffer),value);append(std::string_view(buffer,size_t(result.ptr-buffer)));};
  append("npc/");number(level);append("/");number(actor);append("/");number(timer);append("/");
  prefix={actor,level,timer,hash,true};
 }
 struct Field {StateId prefix=0,value=0;size_t size=0;std::array<char,48> text{};};
 static thread_local std::array<Field,2048> fields{};
 auto fieldSlot=(reinterpret_cast<std::uintptr_t>(field.data())>>3)^prefix.hash;fieldSlot^=fieldSlot>>16;
 auto&cached=fields[fieldSlot&(fields.size()-1)];
 if(field.size()<=cached.text.size()&&cached.value&&cached.prefix==prefix.hash&&cached.size==field.size()&&
    (field.empty()||std::memcmp(cached.text.data(),field.data(),field.size())==0))return cached.value;
 StateId hash=prefix.hash;for(unsigned char c:field)hash=(hash^c)*16777619u;
 StateId value=hash|0x80000000u; // Preserve every existing serialized state ID.
 if(field.size()<=cached.text.size()){cached.prefix=prefix.hash;cached.value=value;cached.size=field.size();if(!field.empty())std::memcpy(cached.text.data(),field.data(),field.size());}
 return value;
}
ActorPose sampleActor(const ActorTrack& track,int ageMs);
ActorPose sampleActorKeys(const std::vector<ActorKey>& keys,int ageMs,bool loop=false);
struct ScriptAction {
 enum class Type {SetState,SetObjective,GiveItem,TakeItem,OpenDoor,CloseDoor,ReleaseControl,PlaySound,SpawnEnemy,Shake,Checkpoint,CompleteCampaign,EnterMainCampaign};
 Type type=Type::SetState;StateId id=0;int value=0,index=0;CreatureKind enemyKind=CreatureKind::Huntsman;Vec2 position{};float z=-999,amount=0;Sound sound=Sound::Exit;
};
struct ScriptEvent {
 StateId id=0;int level=0;float x1=0,y1=0,x2=0,y2=0,bottom=-100,top=100;StateId requireState=0;int requireValue=1;bool requireEnemiesClear=false,once=true;
 std::vector<ScriptAction> actions;
};
}
