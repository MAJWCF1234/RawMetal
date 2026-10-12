#include "Mind.h"
#include "../../Game.h"
#include <cmath>
namespace retro {
int CivilianMindView::get(std::string_view field) const { return game.state(game.friendlyKey(actor, field)); }
void CivilianMindView::put(std::string_view field, int value) { game.setState(game.friendlyKey(actor, field), value); }
int CivilianMindView::fear() const { return get("fear"); }
void CivilianMindView::setFear(int ms) { put("fear", ms); }
Vec2 CivilianMindView::threat() const { return {get("threat_x") * .001f, get("threat_y") * .001f}; }
void CivilianMindView::setThreat(Vec2 position) {
 put("threat_x", int(std::round(position.x * 1000)));
 put("threat_y", int(std::round(position.y * 1000)));
}
CivilianSchedule CivilianMindView::schedule() const { return CivilianSchedule(get("schedule")); }
void CivilianMindView::setSchedule(CivilianSchedule value) { put("schedule", int(value)); }
CivilianTask CivilianMindView::task() const { return CivilianTask(get("task")); }
void CivilianMindView::setTask(CivilianTask value) { put("task", int(value)); }
CivilianTaskStatus CivilianMindView::taskStatus() const { return CivilianTaskStatus(get("task_status")); }
void CivilianMindView::setTaskStatus(CivilianTaskStatus value) { put("task_status", int(value)); }
bool CivilianMindView::engaged() const { return get("engaged") != 0; }
void CivilianMindView::setEngaged(bool value) { put("engaged", value ? 1 : 0); }
bool CivilianMindView::holding() const { return get("hold") != 0; }
void CivilianMindView::setHolding(bool value) { put("hold", value ? 1 : 0); }
bool CivilianMindView::following() const { return get("follow") != 0; }
void CivilianMindView::setFollowing(bool value) { put("follow", value ? 1 : 0); }
}
