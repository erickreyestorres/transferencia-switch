param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$distNro = Join-Path $root "dist\transferencia_switch.nro"
$fixtureDir = Join-Path $root "_local\fixtures\smoke"
$largeXci = Join-Path $fixtureDir "large-smoke.xci"
$paddedXci = Join-Path $fixtureDir "padded-smoke.xci"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

Push-Location $root
try {
    Write-Step "Pruebas Python"
    python -m unittest discover -s tests -q

    Write-Step "Pruebas C del núcleo Switch"
    powershell -ExecutionPolicy Bypass -File .\scripts\test_switch_core.ps1

    Write-Step "Simulación MTP local con XCI sintéticos"
    New-Item -ItemType Directory -Force -Path $fixtureDir | Out-Null
    python .\tools\package_fixtures.py make-xci $largeXci --secure-payload-size 5368709120 | Out-Host
    $largeSimulation = python .\tools\mtp_stream_simulator.py $largeXci --unknown-size --chunk-size 524288
    $largeSimulation | Out-Host
    if ($LASTEXITCODE -ne 0 -or -not ($largeSimulation -contains "succeeded=True")) {
        throw "La simulación MTP de XCI grande falló"
    }

    python .\tools\package_fixtures.py make-xci $paddedXci --secure-payload-size 2097152 --trailing-padding 4096 | Out-Host
    $paddedSimulation = python .\tools\mtp_stream_simulator.py $paddedXci --unknown-size --chunk-size 262144
    $paddedSimulation | Out-Host
    if ($LASTEXITCODE -eq 0 -or -not ($paddedSimulation -contains "trailing_bytes=4096")) {
        throw "La simulación MTP con padding no detectó trailing_bytes=4096"
    }

    if (-not $SkipBuild) {
        Write-Step "Compilación NRO con Docker/devkitPro"
        powershell -ExecutionPolicy Bypass -File .\scripts\build_switch.ps1
    }

    Write-Step "Verificación del NRO generado"
    if (-not (Test-Path $distNro)) {
        throw "No existe el NRO esperado: $distNro"
    }

    $item = Get-Item $distNro
    if ($item.Length -le 0) {
        throw "El NRO existe pero está vacío: $distNro"
    }

    $hash = Get-FileHash $distNro -Algorithm SHA256
    Write-Host "NRO: $distNro"
    Write-Host ("Tamaño: {0:N0} bytes" -f $item.Length)
    Write-Host "SHA256: $($hash.Hash)"

    Write-Host ""
    Write-Host "Smoke test OK. Siguiente prueba real: copiar el NRO a la SD y validar en hardware." -ForegroundColor Green
    Write-Host "Log de hardware esperado: sdmc:/switch/transferencia-switch/logs/install.log"
}
finally {
    Pop-Location
}
