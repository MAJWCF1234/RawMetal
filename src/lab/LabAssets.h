#pragma once
#include <string>
#include <vector>
namespace retro::lab {
struct AssetStats {int loads=0,manifest=0;double ms=0;size_t bytes=0;std::string last;};
// Route every engine resource load to the loose source files (LabAssets.cpp).
bool useLooseAssets(std::string& error);
AssetStats assetStats();
std::vector<std::string> assetRoots();
}
