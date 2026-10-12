#pragma once
#include "../../../core/Math.h"
#include <cstddef>
#include <string_view>
namespace retro {
class Game;
struct ActorTrack;
struct ActorPose;
enum class GuardState;
enum class GuardSchedule;
enum class GuardTask;
enum class GuardTaskStatus;
// Narrow facade over Game's per-actor save keys. Prefer this in new guard code
// instead of repeating get/put lambdas so role and behavior modules stay small.
// Typed accessors keep the existing string key names so saves stay compatible.
struct GuardCtx {
 Game& game;
 size_t actor;
 int level;
 const ActorTrack& track;
 ActorPose& pose;
 int get(std::string_view field) const;
 void put(std::string_view field,int value);
 GuardState mode() const;
 void setMode(GuardState value);
 GuardSchedule schedule() const;
 void setSchedule(GuardSchedule value);
 GuardTask task() const;
 void setTask(GuardTask value);
 GuardTaskStatus taskStatus() const;
 void setTaskStatus(GuardTaskStatus value);
 bool hasCoverGoal() const;
 Vec2 coverGoal() const;
 float coverFeet() const;
 int coverMode() const;
 void setCoverGoal(Vec2 position,float feet,int mode);
 void clearCoverGoal();
 bool confirmed() const;
 void setConfirmed(bool value);
 Vec2 lastKnown() const;
 void setLastKnown(Vec2 position,float feet);
};
}
