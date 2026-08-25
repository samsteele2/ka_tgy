# Flash a reviewed KA nFET firmware image with a USBasp.
# Program the board-specific clock and boot fuses on every flash. The lock byte
# is intentionally left unchanged.

$ErrorActionPreference = 'Stop'
$repoDir = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$hexPath = Join-Path $repoDir 'ka_nfet.hex'
$lowFuse = '0x3F'
$highFuse = '0xCA'

function Find-Avrdude {
    if ($env:AVRDUDE_EXE) {
        if (Test-Path -LiteralPath $env:AVRDUDE_EXE -PathType Leaf) {
            return (Resolve-Path -LiteralPath $env:AVRDUDE_EXE).Path
        }
        throw "AVRDUDE_EXE points to a missing file: $env:AVRDUDE_EXE"
    }

    # Prefer a complete known installation over PATH. Some Windows AVR tools
    # put an avrdude.exe and an incompatible avrdude.conf in different PATH
    # locations, which produces a build/config version mismatch and omits
    # programmer definitions such as usbasp.
    $candidates = @(
        (Join-Path $repoDir 'tools\avrdude\avrdude.exe'),
        (Join-Path $env:LOCALAPPDATA 'Programs\AVRDUDE\avrdude.exe'),
        (Join-Path $env:ProgramFiles 'AVRDUDE\avrdude.exe'),
        (Join-Path $env:ProgramFiles 'AVRDUDESS\avrdude.exe')
    )
    if (${env:ProgramFiles(x86)}) {
        $candidates += Join-Path ${env:ProgramFiles(x86)} 'AVRDUDE\avrdude.exe'
        $candidates += Join-Path ${env:ProgramFiles(x86)} 'AVRDUDESS\avrdude.exe'
    }

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $command = Get-Command avrdude -CommandType Application -ErrorAction SilentlyContinue
    if ($null -ne $command) {
        return $command.Source
    }

    throw @'
AVRDUDE was not found. Install it or set AVRDUDE_EXE to the full path to
avrdude.exe. A repo-local installation is also recognized at:
  tools\avrdude\avrdude.exe
'@
}

if (-not (Test-Path -LiteralPath $hexPath -PathType Leaf)) {
    throw "Firmware image not found: $hexPath"
}

$avrdudePath = Find-Avrdude
$avrdudeConfig = Join-Path (Split-Path -Parent $avrdudePath) 'avrdude.conf'
if (-not (Test-Path -LiteralPath $avrdudeConfig -PathType Leaf)) {
    throw "Matching AVRDUDE configuration was not found: $avrdudeConfig"
}
Write-Host "Using AVRDUDE: $avrdudePath"
Write-Host "Using configuration: $avrdudeConfig"
Write-Host "Writing flash, low fuse $lowFuse, and high fuse $highFuse."
Write-Host 'The lock byte is unchanged.'

& $avrdudePath -C $avrdudeConfig -c usbasp -p m8 `
    -U "flash:w:${hexPath}:i" `
    -U "lfuse:w:${lowFuse}:m" `
    -U "hfuse:w:${highFuse}:m"
if ($LASTEXITCODE -ne 0) {
    throw "AVRDUDE flash/fuse write failed with exit code $LASTEXITCODE."
}

Write-Host 'Verifying flash and fuse bytes...'
& $avrdudePath -C $avrdudeConfig -c usbasp -p m8 `
    -U "flash:v:${hexPath}:i" `
    -U "lfuse:v:${lowFuse}:m" `
    -U "hfuse:v:${highFuse}:m"
if ($LASTEXITCODE -ne 0) {
    throw "AVRDUDE verification failed with exit code $LASTEXITCODE."
}
