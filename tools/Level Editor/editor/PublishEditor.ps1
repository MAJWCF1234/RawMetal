$ErrorActionPreference = 'Stop'
$editorRoot = Split-Path -Parent $PSScriptRoot
$candidate = Join-Path $editorRoot '.build\native-ui\RawMetalEditor.exe'
$destination = Join-Path $editorRoot 'RawMetalEditor.exe'
if (-not (Test-Path -LiteralPath $candidate)) { throw 'Native editor build is missing.' }
try {
    Copy-Item -LiteralPath $candidate -Destination $destination -Force
} catch {
    # Windows permits renaming a running image. Preserve it and its live session;
    # future launches use the repaired build without terminating the old editor.
    if (-not (Test-Path -LiteralPath $destination)) { throw }
    $previous = Join-Path $editorRoot ('.build\native-ui\RawMetalEditor-running-' + [guid]::NewGuid().ToString('N') + '.exe')
    Move-Item -LiteralPath $destination -Destination $previous
    try {
        Copy-Item -LiteralPath $candidate -Destination $destination
    } catch {
        Move-Item -LiteralPath $previous -Destination $destination
        throw
    }
}
Write-Output 'Published native editor; existing sessions remain open.'
