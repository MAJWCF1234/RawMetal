param(
    [Parameter(Mandatory=$true)][string]$Before,
    [Parameter(Mandatory=$true)][string]$After,
    [Parameter(Mandatory=$true)][string]$Output
)
$ErrorActionPreference = 'Stop'
$beforeRoot = (Resolve-Path -LiteralPath $Before).Path
$afterRoot = (Resolve-Path -LiteralPath $After).Path
New-Item -ItemType Directory -Path $Output -Force | Out-Null
$outputRoot = (Resolve-Path -LiteralPath $Output).Path
$results = foreach ($file in Get-ChildItem -LiteralPath $beforeRoot -Filter '*.png') {
    if ($file.BaseName -notmatch '^(annex|junction|waste)-') { continue }
    $current = Join-Path $afterRoot $file.Name
    if (-not (Test-Path -LiteralPath $current)) { throw "Missing matching view: $current" }
    $oldSize = (& magick identify -format '%wx%h' $file.FullName).Trim()
    $newSize = (& magick identify -format '%wx%h' $current).Trim()
    if ($oldSize -ne '640x360' -or $newSize -ne $oldSize) { throw "Unexpected frame dimensions for $($file.Name)" }
    # Compare identical cameras, with the top and bottom HUD bands excluded.
    # Any changed RGB channel counts once per pixel; this is change evidence,
    # not a score for visual quality or playtester satisfaction.
    $meanText = & magick $file.FullName $current -compose difference -composite -crop '640x280+0+40' +repage -fx '(r+g+b)>0?1:0' -format '%[fx:mean]' info:
    if ($LASTEXITCODE -ne 0) { throw "Comparison failed for $($file.Name)" }
    $mean = [double]::Parse($meanText.Trim(), [Globalization.CultureInfo]::InvariantCulture)
    [pscustomobject]@{
        View = $file.BaseName
        ChangedPixels = [Math]::Round($mean * 640 * 280)
        ComparedPixels = 640 * 280
        ChangedPercent = [Math]::Round($mean * 100, 2)
    }
    & magick montage $file.FullName $current -tile '2x1' -geometry '640x360+6+6' -background '#171614' (Join-Path $outputRoot "$($file.BaseName)-before-after.png")
    if ($LASTEXITCODE -ne 0) { throw "Comparison image failed for $($file.Name)" }
}
$results | Export-Csv -LiteralPath (Join-Path $outputRoot 'pixel-comparison.csv') -NoTypeInformation
$results | Format-Table -AutoSize
