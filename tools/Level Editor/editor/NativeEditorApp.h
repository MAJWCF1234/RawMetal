#pragma once
#ifdef _WIN32
#include <windows.h>
// One program: the big menu opens the editor. Play inside the editor fills playFile and returns 2.
int runNativeEditor(HINSTANCE instance);
int runEditorFromMenu(HINSTANCE instance, bool fresh, const wchar_t* project, wchar_t* playFile, int playFileChars);
#endif
