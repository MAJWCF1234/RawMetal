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
std::shared_ptr<const CustomCampaign> loadCustomCampaignText(std::string text,std::string sourceFile);
// Embedded one-file release payload (assets.rc id 346).
std::shared_ptr<const CustomCampaign> loadEmbeddedExclusionZoneCampaign();
// The main campaign's opening (assets.rc id 398). New Game starts here and its
// ENTER_CAMPAIGN hands off to campaign Map 0. It is not listed in Custom Maps.
std::shared_ptr<const CustomCampaign> loadEmbeddedPrologueCampaign();
// Built-in payloads that saves may reference without a custom-maps folder.
std::shared_ptr<const CustomCampaign> builtInCampaignForKey(std::uint64_t key);
}
