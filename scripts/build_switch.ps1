$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
docker version | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Docker Desktop no está disponible"
}

docker run --rm `
    -v "${root}:/work" `
    -w /work/switch_app `
    devkitpro/devkita64:latest `
    bash -lc "source /opt/devkitpro/devkita64.sh && make -j2"

if ($LASTEXITCODE -ne 0) {
    throw "Falló la compilación de la aplicación Switch"
}

Write-Host "Generado: $root\switch_app\transferencia_switch.nro"
