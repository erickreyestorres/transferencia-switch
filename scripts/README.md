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
