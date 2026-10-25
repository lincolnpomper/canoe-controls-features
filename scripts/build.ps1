param(
    [Parameter(Mandatory = $true)]
    [string]$Zig
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$source = Join-Path $repoRoot 'src\canoe_analog_state.c'
$pauseSource = Join-Path $repoRoot 'src\canoe_pause.c'
$fastSource = Join-Path $repoRoot 'src\canoe_fast_forward.c'
$pacerSource = Join-Path $repoRoot 'src\frame_pacer.c'
$scanlineSource = Join-Path $repoRoot 'src\canoe_scanline.c'
$scanlineTransformSource = Join-Path $repoRoot 'src\canoe_scanline_transform.c'
$output = Join-Path $repoRoot 'dist\libcanoe_analog_state.so'

# SNES Classic Mini: ARMv7, hard-float Linux userspace compatible with glibc 2.17.
& $Zig cc `
    -target arm-linux-gnueabihf.2.17 `
    -mcpu=cortex_a7 -marm -shared -fPIC -O2 -s -Wall -Wextra -Werror `
    -o $output $source $pauseSource $fastSource $pacerSource $scanlineSource $scanlineTransformSource -ldl

if ($LASTEXITCODE -ne 0) {
    throw 'Zig failed to build the library.'
}

Get-FileHash -LiteralPath $output -Algorithm SHA256
