# Windows 10 compatibility

RawMetal targets Windows 10 and Windows 11 **x64**. The build explicitly sets
`WINVER` and `_WIN32_WINNT` to `0x0A00`, and embeds Microsoft's shared Windows
10/11 `supportedOS` identity. This declaration does not itself prove compatibility.

## Player requirements

- Windows 10 x64 or Windows 11 x64.
- [Microsoft Visual C++ x64 Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/), version **14.44 or later**.
- A GPU driver supporting Vulkan 1.0 and Win32 presentation. The vendor's driver
  supplies `vulkan-1.dll`; the Vulkan SDK is only needed to build the game.

Assets and shaders are embedded, but the C++ runtime is dynamically linked.
Missing `MSVCP140.dll`, `VCRUNTIME140.dll`, or `VCRUNTIME140_1.dll` means the x64
Redistributable must be installed. Do not obtain individual DLLs from download sites.
The `--software` option remains available when hardware rendering is unavailable;
it still requires the C++ runtime.

## Verification

Run `powershell -File tools/Check-WindowsCompatibility.ps1` after a Release build.
It checks x64 architecture, the actual embedded compatibility manifest and the
runtime DLL dependency list. Unexpected dependencies fail the check. Developers
must also review new imported functions; a compatible DLL can export newer APIs.

The 0.6.3 import audit found no Windows 11-only mandatory APIs. Windowing uses
established Win32 calls; timing, threads, filesystem and compression calls are
available on Windows 10. Vulkan requests API 1.0 and compiles shaders for 1.0.
The required C++ runtime supports Windows 10, per Microsoft's documentation.

Local validation uses **Windows 11 Pro**, not Windows 10. Binary inspection and
successful local tests do not certify Windows 10 execution. The remaining runtime
check is a Windows 10 x64 playtest covering startup, audio, saving/loading,
fullscreen Vulkan, and the software fallback on representative hardware.

References: [Microsoft manifests](https://learn.microsoft.com/en-us/windows/win32/sbscs/application-manifests),
[Microsoft runtime requirements](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist/).
