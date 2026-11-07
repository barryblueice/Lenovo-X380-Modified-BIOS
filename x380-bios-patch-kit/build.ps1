<#
.SYNOPSIS
    Build a patched ThinkPad X380 Yoga (R0SET57W) BIOS image.

.DESCRIPTION
    Wrapper around scripts/build.py. Validates the dump, applies the patch set
    with UEFIPatch, and writes the image plus a JSON manifest. It never flashes.

.EXAMPLE
    .\build.ps1 -InputImage a.bin -Mode both

.EXAMPLE
    .\build.ps1 -InputImage a.bin -Mode wwan -OutDir .\dist
#>
param(
    [Parameter(Mandatory = $true)]
    [string]$InputImage,

    [ValidateSet('both', 'adv', 'wwan')]
    [string]$Mode = 'both',

    [string]$OutDir
)

$ErrorActionPreference = 'Stop'

$script = Join-Path $PSScriptRoot 'scripts\build.py'
$pyArgs = @($script, '--input', $InputImage, '--mode', $Mode)
if ($OutDir) {
    $pyArgs += @('--out-dir', $OutDir)
}

& python @pyArgs
exit $LASTEXITCODE
