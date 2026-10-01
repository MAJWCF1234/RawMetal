#pragma once
#include "World.h"
namespace retro {
// Built-in map authoring registry; null entries retain the legacy map builder.
std::shared_ptr<const AuthoredMapData> campaignMap(int level);
}
