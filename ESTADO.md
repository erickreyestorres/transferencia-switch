# Estado para retomar

## Terminado

- Responder MTP estándar reconocido por Windows como `Transferencia Switch`.
- `1: SD Card` enumerada y navegable en hardware real.
- Lectura de SD estrictamente sin escritura, borrado ni movimiento.
- Modo separado `10: Safe Inbox` para recepción controlada.
- Destino restringido a `sdmc:/switch/transferencia-switch/inbox/`.
- Validación de hijos directos y rechazo de rutas `..`, separadores y rutas externas.
- Recepción en staging oculto, comprobación de tamaño, `fsync` y renombrado final.
- Reserva de 512 MiB para no agotar la tarjeta durante una transferencia.
- Progreso porcentual y resumen por sesión: correctos, fallidos y cancelados.
- 61 pruebas Python aprobadas.
- 37 comprobaciones C aprobadas, incluidas siete reglas de rutas seguras.
- Recepción básica del Inbox confirmada en hardware real.
- Registro multi-storage para publicar SD e Inbox simultáneamente.
- Adaptador opcional de Album SD/NAND mediante `fsOpenImageDirectoryFileSystem`.
- Album se publica únicamente si el montaje funciona y siempre en solo lectura.
- Nand USER y Nand SYSTEM opcionales mediante montajes BIS de solo lectura.
- Safe Inbox renumerado como storage 10 para liberar los números de referencia.
- `7: Saves` agregado como raíz virtual de respaldo read-only.
- Enumeración limitada inicialmente a 20 montajes simultáneos con contadores visibles.
- `4: Installed games` agregado como catálogo read-only generado mediante NS.
- Cada aplicación publica metadatos en `info.txt` y su icono cuando está disponible.
- La caché queda restringida a `sdmc:/switch/transferencia-switch/cache/installed/`.
- Espera USB cancelable aun cuando el host no esté conectado.
- Navegación del menú mediante cruceta y palancas de ambos Joy-Con.
- Puerto hexagonal `IncomingObjectSink` para receptores MTP de acción.
- `5: SD Card install` agregado para NSP/XCI sin compresión, un CNMT y menos de 4 GiB.
- XCI valida HFS0 raíz/secure y convierte cabeceras NCA de Gamecard antes del registro.
- Escritura secuencial directa a placeholders NCM, sin duplicar el paquete en FAT32.
- Validación PFS0/CNMT, registro de metadatos y ApplicationRecord.
- Rollback de metadatos y contenidos nuevos ante error o cancelación.
- Primera instalación NSP confirmada en hardware; Windows perdió la confirmación final.
- La respuesta MTP final ahora se envía antes de cerrar los servicios NCM/NS/ES.
- El ObjectHandle instalado permanece consultable hasta cerrar la sesión MTP para que
  Windows pueda completar su verificación posterior a `SendObject`.
- Un rechazo del instalador ahora drena el objeto MTP completo y conserva el motivo
  específico en pantalla en lugar de sustituirlo por `fallo durante la recepción`.
- Colecciones XCI con hasta 32 CNMT se confirman y revierten como un grupo.
- Compilacion limpia de `switch_app/transferencia_switch.nro` version 0.5.5 con Docker devkitPro.

### Correcciones de instalador (0.5.5)

- Bug XCI corregido: `parseXciSecureHeader` validaba entradas contra
  `xci_secure_target_ + raw.data_offset + raw.file_size > xci_secure_size_`, lo que
  rechazaba NCAs válidas. La validación correcta es `raw.data_offset + raw.file_size
  <= xci_secure_size_ - xci_secure_target_` (los offsets son relativos al inicio de
  los datos de la partición, no al inicio del header). Diagnosticado a partir de
  fotografías de hardware que mostraban `cabecera PFS0 invalida` para un XCI real.
- Bug NSP/XCI: el tamaño mínimo en `initialize()` era `sizeof(Pfs0Header)` = 12 bytes,
  permitiendo que archivos de texto llegaran al parser. Elevado a 1 MiB.
- Bug `detail_` del factory: el mensaje de error de una transferencia anterior
  contaminaba el mensaje del archivo siguiente. Resuelto reseteando `detail_` al inicio
  de cada `open()`.

### UI gráfica (0.5.5)

- Sistema de assets organizado: 24 iconos PNG (32/48/64/128/256 px), 10 frames de
  animación SNES 128 px, barra de progreso SVG/PNG. Todos en `data/ui/icons/`.
- `romfs/ui/icons/` y `romfs/ui/animation/` creados con los PNG de 64 px e iconos de
  estado seleccionados. Script `setup_romfs.ps1` reproducible.
- Makefile actualizado: `ROMFS := romfs`, `--romfsdir` en NROFLAGS, `vendor/stb` en
  INCLUDES, `-lm` agregado a LIBS.
- `stb_image.h` v2.30 incluida como vendor header (`vendor/stb/`). `fetch_stb.ps1`
  descarga el archivo si no existe.
- `FbRenderer` (framebuffer directo 1280×720 RGBA8): primitivas, texto con fuente
  bitmap 8×8, carga PNG desde romfs, barra de progreso con colores del proyecto,
  alpha compositing.
- `GraphicalTransferPresenter`: reemplaza `ConsoleTransferPresenter`. Muestra animación
  SNES durante transferencia, barra de progreso, nombre del archivo, MiB transferidos y
  porcentaje. La lista de resultados guarda **todos** los archivos de la sesión con su
  mensaje de error completo — esto resuelve el problema central que motivó el proyecto
  (DBI no mostraba cuál archivo falló de un grupo).
- `GraphicalMenu`: menú con tarjetas cuadradas 200×200 px, icono 64 px centrado, punto
  de acento por color, borde teal al seleccionar, soporte táctil con hitTest de bounding
  box, navegación con cruceta y palancas.
- `main.cpp` actualizado: usa `GraphicalMenu` y `GraphicalTransferPresenter`. Hilo de
  render paralelo (~60 fps) con tick de animación cada 80 ms. Respaldo con consola libnx
  si el renderer no puede inicializarse.
- `GraphicalTransferPresenter` protegido con mutex para evitar data races entre el hilo
  MTP y el hilo de render.

## Próximos pasos

- Transferir `switch_app/transferencia_switch.nro` a la consola.
- Validar el menú gráfico y la pantalla de transferencia en hardware.
- Validar `5: SD Card install` con un NSP propio pequeño (prueba de XCI pendiente tras
  corrección del parser HFS0 secure).
- `6: NAND install` permanece deshabilitado hasta validar la ruta SD.
