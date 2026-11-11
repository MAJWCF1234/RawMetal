#pragma once
// The campaign's C++ maps as custom-campaign text (.rmc), the way back in
// (campaign map overrides), and the record-by-record comparison that proves
// the trip loses nothing. See MAP_SYSTEM.md "Campaign maps as .rmc".
#include "World.h"
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace retro {
// THE RELEASE SWITCHES, one per chapter boundary.
// Chapter 2 is everything past the great door (chunk 68, Research Intake, behind
// the chapter2_open seal) up to the Extraction Interface (109). Chapter 3 begins
// at Mine Access (110), behind the chapter3_open seal on 109's south door.
// While a chapter's switch is false: its seal door has no way to open in play
// (the great door's own script sets chapter2_open only when Chapter 2 is
// released; nothing sets chapter3_open yet), and a released copy of the game or
// the Campaign Builder neither lists nor exports its levels. Set a switch true
// when that chapter ships and its door and its levels open by themselves. The
// source workspace (or --dev-campaign) always sees every level.
inline constexpr bool Chapter2Released=false;
inline constexpr bool Chapter3Released=false;
inline constexpr int FirstSealedChunk=68;      // Chapter 2's first level
inline constexpr int FirstChapter3Chunk=110;   // Chapter 3's first level
inline bool campaignChunkSealed(int chunk){
 if(chunk<FirstSealedChunk||chunk>=CampaignChunkCount||chunk==PrologueChunk)return false;
 return chunk>=FirstChapter3Chunk?!(Chapter2Released&&Chapter3Released):!Chapter2Released;
}
// The source workspace the exe runs from (src/world/CampaignChapters.cpp in its folder or
// up to two above it: the owner's own build), or empty for a released copy.
std::filesystem::path campaignWorkspace();
// A public release: public-release.txt beside the exe (tools/release/package.py puts it
// in every public zip). Only there do the tools hide sealed levels; every other build,
// the owner's own and any copy he hands on, lists, exports and overrides them all.
bool publicRelease();
// Sealed levels are open to the tools: not a public release, or a public one whose
// Windows user unlocked them as a whitelisted admin (core/CampaignAdmin.h).
bool devCampaignTools();
void setDevCampaignTools(bool on);
// "CHAPTER 2" and "RESEARCH FRONT" for a chunk; every chunk in play order (prologue first).
struct CampaignChunkLabel {int chapter=1;const char* district="";};
CampaignChunkLabel campaignChunkLabel(int chunk);
std::vector<int> campaignChunksInPlayOrder();

// One map to write. chunk is its campaign index (-1: a custom map), for notes
// about C++ behaviour the format cannot carry.
struct RmcMapSource {std::shared_ptr<const AuthoredMapData> map;int chunk=-1;};
struct RmcExport {
 std::string text;
 // What the format could not carry, one line each; also in the file's header.
 std::vector<std::string> notes;
};
// The maps as one custom campaign (map i of the file is maps[i]). Terrain is
// the first map's; storm applies to the whole campaign.
RmcExport exportRmc(const std::string& campaignName,const std::vector<RmcMapSource>& maps,int startMap=0,const StormCycle& storm={});
// A campaign chunk alone, named from the campaign table: as the game plays it (its
// override, if any), or its C++ map. The header gets a CAMPAIGN_BASE line: the chunk,
// the game version and the hash of what was exported, the base an edit is merged against.
RmcExport exportCampaignChunk(int chunk,bool asPlayed=true);
// SHA-256 (hex) of a file's records: the data section without comments or blank lines.
std::string rmcDataHash(const std::string& text);
// This exe's FileVersion, "1.2.8".
std::string gameVersion();
// A state's name when the map sources spell one with this id, else "#id".
std::string stateReference(StateId id);
// A file name for a chunk: "068 RESEARCH INTAKE.rmc".
std::string campaignChunkFileName(int chunk);

// Record-by-record differences between two maps, "doors[2].swing: 1 vs -1".
// Test-lead pilot lamps (re-made when a LEAD is seated) are matched by what
// they are, not by where they sit in the light list. At most `limit` lines.
std::vector<std::string> compareAuthoredMaps(const AuthoredMapData& a,const AuthoredMapData& b,bool compareEventLevels,size_t limit=40);

// Campaign map overrides: campaign-overrides/NN.rmc in the repository is
// embedded at build (resource 6000+NN) and the game builds chunk NN from it
// instead of its C++ builder. In the workspace the folder is read directly (so
// a level saved into the game plays at once). None are installed by default.
inline constexpr int CampaignOverrideResourceBase=6000;
// The map an override file holds, fitted to campaign chunk `chunk`: its world
// origin is the chunk's own and its script events belong to the chunk.
std::shared_ptr<const AuthoredMapData> campaignOverrideFromText(int chunk,std::string text,const std::string& source);
// The override in force for a chunk, or null.
std::shared_ptr<const AuthoredMapData> campaignOverride(int chunk);
// What World and the tools build a campaign chunk from: its override, else campaignMap.
std::shared_ptr<const AuthoredMapData> campaignMapResolved(int chunk);
// Developer and test use: install (or with null, remove) an override at run time.
void setCampaignOverride(int chunk,std::shared_ptr<const AuthoredMapData> map);
// Install every "NN*.rmc" in a folder; returns one line per file (loaded or why not).
std::vector<std::string> loadCampaignOverrideFolder(const std::filesystem::path& folder);
// The chunk a file name starts with ("68.rmc", "068 RESEARCH INTAKE.rmc"), or -1.
int campaignOverrideChunk(const std::filesystem::path& file);
// Embedded overrides that failed to load (the C++ map is used instead).
std::vector<std::string> campaignOverrideErrors();

// --export-roundtrip-test and --campaign-override-test (results in the named report).
bool testRmcExportRoundTrip(const std::filesystem::path& report);
bool testCampaignOverrides(const std::vector<int>& chunks,const std::filesystem::path& report,const std::filesystem::path& keepDirectory);
// The command line: --dev-campaign (sealed maps), --campaign-overrides=<folder> (dev),
// --export-rmc=<chunks> --out=<file.rmc|folder>, --campaign-map-list [--out=<file>],
// --export-roundtrip-test and --campaign-override-test[=<chunks>] [--keep=<folder>].
// Chunks: "68-109", "0,10,34", "chapter1", "chapter2", "all". Returns the exit code of the
// command run, or -1 when none was asked for (the switches still apply).
int campaignRmcCommands(const std::vector<std::wstring>& arguments);
// "68-109,179" as chunk numbers; throws on anything else.
std::vector<int> parseCampaignChunks(const std::string& spec);
}
