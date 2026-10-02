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
$culture = [Globalization.CultureInfo]::InvariantCulture
$results = foreach ($file in Get-ChildItem -LiteralPath $beforeRoot -Filter '*.ppm') {
    $current = Join-Path $afterRoot $file.Name
    if (-not (Test-Path -LiteralPath $current)) { throw "Missing matching camera: $current" }
    $oldSize = (& magick identify -format '%wx%h' $file.FullName).Trim()
    $newSize = (& magick identify -format '%wx%h' $current).Trim()
    if ($oldSize -ne $newSize) { throw "Resolution changed: $($file.Name)" }
    $count = & magick $file.FullName $current -alpha off -compose difference -composite -fx 'max(r,max(g,b))*255>4.001?1:0' -format '%[fx:mean*w*h]' info:
    if ($LASTEXITCODE -ne 0) { throw 'Pixel comparison failed.' }
    $changed = [math]::Round([double]::Parse($count.Trim(),$culture))
    $errorValue = & magick $file.FullName $current -alpha off -compose difference -composite -format '%[fx:mean*255]' info:
    if ($LASTEXITCODE -ne 0) { throw 'Mean error calculation failed.' }
    $meanError = [double]::Parse($errorValue.Trim(),$culture)
    & magick $current (Join-Path $outputRoot ($file.BaseName+'.png'))
    if ($LASTEXITCODE -ne 0) { throw 'Image conversion failed.' }
    [pscustomobject]@{ View=$file.BaseName; Resolution=$oldSize; PixelsAbove4=$changed; MeanChannelError255=$meanError }
}
$results | ConvertTo-Json | Set-Content (Join-Path $outputRoot 'pixel-comparison.json') -Encoding UTF8
$results | Format-Table -AutoSize
