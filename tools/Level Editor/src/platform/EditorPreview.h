#pragma once
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#include <array>
#include <string>
#include <vector>
#include <memory>
namespace retro {
struct CustomCampaign;
class Win32Window;
struct EditorPreviewOptions {
 std::filesystem::path pack,control,status,capture;
 // --perf-trace: reload costs and frame times, appended here (empty: off).
 std::filesystem::path perf;
 HWND parent=nullptr; bool hidden=false; int frames=0;
 // Created on the builder's thread. A child window made on the picture's
 // thread can destroy that builder window, which drops the user back at the menu.
 Win32Window* sharedWindow=nullptr;
 // The builder in this same process (RawMetal.exe): pack, control and status
 // live in editorchannel (EditorChannel.h) instead of on disk.
 bool inProcess=false;
 // --perf-baseline: reparse and build the whole campaign on every edit, as before (timing only).
 bool legacyReload=false;
};
// Live edits: the campaign the picture shows, reparsed one map at a time.
// The builder edits one map at a time, so a change usually touches one map's
// records; only those (with the campaign-wide records and the few that are
// checked across maps) go back through the engine's own parser, and the other
// maps' parsed data is kept. Anything else, or any doubt, parses it all.
class LiveCampaign {
public:
 // `how` says which path ran: "map only", "whole campaign" or "unchanged".
 std::shared_ptr<const CustomCampaign> load(const std::string& text,int shown,const std::string& source,std::string& how);
 // Self-check for --editor-live-campaign-test: every map-only result against a full parse.
 static bool test(const std::filesystem::path& file,std::string& report);
private:
 struct Split{std::uint64_t global=0;std::vector<std::uint64_t> maps;std::vector<int> owner;};
 static Split split(const std::string& text);
 static std::shared_ptr<const CustomCampaign> mapOnly(const std::string& text,const Split& split,int map,const std::string& source,const std::shared_ptr<const CustomCampaign>& good);
 std::shared_ptr<const CustomCampaign> m_good;Split m_split;
};
// The picture window, made on the calling thread (the builder's), so the
// picture thread only draws into it.
Win32Window* createEditorViewport(HWND parent, bool visible);
void destroyEditorViewport(Win32Window* window);
int runEditorPreview(const EditorPreviewOptions& options);
// Native parser/socket placement and initial cable physics, used before detaching
// a copied cable from owners that were not copied. Coordinates are ax,ay,az,bx,by,bz.
std::vector<std::array<float,6>> resolveEditorLeadEndpoints(const std::string& text,int map);
int validateEditorCampaign(const std::filesystem::path& path);
int testCampaignEditingExport(const std::filesystem::path& path);
int testLayerEditingExport(const std::filesystem::path& path);
int testCorridorEditingExport(const std::filesystem::path& list);
// The same check on the document text itself (the builder in this process).
int validateEditorCampaignText(const std::string& text);
}
#endif
