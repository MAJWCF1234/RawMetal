Set-StrictMode -Version Latest

$script:Invariant = [Globalization.CultureInfo]::InvariantCulture

function Convert-MapScalar {
    param(
        [Parameter(Mandatory=$true)][string]$Text,
        [double]$Roof = 0
    )
    $expr = $Text.Trim()
    $expr = [regex]::Replace($expr, '(?i)(?<=[0-9.])f\b', '')
    $expr = [regex]::Replace($expr, '\bkPi\b', [Math]::PI.ToString("R", $script:Invariant))
    $expr = [regex]::Replace($expr, '\broof\b', $Roof.ToString("R", $script:Invariant))
    if($expr -notmatch '^[0-9.+\-*/()\s]+$') {
        throw "Unsupported numeric expression in legacy custom payload: $Text"
    }
    try {
        $value = Invoke-Expression $expr
        return [Convert]::ToDouble($value, $script:Invariant)
    } catch {
        throw "Could not evaluate numeric expression in legacy custom payload: $Text"
    }
}

function Format-MapNumber {
    param([double]$Value)
    if([Math]::Abs($Value) -lt 0.0000005) { $Value = 0 }
    return $Value.ToString("0.######", $script:Invariant)
}

function Convert-MapBool {
    param([string]$Text)
    $value = $Text.Trim().ToLowerInvariant()
    if($value -eq "true" -or $value -eq "1") { return $true }
    if($value -eq "false" -or $value -eq "0") { return $false }
    throw "Unsupported boolean expression in legacy custom payload: $Text"
}

function Escape-RuntimeField {
    param([string]$Text)
    return [Uri]::EscapeDataString([string]$Text)
}

