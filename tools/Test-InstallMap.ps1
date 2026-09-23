$ErrorActionPreference = 'Stop'
$sandbox = Join-Path ([IO.Path]::GetTempPath()) ('depthworks-install-test-' + [guid]::NewGuid())
New-Item -ItemType Directory -Path (Join-Path $sandbox 'tools') -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $sandbox 'src/world') -Force | Out-Null
Copy-Item (Join-Path $PSScriptRoot 'InstallMap.ps1'), (Join-Path $PSScriptRoot 'ConvertMapPayload.ps1') (Join-Path $sandbox 'tools')
$installer = Join-Path $sandbox 'tools/InstallMap.ps1'
$world = Join-Path $sandbox 'src/world/World.cpp'
[IO.File]::WriteAllText($world, 'main campaign must remain untouched')
$rows = ((1..24 | ForEach-Object { '"########################"' }) -join ",`n")
$body = @"
--- MAP_CODE_START ---
if(m_level==1){
static constexpr MapRows Ground = {$rows};
m_layers = {{"Ground",0,0,Ground},{"Upper",3.5,0.2,Ground}};
m_creatureSpawns = {{CreatureKind::Brute,{2,3},0},{CreatureKind::Wasp,{4,5},3.5}};
m_pickupSpawns = {{{2,2},PickupKind::Ammo},{{3,3},PickupKind::Health}};
}
--- MAP_CODE_END ---
"@
$payload = Join-Path $sandbox 'payload with spaces.txt'
$encodings = @([Text.UTF8Encoding]::new($false), [Text.UTF8Encoding]::new($true), [Text.Encoding]::Unicode)
foreach($encoding in $encodings) {
    foreach($newline in @("`n", "`r`n")) {
        $metadata = @('META_LEVEL_ID: 1', 'META_LEVEL_NAME: Regression', 'META_CAMPAIGN_NAME: Regression', " `tMETA_DEFAULT_TARGET : custom `t") -join $newline
        [IO.File]::WriteAllText($payload, $metadata + $newline + $body, $encoding)
        $resolved = & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -Payload $payload -Mode Resolve
        if($LASTEXITCODE -ne 0 -or $resolved -ne 'CUSTOM') { throw 'Custom routing failed' }
        & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -Payload $payload -Mode Auto
        if($LASTEXITCODE -ne 0) { throw 'Auto installation failed' }
        $installed = [IO.File]::ReadAllText((Join-Path $sandbox 'custom maps/Regression.txt'))
        foreach($tag in @('LAYER', 'CREATURE', 'PICKUP')) {
            if([regex]::Matches($installed, "(?m)^$tag\|").Count -ne 2) { throw "Lost $tag entries during conversion" }
        }
        if([IO.File]::ReadAllText($world) -ne 'main campaign must remain untouched') { throw 'Custom install changed World.cpp' }
    }
}
[IO.File]::WriteAllText($payload, "META_LEVEL_ID: 6`nMETA_LEVEL_NAME: Main`nMETA_DEFAULT_TARGET: MAIN")
$resolved = & powershell -NoProfile -ExecutionPolicy Bypass -File $installer -Payload $payload -Mode Resolve
if($LASTEXITCODE -ne 0 -or $resolved -ne 'MAIN') { throw 'Main routing failed' }
Write-Host "PASS: routing across UTF-8, BOM, UTF-16, LF/CRLF and whitespace; multi-entry legacy conversion; main source preserved."
# Keep the isolated outputs available for inspection; no game files were used.
Write-Host "Test outputs: $sandbox"
