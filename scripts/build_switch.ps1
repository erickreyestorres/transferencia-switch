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

$sourceNro = Join-Path $root "switch_app\transferencia_switch.nro"
$dist = Join-Path $root "dist"
$distNro = Join-Path $dist "transferencia_switch.nro"

if (-not (Test-Path -LiteralPath $sourceNro)) {
    throw "No se encontro el NRO generado en $sourceNro"
}

New-Item -ItemType Directory -Force -Path $dist | Out-Null
Copy-Item -LiteralPath $sourceNro -Destination $distNro -Force

Write-Host "Generado para instalar/copiar: $distNro"
