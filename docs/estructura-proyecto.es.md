# Estructura del proyecto

Este documento explica qué contiene cada carpeta y para qué sirve. La idea es que
puedas ubicarte rápido aunque no estés metido todos los días en el código.

## Vista general

```text
transferencia-switch/
├─ switch_app/      Aplicación real que corre en Nintendo Switch.
├─ pc_backend/      Backend USB antiguo/prototipo, conservado para pruebas.
├─ tests/           Pruebas automáticas Python.
├─ docs/            Documentación técnica y de producto.
├─ scripts/         Scripts para compilar, probar o ejecutar prototipos.
├─ tools/           Herramientas auxiliares para mantener assets.
├─ dist/            Salida generada: NRO listo para copiar a la consola.
├─ README*.md       Documentación principal en español e inglés.
└─ .gitignore       Reglas para no subir binarios ni archivos temporales.
```

## `switch_app/`

Es el corazón del proyecto. Aquí vive el `.nro` que se compila para la Nintendo
Switch.

- `source/`: código C/C++ de la aplicación.
- `include/`: headers públicos del código de Switch.
- `romfs/`: assets finales que se empaquetan dentro del NRO.
- `data/`: assets fuente y material de trabajo, por ejemplo iconos originales.
- `vendor/`: código externo incorporado al proyecto.
- `tests/`: pruebas C del núcleo independiente.
- `Makefile`: receta de compilación con devkitPro.
- `icon.jpg`: icono mostrado por Homebrew Menu.

## `switch_app/source/`

Código dividido por arquitectura hexagonal:

- `domain/`: reglas puras del dominio. No deberían depender de libnx, MTP ni del
  sistema operativo.
- `application/`: casos de uso que coordinan el dominio.
- `adapters/`: integración con el mundo real: MTP, SD, NAND en lectura, saves,
  instalador SD, UI, libnx.
- `main.cpp`: punto de entrada de la app en Switch. Conecta menú, MTP, storages e
  instalador.

## `switch_app/include/`

Headers que declaran las piezas anteriores. Mantenerlos ordenados ayuda a que los
módulos no se mezclen demasiado.

## `switch_app/romfs/`

Contiene solo assets que la app necesita en tiempo de ejecución:

- `ui/icons/`: iconos PNG ya seleccionados para el menú y estados.
- `ui/animation/`: frames usados durante la transferencia.

Si un archivo está en `romfs`, normalmente termina dentro del `.nro`.

## `switch_app/data/`

Material fuente o de diseño. No todo lo que está aquí se usa directamente en la
app. Por ejemplo, aquí pueden vivir lotes de iconos grandes, SVGs, previews y
manifests generados por IA.

Regla práctica:

- `data/` = material editable/fuente.
- `romfs/` = material final que se empaqueta en la app.

## `pc_backend/`

Prototipo anterior de comunicación USB privada desde PC. Ya no es el flujo principal
porque ahora usamos MTP estándar, pero se conserva porque:

- documenta decisiones antiguas;
- mantiene pruebas útiles;
- puede servir para diagnóstico futuro.

## `tests/`

Pruebas automáticas Python. No corren en la Switch; corren en el PC y revisan que el
proyecto mantenga reglas importantes:

- arquitectura hexagonal;
- seguridad de rutas;
- MTP read-only;
- storages virtuales;
- instalador SD;
- logs;
- reglas de NAND deshabilitada.

Comando:

```powershell
python -m unittest discover -s tests -q
```

## `docs/`

Documentación técnica y de producto. Aquí se guarda el “por qué” de las decisiones.

- `architecture.md`: arquitectura hexagonal.
- `vision-producto.md`: objetivo del producto.
- `analisis-tecnico-mtp.md`: notas técnicas MTP/Switch.
- `vistas-virtuales.md`: storages virtuales.
- `instalacion-mtp.md`: diseño de instalación por MTP.
- `hardware-test.md`: pruebas en consola real.
- `mtp-roadmap.md`: próximos pasos MTP.
- `estado.es.md`: resumen práctico para retomar el proyecto.

## `tools/`

Herramientas auxiliares que ayudan al proyecto pero no forman parte de la app final.

Ejemplo:

- `tools/homebrew-icon/`: scripts y patch usados para integrar el icono del Homebrew
  Menu.

## Archivos generados que no deben subirse

Estos archivos se generan al compilar y están ignorados por Git:

- `switch_app/transferencia_switch.nro`
- `switch_app/transferencia_switch.elf`
- `switch_app/transferencia_switch.nacp`
- `switch_app/*.map`
- carpetas `build/`
- ZIPs temporales

El script `scripts/build_switch.ps1` copia automáticamente el NRO final a:

`dist/transferencia_switch.nro`

## Regla de trabajo recomendada

Cada mejora debería ir en su propia rama:

```powershell
git checkout -b feature/nombre-corto
python -m unittest discover -s tests -q
powershell -ExecutionPolicy Bypass -File .\scripts\test_switch_core.ps1
git add .
git commit -m "Mensaje claro"
git push -u origin feature/nombre-corto
```
