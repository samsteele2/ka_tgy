# Build the KA nFET firmware on Windows. Requires AVRA in PATH or the fallback below.

$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$avra = Get-Command avra -ErrorAction SilentlyContinue
if ($null -ne $avra) {
    $avraPath = $avra.Source
}
else {
    $avraPath = Join-Path $env:LOCALAPPDATA 'Programs\AVRA\avra.exe'
}

if (-not (Test-Path -LiteralPath $avraPath -PathType Leaf)) {
    throw 'AVRA was not found. Follow README.md, or add avra.exe to PATH, then run this script again.'
}

Push-Location $repoDir
try {
    & $avraPath -fI -D ka_nfet_esc -I $repoDir -I (Join-Path $repoDir 'other_escs') tgy.asm
    if ($LASTEXITCODE -ne 0) { throw "AVRA failed with exit code $LASTEXITCODE." }

    Move-Item -LiteralPath 'tgy.hex' -Destination 'ka_nfet.hex' -Force
    Remove-Item -LiteralPath 'tgy.eep.hex','tgy.obj','tgy.cof' -Force -ErrorAction SilentlyContinue
}
finally {
    Pop-Location
}

Write-Host "Built: $repoDir\ka_nfet.hex"
