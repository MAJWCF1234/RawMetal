[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [string]$Payload,

    [ValidateSet("Info","Main","Custom")]
    [string]$Mode = "Info"
)

$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$worldPath = Join-Path $root "src\world\World.cpp"
$customDir = Join-Path $root "custom maps"

try {
    $payloadPath = (Resolve-Path -LiteralPath $Payload).Path
} catch {
    throw "Payload file not found: $Payload"
}

$payloadText = [IO.File]::ReadAllText($payloadPath)

function Read-Metadata([string]$key) {
    $pattern = "(?m)^\s*" + [regex]::Escape($key) + "\s*:\s*(.*?)\s*$"
    $match = [regex]::Match($payloadText, $pattern)
    if(-not $match.Success) {
        throw "Missing payload metadata: $key"
    }
    return $match.Groups[1].Value.Trim()
}

$levelText = Read-Metadata "META_LEVEL_ID"
$level = 0
if(-not [int]::TryParse($levelText, [ref]$level)) {
    throw "META_LEVEL_ID must be an integer."
}
$name = Read-Metadata "META_LEVEL_NAME"
$defaultTarget = Read-Metadata "META_DEFAULT_TARGET"

if($Mode -eq "Info") {
    Write-Host ("[*] Loaded Payload : " + [IO.Path]::GetFileName($payloadPath))
    Write-Host ("[*] Level Name     : " + $name)
    Write-Host ("[*] Target Slot    : Level " + $level)
    Write-Host ("[*] Default Target : " + $defaultTarget)
    exit 0
}

if($Mode -eq "Custom") {
    if(-not (Test-Path -LiteralPath $customDir)) {
        New-Item -ItemType Directory -Path $customDir | Out-Null
    }
    $safeName = $name -replace '[<>:"/\\|?*]', '_'
    $safeName = ($safeName -replace '\s+', '_').Trim('_')
    if([string]::IsNullOrWhiteSpace($safeName)) {
        $safeName = "Map"
    }
    $destination = Join-Path $customDir ($safeName + "_Slot" + $level + ".txt")
    Copy-Item -LiteralPath $payloadPath -Destination $destination -Force
    Write-Host ("[OK] Custom map archived at " + $destination) -ForegroundColor Green
    exit 0
}

if($level -lt 6) {
    throw "Main-campaign injection currently owns dynamic slots 6 and above. Level $level is part of the hand-authored core campaign."
}
if(-not (Test-Path -LiteralPath $worldPath)) {
    throw "World.cpp not found at $worldPath"
}

$codeMatch = [regex]::Match(
    $payloadText,
    '(?s)--- MAP_CODE_START ---\s*(.*?)\s*--- MAP_CODE_END ---'
)
if(-not $codeMatch.Success) {
    throw "Payload is missing MAP_CODE_START / MAP_CODE_END."
}
$code = $codeMatch.Groups[1].Value.Trim()

$levelPattern = "(?m)^\s*if\s*\(\s*m_level\s*==\s*" + $level + "\s*\)\s*\{"
if(-not [regex]::IsMatch($code, $levelPattern)) {
    throw "Payload code does not declare if(m_level==$level). Metadata and map code must target the same slot."
}

$world = [IO.File]::ReadAllText($worldPath)
$dynamicStart = "// === DYNAMIC_CAMPAIGN_MAPS_START ==="
$dynamicEnd = "// === DYNAMIC_CAMPAIGN_MAPS_END ==="
if(([regex]::Matches($world, [regex]::Escape($dynamicStart))).Count -ne 1 -or
   ([regex]::Matches($world, [regex]::Escape($dynamicEnd))).Count -ne 1) {
    throw "World.cpp does not contain exactly one dynamic campaign map region."
}

$startTag = "// === LEVEL_" + $level + "_START ==="
$endTag = "// === LEVEL_" + $level + "_END ==="
$startCount = ([regex]::Matches($world, [regex]::Escape($startTag))).Count
$endCount = ([regex]::Matches($world, [regex]::Escape($endTag))).Count
if($startCount -ne $endCount -or $startCount -gt 1) {
    throw "Level $level slot markers are malformed."
}

$backup = $worldPath + ".bak"
Copy-Item -LiteralPath $worldPath -Destination $backup -Force

try {
    if($startCount -eq 1) {
        $slotPattern = [regex]::Escape($startTag) + '(?s:.*?)' + [regex]::Escape($endTag)
        $replacement = $startTag + "`r`n" + $code + "`r`n  " + $endTag
        $slotRegex = [regex]::new($slotPattern)
        $world = $slotRegex.Replace(
            $world,
            [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $replacement },
            1
        )
        Write-Host ("[OK] Updated existing slot for Level " + $level + ".") -ForegroundColor Green
    } else {
        $markerRegex = [regex]::new('(?m)^(\s*)' + [regex]::Escape($dynamicEnd) + '\s*$')
        $markerMatch = $markerRegex.Match($world)
        if(-not $markerMatch.Success) {
            throw "Dynamic campaign end marker disappeared while installing."
        }
        $indent = $markerMatch.Groups[1].Value
        $insert = $indent + $startTag + "`r`n" + $code + "`r`n" +
                  $indent + $endTag + "`r`n" + $indent + $dynamicEnd
        $world = $markerRegex.Replace(
            $world,
            [System.Text.RegularExpressions.MatchEvaluator]{ param($m) $insert },
            1
        )
        Write-Host ("[OK] Created new slot for Level " + $level + ".") -ForegroundColor Green
    }

    $utf8 = New-Object System.Text.UTF8Encoding($false)
    [IO.File]::WriteAllText($worldPath, $world, $utf8)
} catch {
    Copy-Item -LiteralPath $backup -Destination $worldPath -Force
    throw
}

Write-Host ("[OK] World.cpp patched. Backup: " + $backup) -ForegroundColor Green
