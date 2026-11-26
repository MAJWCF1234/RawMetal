#include "lab/Lab.h"
#include "lab/LabAccess.h"

namespace retro::lab {
namespace {
thread_local std::vector<std::pair<StateId, int>> stepTrace;
const bool registered = add({
  .name = "ai-manager",
  .summary = "Play the map built, saved, reopened and undone in the Builder manager regression.",
  .fixtures = {"ai-manager"},
  .setup = [](Scene &scene) {
    LabAccess::setGod(scene.g(), true);
    stepTrace.clear();
    scene.g().setScriptTrace([](StateId event, int action) { stepTrace.emplace_back(event, action); });
  },
  .metrics = [](Scene &scene, Report &report) {
    report.check("the Director rule from the saved Builder map ran",
                 LabAccess::scriptState(scene.g(), stateId("switch_1")) == 1, "switch_1");
    const auto event = stateId("director_0_1_rule_1");
    const std::vector<std::pair<StateId, int>> expected{
      {event, -1}, {event, 0}, {event, 1}, {event, 2}, {event, 3}};
    report.check("the trace marked the trigger and each action in execution order",
                 stepTrace == expected, std::to_string(stepTrace.size()) + " entries");
  }
});
}
}
