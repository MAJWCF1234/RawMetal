#pragma once
#include <cstdint>

namespace retro {
// Named recurring hunters can be given this policy by a map script. The high
// bit range is separate from ordinary RMC state names and actor state IDs.
constexpr std::uint32_t hunterLightRetreatState(std::uint32_t hunter) {
  return 0x40000000u | hunter;
}
}