function Unquote-CppString {
    param([string]$Text)
    $value = $Text.Trim()
    if($value.Length -lt 2 -or $value[0] -ne '"' -or $value[$value.Length-1] -ne '"') {
        throw "Expected a quoted string in legacy custom payload: $Text"
    }
    $value = $value.Substring(1, $value.Length-2)
    $value = $value.Replace('\"','"').Replace('\\','\')
    return $value
}

function Split-TopLevel {
    param([string]$Text)
    $result = New-Object System.Collections.Generic.List[string]
    $start = 0
    $brace = 0
    $paren = 0
    $bracket = 0
    $quote = $false
    $escape = $false
    for($i=0; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if($quote) {
            if($escape) { $escape = $false; continue }
            if($ch -eq '\') { $escape = $true; continue }
            if($ch -eq '"') { $quote = $false }
            continue
        }
        if($ch -eq '"') { $quote = $true; continue }
        switch($ch) {
            '{' { $brace++ }
            '}' { $brace-- }
            '(' { $paren++ }
            ')' { $paren-- }
            '[' { $bracket++ }
            ']' { $bracket-- }
            ',' {
                if($brace -eq 0 -and $paren -eq 0 -and $bracket -eq 0) {
                    $result.Add($Text.Substring($start, $i-$start).Trim())
                    $start = $i+1
                }
            }
        }
    }
    $tail = $Text.Substring($start).Trim()
    if($tail.Length) { $result.Add($tail) }
    return ,$result.ToArray()
}

function Get-BraceEntries {
    param([string]$Text)
    $result = New-Object System.Collections.Generic.List[string]
    $depth = 0
    $start = -1
    $quote = $false
    $escape = $false
    for($i=0; $i -lt $Text.Length; $i++) {
        $ch = $Text[$i]
        if($quote) {
            if($escape) { $escape=$false; continue }
            if($ch -eq '\') { $escape=$true; continue }
            if($ch -eq '"') { $quote=$false }
            continue
        }
        if($ch -eq '"') { $quote=$true; continue }
        if($ch -eq '{') {
            if($depth -eq 0) { $start=$i+1 }
            $depth++
        } elseif($ch -eq '}') {
            $depth--
            if($depth -eq 0 -and $start -ge 0) {
                $result.Add($Text.Substring($start, $i-$start).Trim())
                $start=-1
            }
            if($depth -lt 0) { throw "Malformed brace initializer in legacy custom payload." }
        }
    }
    if($depth -ne 0) { throw "Unbalanced brace initializer in legacy custom payload." }
    return ,$result.ToArray()
}

function Get-AssignmentBody {
    param([string]$Code,[string]$Name)
    $pattern = '(?s)\b' + [regex]::Escape($Name) + '\s*=\s*\{(.*?)\};'
    $match = [regex]::Match($Code, $pattern)
    if($match.Success) { return $match.Groups[1].Value }
    return $null
}

function Parse-Vec2 {
    param([string]$Text,[double]$Roof)
    $value = $Text.Trim()
    if($value.StartsWith("Vec2")) { $value=$value.Substring(4).Trim() }
    if($value.StartsWith("{") -and $value.EndsWith("}")) { $value=$value.Substring(1,$value.Length-2) }
    $f = Split-TopLevel $value
    if($f.Count -ne 2) { throw "Expected Vec2 in legacy custom payload: $Text" }
    return @((Convert-MapScalar $f[0] $Roof),(Convert-MapScalar $f[1] $Roof))
}

function Convert-LegacyCustomPayload {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)][string]$PayloadText,
        [Parameter(Mandatory=$true)][string]$CampaignName,
        [Parameter(Mandatory=$true)][string]$MapName
    )

    $codeMatch = [regex]::Match($PayloadText, '(?s)--- MAP_CODE_START ---\s*(.*?)\s*--- MAP_CODE_END ---')
    if(-not $codeMatch.Success) { throw "CUSTOM payload has neither runtime campaign data nor a MAP_CODE block to convert." }
    $code = $codeMatch.Groups[1].Value

    foreach($unsupported in @('event\s*\(', '\bm_scriptEvents\b', '\bm_hazards\b', '\bm_compactors\b', '\bm_waterVolumes\b')) {
        if([regex]::IsMatch($code,$unsupported)) {
            throw "Legacy CUSTOM conversion found advanced runtime content ('$unsupported') that cannot be translated safely. Export this campaign from the Level Editor so the TXT contains CUSTOM_CAMPAIGN_DATA."
        }
    }

    $rowTables = @{}
    $rowMatches = [regex]::Matches($code, '(?s)static\s+constexpr\s+MapRows\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\{(.*?)\};')
    foreach($match in $rowMatches) {
        $name = $match.Groups[1].Value
        $rows = New-Object System.Collections.Generic.List[string]
        foreach($rowMatch in [regex]::Matches($match.Groups[2].Value, '"([^"]*)"')) {
            $rows.Add($rowMatch.Groups[1].Value)
        }
        if($rows.Count -ne 24) { throw "MapRows $name must contain exactly 24 rows for runtime custom maps." }
        for($rowIndex=0; $rowIndex -lt $rows.Count; $rowIndex++) {
            $row = $rows[$rowIndex]
            if($row.Length -gt 24) { throw "MapRows $name row $rowIndex is wider than 24 characters." }
            if($row.Length -lt 24) {
                $fill = if($row.StartsWith("_")) { "_" } elseif($row.StartsWith("#") -or $row.EndsWith("#")) { "#" } else { "." }
                Write-Warning "MapRows $name row $rowIndex is only $($row.Length) characters. CUSTOM compatibility import padded the right edge with '$fill' to 24."
                $rows[$rowIndex] = $row.PadRight(24, [char]$fill)
            }
        }
        $rowTables[$name] = $rows.ToArray()
    }

    $layerBody = Get-AssignmentBody $code 'm_layers'
    if([string]::IsNullOrWhiteSpace($layerBody)) { throw "Legacy CUSTOM payload does not define m_layers." }
    $layers = New-Object System.Collections.Generic.List[object]
    foreach($entry in Get-BraceEntries $layerBody) {
        $f = Split-TopLevel $entry
        if($f.Count -lt 4) { throw "Malformed m_layers entry in legacy CUSTOM payload." }
        $layerName = Unquote-CppString $f[0]
        $z = Convert-MapScalar $f[1]
        $thickness = Convert-MapScalar $f[2]
        $rowsName = $f[3].Trim()
        if(-not $rowTables.ContainsKey($rowsName)) { throw "Layer '$layerName' references unknown MapRows '$rowsName'." }
        $layers.Add([pscustomobject]@{ Name=$layerName; Z=$z; Thickness=$thickness; Rows=$rowTables[$rowsName] })
    }
    if($layers.Count -eq 0) { throw "Legacy CUSTOM payload has no layers." }
    $baseZ = ($layers | Measure-Object -Property Z -Minimum).Minimum
    $roof = (($layers | Measure-Object -Property Z -Maximum).Maximum) + 3.5

    $doors = New-Object System.Collections.Generic.List[object]
    $doorBody = Get-AssignmentBody $code 'm_doors'
    if(-not [string]::IsNullOrWhiteSpace($doorBody)) {
        foreach($entry in Get-BraceEntries $doorBody) {
            $f = Split-TopLevel $entry
            if($f.Count -lt 3) { throw "Malformed m_doors entry in legacy CUSTOM payload." }
            $door = [pscustomobject]@{
                Left=(Convert-MapScalar $f[0] $roof); Right=(Convert-MapScalar $f[1] $roof); Y=(Convert-MapScalar $f[2] $roof)
                Z=if($f.Count -ge 8){Convert-MapScalar $f[7] $roof}else{0.0}
                Entry=if($f.Count -ge 7){Convert-MapBool $f[6]}else{$false}
                Transfer=if($f.Count -ge 6){Convert-MapBool $f[5]}else{$false}
                Swinging=$false; Clear=$false; Sign=-1; State=""; RequireValue=1
            }
            $doors.Add($door)
        }
    }
    $backState = [regex]::Match($code, 'm_doors\.back\(\)\.requireState\s*=\s*stateId\(\s*"([^"]+)"\s*\)\s*;')
    if($backState.Success -and $doors.Count) { $doors[$doors.Count-1].State=$backState.Groups[1].Value }
    foreach($m in [regex]::Matches($code, 'm_doors\[(\d+)\]\.requireState\s*=\s*stateId\(\s*"([^"]+)"\s*\)\s*;')) {
        $index=[int]$m.Groups[1].Value;if($index -ge 0 -and $index -lt $doors.Count){$doors[$index].State=$m.Groups[2].Value}
    }
    if([regex]::IsMatch($code,'m_doors\.back\(\)\.swinging\s*=\s*true\s*;') -and $doors.Count){$doors[$doors.Count-1].Swinging=$true}
    foreach($m in [regex]::Matches($code, 'm_doors\[(\d+)\]\.swinging\s*=\s*(true|false)\s*;')) {
        $index=[int]$m.Groups[1].Value;if($index -ge 0 -and $index -lt $doors.Count){$doors[$index].Swinging=Convert-MapBool $m.Groups[2].Value}
    }

    $spawnX=3.5;$spawnY=3.5;$spawnZ=[double]$baseZ
    $entryDoor=$doors | Where-Object Entry | Select-Object -First 1
    if($null -ne $entryDoor) {
        $spawnX=($entryDoor.Left+$entryDoor.Right)/2
        $spawnY=if($entryDoor.Y -lt 12){[Math]::Min(23.5,$entryDoor.Y+1.0)}else{[Math]::Max(.5,$entryDoor.Y-1.0)}
        $spawnZ=$baseZ+$entryDoor.Z
    } else {
        $ground=$layers | Sort-Object Z | Select-Object -First 1
        $found=$false
        for($y=0;$y -lt 24 -and -not $found;$y++) {
            for($x=0;$x -lt 24;$x++) {
                $ch=$ground.Rows[$y][$x]
                if($ch -eq '.' -or $ch -eq 'X') {$spawnX=$x+.5;$spawnY=$y+.5;$found=$true;break}
            }
        }
    }

    $out = New-Object System.Collections.Generic.List[string]
    $out.Add("CAMPAIGN|$(Escape-RuntimeField $CampaignName)|0")
    $out.Add("MAP|0|$(Escape-RuntimeField $MapName)|0|0|$(Format-MapNumber $spawnX)|$(Format-MapNumber $spawnY)|$(Format-MapNumber $spawnZ)|0|$(Format-MapNumber $roof)|0.27|industrial_night|0|0|0|0|0")
    for($li=0;$li -lt $layers.Count;$li++) {
        $layer=$layers[$li]
        $out.Add("LAYER|0|$li|$(Escape-RuntimeField $layer.Name)|$(Format-MapNumber $layer.Z)|$(Format-MapNumber $layer.Thickness)")
        for($y=0;$y -lt 24;$y++) {$out.Add("ROW|0|$li|$y|$($layer.Rows[$y])")}
    }
    foreach($door in $doors) {
        $out.Add("DOOR|0|$(Format-MapNumber $door.Left)|$(Format-MapNumber $door.Right)|$(Format-MapNumber $door.Y)|$(Format-MapNumber $door.Z)|$([int]$door.Entry)|$([int]$door.Transfer)|$([int]$door.Swinging)|$([int]$door.Clear)|$($door.Sign)|$(Escape-RuntimeField $door.State)|$($door.RequireValue)")
    }

    foreach($m in [regex]::Matches($code,'wall\s*\(\s*([^)]+)\)\s*;')) {
        $f=Split-TopLevel $m.Groups[1].Value;if($f.Count -ne 6){throw "wall(...) in legacy CUSTOM payload must have six scalar arguments."}
        $v=@();foreach($part in $f){$v+=Convert-MapScalar $part $roof}
        $out.Add("STRUCT|0|$(Format-MapNumber $v[0])|$(Format-MapNumber $v[1])|$(Format-MapNumber $v[2])|$(Format-MapNumber $v[3])|$(Format-MapNumber $v[4])|$(Format-MapNumber $v[5])|0|3")
    }

    $stairsBody=Get-AssignmentBody $code 'stairs'
    if(-not [string]::IsNullOrWhiteSpace($stairsBody)) {
        foreach($entry in Get-BraceEntries $stairsBody) {
            $f=Split-TopLevel $entry;if($f.Count -lt 9){throw "Malformed stairs entry in legacy CUSTOM payload."}
            $out.Add("STAIR|0|$(Format-MapNumber (Convert-MapScalar $f[0] $roof))|$(Format-MapNumber (Convert-MapScalar $f[1] $roof))|$(Format-MapNumber (Convert-MapScalar $f[2] $roof))|$(Format-MapNumber (Convert-MapScalar $f[3] $roof))|$(Format-MapNumber (Convert-MapScalar $f[4] $roof))|$(Format-MapNumber (Convert-MapScalar $f[5] $roof))|$([int](Convert-MapScalar $f[6] $roof))|$([int](Convert-MapBool $f[7]))|$([int](Convert-MapBool $f[8]))")
        }
    }

    foreach($m in [regex]::Matches($code,'m_pipes\.push_back\s*\(\s*\{\s*\{([^{}]+)\}\s*,\s*\{([^{}]+)\}\s*,\s*([^,{}]+)\s*,\s*([^,{}]+)(?:\s*,\s*([^{}]+))?\s*\}\s*\)\s*;')) {
        $a=Parse-Vec2 $m.Groups[1].Value $roof;$b=Parse-Vec2 $m.Groups[2].Value $roof
        $z=Convert-MapScalar $m.Groups[3].Value $roof;$radius=Convert-MapScalar $m.Groups[4].Value $roof
        $endZ=if($m.Groups[5].Success){Convert-MapScalar $m.Groups[5].Value $roof}else{-999}
        $out.Add("PIPE|0|$(Format-MapNumber $a[0])|$(Format-MapNumber $a[1])|$(Format-MapNumber $b[0])|$(Format-MapNumber $b[1])|$(Format-MapNumber $z)|$(Format-MapNumber $radius)|$(Format-MapNumber $endZ)")
    }

    foreach($m in [regex]::Matches($code,'(?s)m_fixtures\.push_back\s*\(\s*\{(.*?)\}\s*\)\s*;')) {
        $f=Split-TopLevel $m.Groups[1].Value;if($f.Count -lt 8){throw "Malformed m_fixtures.push_back entry in legacy CUSTOM payload."}
        $p=Parse-Vec2 $f[1] $roof
        $out.Add("FIXTURE|0|$([int](Convert-MapScalar $f[0] $roof))|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|$(Format-MapNumber (Convert-MapScalar $f[2] $roof))|$(Format-MapNumber (Convert-MapScalar $f[3] $roof))|$(Format-MapNumber (Convert-MapScalar $f[4] $roof))|$(Format-MapNumber (Convert-MapScalar $f[5] $roof))|$(Format-MapNumber (Convert-MapScalar $f[6] $roof))|$([int](Convert-MapBool $f[7]))")
    }
    foreach($m in [regex]::Matches($code,'shelf\s*\(\s*\{([^{}]+)\}\s*(?:,\s*([^,\)]+))?(?:,\s*([^\)]+))?\)\s*;')) {
        $p=Parse-Vec2 $m.Groups[1].Value $roof;$yaw=if($m.Groups[2].Success){Convert-MapScalar $m.Groups[2].Value $roof}else{0};$z=if($m.Groups[3].Success){Convert-MapScalar $m.Groups[3].Value $roof}else{0}
        $out.Add("FIXTURE|0|7|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|$(Format-MapNumber $z)|1.8|0.5|1.8|$(Format-MapNumber $yaw)|1")
    }
    foreach($m in [regex]::Matches($code,'cabinet\s*\(\s*\{([^{}]+)\}\s*(?:,\s*([^\)]+))?\)\s*;')) {
        $p=Parse-Vec2 $m.Groups[1].Value $roof;$yaw=if($m.Groups[2].Success){Convert-MapScalar $m.Groups[2].Value $roof}else{0}
        $out.Add("FIXTURE|0|13|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|0|0.9066|0.4956|2.2|$(Format-MapNumber $yaw)|1")
    }
    foreach($m in [regex]::Matches($code,'tank\s*\(\s*\{([^{}]+)\}\s*,\s*([^\)]+)\)\s*;')) {
        $p=Parse-Vec2 $m.Groups[1].Value $roof;$h=Convert-MapScalar $m.Groups[2].Value $roof;$scale=$h/2.390135
        $out.Add("FIXTURE|0|14|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|0|$(Format-MapNumber (2.612115*$scale))|$(Format-MapNumber (2.874012*$scale))|$(Format-MapNumber $h)|0|1")
    }

    $terminalBody=Get-AssignmentBody $code 'm_terminals'
    if(-not [string]::IsNullOrWhiteSpace($terminalBody)) {
        foreach($entry in Get-BraceEntries $terminalBody) {
            $f=Split-TopLevel $entry;if($f.Count -lt 6){throw "Malformed terminal entry in legacy CUSTOM payload."}
            $p=Parse-Vec2 $f[0] $roof;$title=Unquote-CppString $f[1];$line1=Unquote-CppString $f[2];$line2=Unquote-CppString $f[3]
            $z=Convert-MapScalar $f[4] $roof;$control=Convert-MapBool $f[5];$state="";$toggle=$false
            if($f.Count -ge 8) {$sm=[regex]::Match($f[7],'stateId\(\s*"([^"]+)"\s*\)');if($sm.Success){$state=$sm.Groups[1].Value}}
            if($f.Count -ge 9) {$toggle=Convert-MapBool $f[8]}
            $out.Add("TERMINAL|0|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|$(Format-MapNumber $z)|$([int]$control)|$(Escape-RuntimeField $title)|$(Escape-RuntimeField $line1)|$(Escape-RuntimeField $line2)|$(Escape-RuntimeField $state)|$([int]$toggle)")
        }
    }

    $creatureBody=Get-AssignmentBody $code 'm_creatureSpawns'
    if(-not [string]::IsNullOrWhiteSpace($creatureBody)) {
        $kindMap=@{Huntsman=0;Wasp=1;Brute=2;Warden=3}
        foreach($entry in Get-BraceEntries $creatureBody) {
            $f=Split-TopLevel $entry;if($f.Count -lt 3){throw "Malformed creature spawn in legacy CUSTOM payload."}
            $kindName=($f[0] -replace '^CreatureKind::','').Trim();if(-not $kindMap.ContainsKey($kindName)){throw "Unsupported creature kind: $kindName"}
            $p=Parse-Vec2 $f[1] $roof;$z=Convert-MapScalar $f[2] $roof
            $out.Add("CREATURE|0|$($kindMap[$kindName])|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|$(Format-MapNumber $z)")
        }
    }

    $pickupBody=Get-AssignmentBody $code 'm_pickupSpawns'
    if(-not [string]::IsNullOrWhiteSpace($pickupBody)) {
        foreach($entry in Get-BraceEntries $pickupBody) {
            $f=Split-TopLevel $entry;if($f.Count -lt 2){throw "Malformed pickup spawn in legacy CUSTOM payload."}
            $p=Parse-Vec2 $f[0] $roof;$kind=if($f[1].Trim() -eq 'PickupKind::Health'){0}elseif($f[1].Trim() -eq 'PickupKind::Ammo'){1}else{throw "Unsupported pickup kind: $($f[1])"}
            $out.Add("PICKUP|0|$kind|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])")
        }
    }

    $clutterBody=Get-AssignmentBody $code 'm_clutterSpawns'
    if(-not [string]::IsNullOrWhiteSpace($clutterBody)) {
        foreach($entry in Get-BraceEntries $clutterBody) {
            $f=Split-TopLevel $entry;if($f.Count -lt 2){throw "Malformed clutter spawn in legacy CUSTOM payload."}
            $p=Parse-Vec2 $f[1] $roof;$z=if($f.Count -ge 3){Convert-MapScalar $f[2] $roof}else{-999};$yaw=if($f.Count -ge 4){Convert-MapScalar $f[3] $roof}else{0}
            $out.Add("CLUTTER|0|$([int](Convert-MapScalar $f[0] $roof))|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|$(Format-MapNumber $z)|$(Format-MapNumber $yaw)")
        }
    }

    foreach($m in [regex]::Matches($code,'m_lights\.push_back\s*\(\s*\{\s*\{([^{}]+)\}\s*,\s*([^{}]+)\}\s*\)\s*;')) {
        $p=Parse-Vec2 $m.Groups[1].Value $roof;$z=Convert-MapScalar $m.Groups[2].Value $roof
        $out.Add("LIGHT|0|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|$(Format-MapNumber $z)")
    }
    foreach($m in [regex]::Matches($code,'(?s)for\s*\(\s*Vec2\s+p\s*:\s*\{(.*?)\}\s*\)\s*m_lights\.push_back\s*\(\s*\{\s*p\s*,\s*([^)]+)\}\s*\)\s*;')) {
        $z=Convert-MapScalar $m.Groups[2].Value $roof
        foreach($vm in [regex]::Matches($m.Groups[1].Value,'Vec2\s*\{([^{}]+)\}')) {
            $p=Parse-Vec2 $vm.Groups[1].Value $roof
            $out.Add("LIGHT|0|$(Format-MapNumber $p[0])|$(Format-MapNumber $p[1])|$(Format-MapNumber $z)")
        }
    }

    return ($out -join [Environment]::NewLine)
}
