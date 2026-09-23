#pragma once
#include "World.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace retro {
std::vector<std::shared_ptr<const CustomCampaign>> loadCustomCampaignDirectory(
    const std::filesystem::path& directory,
    std::vector<std::string>* errors=nullptr);
std::shared_ptr<const CustomCampaign> loadCustomCampaignFile(const std::filesystem::path& path);
}
