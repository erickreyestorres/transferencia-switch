$ErrorActionPreference = "Stop"

$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
docker version | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Docker Desktop no está disponible"
}

docker run --rm `
    -v "${root}:/work" `
    -w /work/switch_app/tests `
    gcc:14-bookworm `
    bash -lc 'make clean && make test; result=$?; make clean; exit $result'

if ($LASTEXITCODE -ne 0) {
    throw "Fallaron las pruebas del núcleo C"
}
