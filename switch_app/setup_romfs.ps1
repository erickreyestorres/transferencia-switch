$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

New-Item -ItemType Directory -Force -Path "romfs\ui\icons" | Out-Null
New-Item -ItemType Directory -Force -Path "romfs\ui\animation" | Out-Null

$iconSrc = "data\ui\icons"
$iconDst = "romfs\ui\icons"

$copies = @(
    "lote1_snes\01_mtp_connect_64.png|01_mtp_connect.png",
    "lote1_snes\02_sd_card_readonly_64.png|02_sd_card.png",
    "lote1_snes\03_safe_inbox_64.png|03_inbox.png",
    "lote1_snes\04_nand_user_64.png|04_nand_user.png",
    "lote1_snes\05_nand_system_64.png|05_nand_system.png",
    "lote1_snes\06_installed_content_64.png|06_installed.png",
    "lote1_snes\07_install_sd_64.png|07_install_sd.png",
    "lote1_snes\08_install_nand_64.png|08_install_nand.png",
    "lote1_snes\09_save_data_64.png|09_saves.png",
    "lote1_snes\10_album_64.png|10_album.png",
    "lote2_snes\13_diagnostics_64.png|13_diagnostics.png",
    "lote2_snes\15_exit_64.png|15_exit.png",
    "lote2_snes\16_state_waiting_connection_64.png|16_waiting.png",
    "lote2_snes\17_state_connected_64.png|17_connected.png",
    "lote2_snes\18_state_receiving_64.png|18_receiving.png",
    "lote2_snes\19_state_sending_64.png|19_sending.png",
    "lote2_snes\20_state_transfer_success_64.png|20_success.png",
    "lote3_snes\21_state_transfer_failed_64.png|21_failed.png",
    "lote3_snes\22_state_cancelled_64.png|22_cancelled.png",
    "lote3_snes\23_state_warning_64.png|23_warning.png"
)

foreach ($entry in $copies) {
    $parts = $entry -split "\|"
    $src = "$iconSrc\$($parts[0])"
    $dst = "$iconDst\$($parts[1])"
    if (Test-Path $src) {
        Copy-Item -Force $src $dst
        Write-Host "OK  $($parts[1])"
    } else {
        Write-Host "MISS $src"
    }
}

$animSrc = "data\ui\icons\animation_snes"
$animDst = "romfs\ui\animation"
for ($i = 0; $i -le 9; $i++) {
    $name = "transfer_active_frame_{0:D2}.png" -f $i
    $dst  = "frame_{0:D2}.png" -f $i
    if (Test-Path "$animSrc\$name") {
        Copy-Item -Force "$animSrc\$name" "$animDst\$dst"
        Write-Host "OK  $dst"
    } else {
        Write-Host "MISS $name"
    }
}

Write-Host ""
Write-Host "romfs listo."
