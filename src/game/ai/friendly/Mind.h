#pragma once
#include "../../../core/Math.h"
#include "Schedule.h"
#include <cstddef>
#include <string_view>
namespace retro {
class Game;
// Typed view over civilian/escort/scripted-handoff save keys. String field
// names stay identical so existing saves and validation keep working.
struct CivilianMindView {
 Game& game;
 size_t actor;
 int get(std::string_view field) const;
 void put(std::string_view field, int value);
 int fear() const;
 void setFear(int ms);
 Vec2 threat() const;
 void setThreat(Vec2 position);
 CivilianSchedule schedule() const;
 void setSchedule(CivilianSchedule value);
 CivilianTask task() const;
 void setTask(CivilianTask value);
 CivilianTaskStatus taskStatus() const;
 void setTaskStatus(CivilianTaskStatus value);
 bool engaged() const;
 void setEngaged(bool value);
 bool holding() const;
 void setHolding(bool value);
 bool following() const;
 void setFollowing(bool value);
};
}
