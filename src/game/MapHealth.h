#pragma once
#include "../core/Math.h"
#include <string>
#include <vector>

namespace retro {
class Game;
struct MapHealthIssue {
 std::string system, object, reason;
 Vec2 position{};
 float z=0;
 bool located=false;
 std::string file;
 unsigned line=0;
};
std::vector<MapHealthIssue> inspectMapHealth(const Game& game);

}
