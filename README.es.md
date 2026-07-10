# Transferencia Switch

Transferencia Switch es una aplicación homebrew experimental para Nintendo Switch.
Su objetivo es ofrecer una experiencia MTP más clara y segura para transferir,
respaldar y, de forma controlada, instalar contenido desde Windows.

El proyecto se está desarrollando como implementación propia. DBI se usa solo como
referencia pública de comportamiento y experiencia de usuario; no se copia código
privado ni se depende de su fuente.

## Estado actual

- La consola aparece en Windows como dispositivo MTP.
- `1: SD Card` expone la SD en modo lectura.
- `10: Safe Inbox` permite recibir archivos en una carpeta controlada.
- `5: SD Card install` instala NSP/XCI sin compresión hacia SD mediante
  placeholders NCM.
- NAND install permanece deshabilitado.
- NAND USER, NAND SYSTEM, Saves, Album e Installed games se mantienen sin permisos
  de escritura.
- La interfaz gráfica usa iconos, animación de transferencia, resumen por archivo y
  listado de fallos.
- Los errores de instalación se registran en:

  `sdmc:/switch/transferencia-switch/logs/install.log`

- El log rota automáticamente cuando llega a 512 KB:

  `install.log` → `install.previous.log`

## Colaboración

El proyecto está abierto a colaboración técnica y revisión. En particular, nos
interesan aportes sobre:

- transferencias MTP estables con archivos grandes;
- soporte robusto para XCI mayores de 4 GB;
- parsing de XCI/CXCI/trimmed;
- NSZ/XCZ;
- prevención de suspensión durante transferencias largas;
- mejoras de interfaz para pantalla de consola y uso táctil.

Si el equipo de DBI u otros desarrolladores homebrew consideran que alguna idea
puede ser útil para proyectos existentes, estamos abiertos a coordinar, aprender o
adaptar el enfoque. Este repositorio busca ser una implementación independiente y
documentada, no una copia de código privado.

## Limitaciones conocidas

- Los XCI/NSP grandes que Windows informa con tamaño MTP desconocido (`0xFFFFFFFF`)
  ahora usan una ruta streaming experimental: la app intenta inferir el tamaño real
  desde PFS0/HFS0 y detenerse al completar el paquete. Debe validarse con XCI grandes
  reales antes de considerarlo estable.
- NSZ/XCZ todavía no están soportados.
- La instalación hacia NAND no está habilitada por seguridad.
- La suspensión automática de la consola puede cortar transferencias largas.
- El soporte táctil existe en el menú gráfico, pero todavía no es el foco principal.

## Estructura

- `switch_app/`: aplicación homebrew para Nintendo Switch.
- `switch_app/vendor/mtp/`: núcleo MTP con licencia Apache 2.0 y avisos propios.
- `pc_backend/`: backend USB privado heredado, conservado para pruebas y diagnóstico.
- `tests/`: pruebas Python de arquitectura, MTP y reglas de seguridad.
- `docs/`: documentación técnica y de producto.

Para una explicación carpeta por carpeta, revisa
[`docs/estructura-proyecto.es.md`](docs/estructura-proyecto.es.md).

Para retomar el desarrollo desde el último punto conocido, revisa
[`docs/estado.es.md`](docs/estado.es.md).

## Compilación

Requiere Docker Desktop y la imagen `devkitpro/devkita64`:

```powershell
docker run --rm -v "${PWD}:/work" -w /work/switch_app `
  devkitpro/devkita64:latest bash -lc "source /opt/devkitpro/devkita64.sh && make clean && make -j2"
```

El resultado se genera en:

`dist/transferencia_switch.nro`

Ese es el archivo que debes copiar a la SD de la consola.

## Pruebas

```powershell
python -m unittest discover -s tests -q
powershell -ExecutionPolicy Bypass -File .\scripts\test_switch_core.ps1
```

Para una prueba funcional rápida de PC antes de copiar un nuevo NRO a la consola:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\functional_smoke.ps1
```

Esto ejecuta pruebas Python, pruebas C, compila con Docker y verifica que exista
`dist/transferencia_switch.nro` con hash SHA256. No reemplaza la prueba real en
hardware, pero evita instalar builds rotos.

## Seguridad del proyecto

Este proyecto prioriza rutas seguras:

- No se escribe en NAND.
- La SD general se expone en lectura.
- La instalación real está limitada a `5: SD Card install`.
- Los errores intentan revertir contenidos nuevos cuando es posible.
- Los logs ayudan a diagnosticar fallos sin adivinar.

El proyecto está pensado para desarrollo, investigación y uso con contenido propio
o respaldos legítimos. Cada usuario es responsable de cumplir las leyes y términos
aplicables en su país.
