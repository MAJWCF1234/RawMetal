#pragma once
#include "../core/Math.h"
namespace retro {
// Engine-owned physical drones; authored Engineer actors only provide their
// faction, squad and owner identity. Positions describe feet in the given chunk.
inline constexpr int EngineerDroneSlots=4,EngineerDroneHealth=60;
inline constexpr float EngineerDroneRadius=.60f,EngineerDroneHeight=1.08f;
struct EngineerDronePose {
 int slot=0,owner=0,level=0;Point3 position{};float yaw=0,health=0,flash=0,repair=0,phase=0;bool walking=false;
};
}
