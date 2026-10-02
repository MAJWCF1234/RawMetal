# v0.6.4 — Windows Compatibility Patch

The game now explicitly targets Windows 10/11 x64 in its build configuration and
embedded application manifest. The renderer fixes from v0.6.3 are included unchanged.

Install the [Microsoft Visual C++ x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/),
version 14.44 or later, and your GPU vendor's graphics driver. Vulkan rendering
requires Vulkan 1.0; a Vulkan SDK is not required to play. All assets and shaders
remain embedded. `RawMetal.exe --software` selects the CPU renderer.

The compatibility audit checks actual binary architecture, its manifest and its
DLL dependencies. The API audit found no mandatory Windows 11-only functions.
The build retains the 22,000,000-byte limit and lossless asset verification.

Validation was performed on Windows 11 Pro. A Windows 10 hardware playtest is
still needed; this release does not claim that such a playtest has occurred.
See [Windows compatibility](docs/windows-compatibility.md) for requirements and checks.
