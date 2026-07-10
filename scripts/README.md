# `scripts`

Comandos de ayuda para compilar, probar y ejecutar partes del proyecto sin tener
que recordar cada comando manualmente.

## `build_switch.ps1`

Compila la app de Nintendo Switch usando Docker/devkitPro y copia el NRO final a:

```text
dist/transferencia_switch.nro
```

## `test_switch_core.ps1`

Compila y ejecuta las pruebas C del núcleo portable del proyecto. No requiere una
Switch.

## `functional_smoke.ps1`

Prueba funcional rápida antes de pasar el NRO a la consola:

1. ejecuta todas las pruebas Python;
2. ejecuta las pruebas C;
3. compila el NRO con Docker;
4. verifica que exista `dist/transferencia_switch.nro`;
5. imprime tamaño y SHA256 del NRO.

Uso recomendado:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\functional_smoke.ps1
```

Si ya compilaste y solo quieres verificar pruebas + NRO existente:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\functional_smoke.ps1 -SkipBuild
```

Esta prueba no reemplaza la validación en hardware; sirve para detectar errores
antes de copiar el NRO a la SD.

## Fixtures de paquetes sintéticos

Para simular NSP/XCI sin usar archivos reales:

```powershell
python .\tools\package_fixtures.py make-nsp .\_local\fixtures\demo.nsp --payload-size 2097152
python .\tools\package_fixtures.py make-xci .\_local\fixtures\large.xci --secure-payload-size 5368709120
python .\tools\package_fixtures.py inspect .\_local\fixtures\large.xci
python .\tools\package_fixtures.py simulate-stream .\_local\fixtures\large.xci --chunk-size 262144
```

Los XCI grandes se crean como archivos sparse cuando Windows/NTFS lo permite, por lo
que pueden simular varios GiB sin ocuparlos físicamente en disco.

`simulate-stream` ayuda a detectar si el tamaño inferido por PFS0/HFS0 coincide con el
tamaño del archivo del host o si quedan bytes extra al final que requieren validación
en hardware.

## Simulador MTP local

Para simular el envío MTP por chunks con tamaño conocido o desconocido:

```powershell
python .\tools\mtp_stream_simulator.py .\_local\fixtures\large.xci --unknown-size --chunk-size 524288
```

También permite simular cancelaciones:

```powershell
python .\tools\mtp_stream_simulator.py .\_local\fixtures\demo.nsp --cancel-after 65536
```
