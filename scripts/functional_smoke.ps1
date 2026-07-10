param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
$distNro = Join-Path $root "dist\transferencia_switch.nro"

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
