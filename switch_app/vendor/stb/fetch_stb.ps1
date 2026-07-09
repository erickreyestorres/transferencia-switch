# Descarga stb_image.h desde el repositorio oficial nothings/stb.
# Ejecutar una vez antes de compilar, o integrarlo en el build script.
# La version minima requerida es 2.28 (ARM64 compatible).
$ErrorActionPreference = "Stop"
$url = "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
$dest = Join-Path $PSScriptRoot "stb_image.h"
if (Test-Path $dest) {
    Write-Host "stb_image.h ya existe, omitiendo descarga."
    exit 0
}
Write-Host "Descargando stb_image.h desde $url ..."
Invoke-WebRequest -Uri $url -OutFile $dest -UseBasicParsing
Write-Host "stb_image.h descargado correctamente ($([math]::Round((Get-Item $dest).Length / 1024)) KB)."
