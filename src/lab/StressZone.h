#pragma once
#include <algorithm>
#include <string>
namespace retro::lab {
// The campaign text of a chunks x chunks open outdoor world (1 to 13 a side) at a detail of 1 to 4
// (about 50 fixtures a chunk at 1, about 200 at 4, which is the Ashfall zone's density).
std::string stressCampaign(int chunks,int detail);
}
