# `scripts`

Scripts cómodos para trabajar desde Windows sin llenar la raíz del proyecto.

Ejecutarlos siempre desde la raíz del proyecto, por ejemplo:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build_switch.ps1
```

## Archivos

- `build_switch.ps1`: compila con Docker/devkitPro y copia el NRO final a
  `dist/transferencia_switch.nro`.
- `build_switch.cmd`: acceso rápido para ejecutar el script anterior.
- `test_switch_core.ps1`: corre las pruebas C dentro de Docker.
- `run_backend.ps1`: ejecuta el backend USB privado heredado.
- `run_backend.cmd`: acceso rápido para el backend heredado.
