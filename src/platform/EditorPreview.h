#pragma once
#ifdef _WIN32
#include <windows.h>
#include <filesystem>
namespace retro {
struct EditorPreviewOptions {
 std::filesystem::path pack,control,status,capture;
 HWND parent=nullptr; bool hidden=false; int frames=0;
};
int runEditorPreview(const EditorPreviewOptions& options);
int validateEditorCampaign(const std::filesystem::path& path);
int testCampaignEditingExport(const std::filesystem::path& path);
int testLayerEditingExport(const std::filesystem::path& path);
int testCorridorEditingExport(const std::filesystem::path& list);
}
#endif
