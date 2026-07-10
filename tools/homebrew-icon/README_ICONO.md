# Icono Homebrew Menu - Transferencia Switch

Este paquete agrega el icono del `.nro` para Homebrew Menu.

## Archivos incluidos

- `switch_app/icon.jpg`  
  Icono final en formato JPG 256x256 para libnx/devkitPro.

- `switch_app/icon_source.png`  
  Versión fuente PNG en alta resolución, útil para editar más adelante.

- `transferencia-switch-icon.patch`  
  Parche para agregar `--icon=$(CURDIR)/icon.jpg` al `Makefile`.

- `aplicar_icono.ps1`  
  Script opcional para modificar el `Makefile` automáticamente.

## Instalación recomendada

1. Descomprime este ZIP encima de la raíz del proyecto:

   `C:\Users\erick\Documents\Codex\2026-07-06\cr\proyectos\transferencia-switch`

2. Ejecuta:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\tools\homebrew-icon\aplicar_icono.ps1
   ```

3. Compila:

   ```powershell
   docker run --rm -v "${PWD}:/work" -w /work/switch_app devkitpro/devkita64:latest bash -lc "source /opt/devkitpro/devkita64.sh && make clean && make -j2"
   ```

4. Copia el nuevo archivo:

   `switch_app\transferencia_switch.nro`

   a la SD de la consola.

## Cambio manual equivalente

En `switch_app/Makefile`, cambia:

```make
export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp --romfsdir=$(CURDIR)/$(ROMFS)
```

por:

```make
export NROFLAGS += --nacp=$(CURDIR)/$(TARGET).nacp --icon=$(CURDIR)/icon.jpg --romfsdir=$(CURDIR)/$(ROMFS)
```

## Nota

Este paquete solo agrega el icono visible en Homebrew Menu. No cambia la lógica MTP, SD install, NAND, Safe Inbox ni la UI interna.
