#include "GuardCtx.h"
#include "../../Game.h"
#include "Schedule.h"
#include <cmath>
namespace retro {
int GuardCtx::get(std::string_view field) const { return game.state(game.friendlyKey(actor, field)); }
void GuardCtx::put(std::string_view field, int value) { game.setState(game.friendlyKey(actor, field), value); }
GuardState GuardCtx::mode() const { return GuardState(get("guard_mode")); }
void GuardCtx::setMode(GuardState value) { put("guard_mode", int(value)); }
GuardSchedule GuardCtx::schedule() const { return GuardSchedule(get("schedule")); }
void GuardCtx::setSchedule(GuardSchedule value) { put("schedule", int(value)); }
GuardTask GuardCtx::task() const { return GuardTask(get("task")); }
void GuardCtx::setTask(GuardTask value) { put("task", int(value)); }
GuardTaskStatus GuardCtx::taskStatus() const { return GuardTaskStatus(get("task_status")); }
void GuardCtx::setTaskStatus(GuardTaskStatus value) { put("task_status", int(value)); }
bool GuardCtx::hasCoverGoal() const { return get("guard_cover") != 0; }
Vec2 GuardCtx::coverGoal() const { return {get("guard_cover_x") * .001f, get("guard_cover_y") * .001f}; }
float GuardCtx::coverFeet() const { return get("guard_cover_z") * .001f; }
int GuardCtx::coverMode() const { return get("guard_cover_mode"); }
void GuardCtx::setCoverGoal(Vec2 position, float feet, int mode) {
 put("guard_cover", 1);
 put("guard_cover_x", int(std::round(position.x * 1000)));
 put("guard_cover_y", int(std::round(position.y * 1000)));
 put("guard_cover_z", int(std::round(feet * 1000)));
 put("guard_cover_mode", mode);
}
void GuardCtx::clearCoverGoal() { put("guard_cover", 0); put("guard_cover_wait", 0); }
bool GuardCtx::confirmed() const { return get("guard_confirmed") != 0; }
void GuardCtx::setConfirmed(bool value) { put("guard_confirmed", value ? 1 : 0); }
Vec2 GuardCtx::lastKnown() const { return {get("guard_x") * .001f, get("guard_y") * .001f}; }
void GuardCtx::setLastKnown(Vec2 position, float feet) {
 put("guard_x", int(std::round(position.x * 1000)));
 put("guard_y", int(std::round(position.y * 1000)));
 put("guard_z", int(std::round(feet * 1000)));
 put("guard_memory", 1);
}
}
