$ErrorActionPreference = "Stop"

$root = (Resolve-Path $PSScriptRoot).Path
$python = Join-Path $root ".venv\Scripts\python.exe"
$requirements = Join-Path $root "pc_backend\requirements.txt"

if (-not (Test-Path -LiteralPath $python)) {
    python -m venv (Join-Path $root ".venv")
}

& $python -m pip install -q -r $requirements
if ($LASTEXITCODE -ne 0) {
    throw "No se pudieron instalar las dependencias del backend"
}

& $python (Join-Path $root "pc_backend\main.py")

