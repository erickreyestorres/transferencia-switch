param(
    [switch]$NoBuild
)

$ErrorActionPreference = "Stop"

$makefile = Join-Path $PSScriptRoot "switch_app\Makefile"
$icon = Join-Path $PSScriptRoot "switch_app\icon.jpg"

if (!(Test-Path $makefile)) {
    throw "No encontré switch_app\Makefile. Descomprime este ZIP encima de la raíz del proyecto transferencia-switch."
}
if (!(Test-Path $icon)) {
    throw "No encontré switch_app\icon.jpg."
}

$content = Get-Content $makefile -Raw
if ($content -notmatch '--icon=\$\(CURDIR\)/icon\.jpg') {
    $old = '--nacp=$(CURDIR)/$(TARGET).nacp --romfsdir=$(CURDIR)/$(ROMFS)'
    $new = '--nacp=$(CURDIR)/$(TARGET).nacp --icon=$(CURDIR)/icon.jpg --romfsdir=$(CURDIR)/$(ROMFS)'
    if ($content.Contains($old)) {
        $content = $content.Replace($old, $new)
        Set-Content -Path $makefile -Value $content -NoNewline
        Write-Host "Makefile actualizado: se agregó --icon=$(CURDIR)/icon.jpg"
    } else {
        throw "No encontré la línea NROFLAGS esperada. Revisa manualmente el Makefile o aplica el .patch."
    }
} else {
    Write-Host "Makefile ya tenía configurado el icono."
}

$hash = Get-FileHash $icon -Algorithm SHA256
Write-Host "icon.jpg OK: $($hash.Hash)"

if (!$NoBuild) {
    Write-Host "`nPara compilar ahora ejecuta desde la raíz del proyecto:"
    Write-Host 'docker run --rm -v "${PWD}:/work" -w /work/switch_app devkitpro/devkita64:latest bash -lc "source /opt/devkitpro/devkita64.sh && make clean && make -j2"'
}
