param(
    [string]$Executable = (Join-Path $PSScriptRoot '../RawMetal.exe'),
    [string]$Dumpbin,
    [string]$Mt
)
$ErrorActionPreference = 'Stop'
if (-not $Dumpbin) {
    $vswhere = "${env:ProgramFiles(x86)}/Microsoft Visual Studio/Installer/vswhere.exe"
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $Dumpbin = Get-ChildItem "$installation/VC/Tools/MSVC/*/bin/Hostx64/x64/dumpbin.exe" |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Mt) {
    $Mt = Get-ChildItem "${env:ProgramFiles(x86)}/Windows Kits/10/bin/*/x64/mt.exe" |
        Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
}
if (-not $Dumpbin -or -not $Mt) { throw 'Visual Studio C++ tools and Windows SDK are required for this developer check.' }
$exePath = (Resolve-Path -LiteralPath $Executable).Path
$headers = & $Dumpbin /headers $exePath
if ($LASTEXITCODE -ne 0) { throw 'Cannot read executable headers.' }
if (-not ($headers -match '8664 machine')) { throw 'Expected an x64 executable.' }
$dependencies = & $Dumpbin /dependents $exePath
if ($LASTEXITCODE -ne 0) { throw 'Cannot read dependencies.' }
$dlls = @($dependencies | ForEach-Object {
    if ($_ -match '^\s+([\w.-]+\.dll)\s*$') { $Matches[1] }
})
$allowed = '^(USER32|GDI32|WINMM|Cabinet|KERNEL32|ADVAPI32|COMCTL32|MSVCP140|VCRUNTIME140(_1)?|vulkan-1)\.dll$|^api-ms-win-crt-[\w-]+\.dll$'
foreach ($dll in $dlls) {
    if ($dll -notmatch $allowed) { throw "Unexpected runtime dependency: $dll. Review Windows 10 availability before shipping." }
}
if ('vulkan-1.dll' -notin $dlls) { throw 'Expected the Vulkan loader dependency.' }
$manifestPath = Join-Path ([IO.Path]::GetTempPath()) ("RawMetal-manifest-" + [guid]::NewGuid() + '.xml')
try {
    & $Mt "-inputresource:$exePath;#1" "-out:$manifestPath" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw 'Cannot extract embedded manifest.' }
    [xml]$manifest = Get-Content -LiteralPath $manifestPath -Raw
    $supported = $manifest.SelectNodes("//*[local-name()='supportedOS']")
    if ('{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}' -notin @($supported | ForEach-Object { $_.Id })) {
        throw 'Missing Windows 10/11 compatibility declaration.'
    }
} finally {
    if (Test-Path -LiteralPath $manifestPath) { Remove-Item -LiteralPath $manifestPath }
}
'PASS: x64 binary; embedded Windows 10/11 manifest; reviewed runtime DLL dependencies.'
'Requires Visual C++ x64 Redistributable 14.44 or later and a Vulkan-capable graphics driver.'
'This binary audit does not replace a Windows 10 runtime playtest or per-function API review.'
$dlls
