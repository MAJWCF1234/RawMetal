#pragma once
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
#include <array>
#include <string>
#include <vector>
namespace retro {
struct EditorPreviewOptions {
 std::filesystem::path pack,control,status,capture;
 HWND parent=nullptr; bool hidden=false; int frames=0;
};
int runEditorPreview(const EditorPreviewOptions& options);
// Native parser/socket placement and initial cable physics, used before detaching
// a copied cable from owners that were not copied. Coordinates are ax,ay,az,bx,by,bz.
std::vector<std::array<float,6>> resolveEditorLeadEndpoints(const std::string& text,int map);
int validateEditorCampaign(const std::filesystem::path& path);
int testCampaignEditingExport(const std::filesystem::path& path);
int testLayerEditingExport(const std::filesystem::path& path);
int testCorridorEditingExport(const std::filesystem::path& list);
}
#endif
