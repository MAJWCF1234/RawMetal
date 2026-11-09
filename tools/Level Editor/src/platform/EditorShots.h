#pragma once
#ifdef _WIN32
#include <filesystem>
namespace retro {
// Renders the editor's picture tiles: parts, shapes and materials, one small
// square image each. Every list line is "pack|out.ppm|x|y|eyeZ|yawDegrees|pitch[|size]".
int runEditorShots(const std::filesystem::path& list);
int testEditorLiveRendering();
}
#endif
