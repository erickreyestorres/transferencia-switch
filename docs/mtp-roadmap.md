# Hoja de ruta MTP

## 0.3 - SD de solo lectura

Estado: validada en hardware real.

- Dispositivo portátil MTP estándar.
- Un storage `1: SD Card`.
- Enumeración diferida de carpetas.
- Lectura completa y parcial.
- Sin operaciones mutables anunciadas.

## 0.4 - Escritura controlada

Estado: implementada; pendiente de validación en hardware real.

La recepción básica fue confirmada en hardware. La versión 0.4.1 añade el registro
multi-storage y un modo combinado SD + Inbox, pendiente de prueba en consola.

La versión 0.4.3 adelanta la validación estrictamente de solo lectura de Nand USER y
Nand SYSTEM. Las funciones NAND mutables continúan fuera de alcance.

La versión 0.4.4 añade `7: Saves` para respaldo read-only con raíz virtual. La
restauración se mantiene deshabilitada hasta diseñar staging y validación específicos.

La versión 0.4.5 añade `4: Installed games` como catálogo read-only. Publica nombre,
Application ID, versión, estados de contenido e icono mediante una caché controlada;
no extrae binarios ni realiza instalaciones.

La versión 0.4.6 hace cancelable la espera inicial de USB. **B** o **+** permiten salir
del servidor MTP incluso si nunca se conectó un host.

La versión 0.4.7 permite navegar el menú con cruceta o palancas. La futura interfaz
por tarjetas conservará las acciones desacopladas para admitir entrada táctil opcional.

- Carpeta inicial restringida `sdmc:/switch/transferencia-switch/inbox/`.
- Nombres y rutas normalizados; rechazo de recorridos `..`.
- Archivo temporal, verificación de tamaño y renombrado atómico.
- Espacio reservado y cancelación segura.
- Sin borrado ni modificación fuera de la carpeta de entrada.

## 0.5 - Instalación SD

La versión 0.5.0 publica `5: SD Card install` como receptor de acción. El primer
alcance acepta NSP sin compresión, con un CNMT y menor de 4 GiB. La versión 0.5.1
añade XCI mediante validación HFS0 raíz/secure y conversión de cabeceras NCA de
Gamecard. El contenido viaja
directamente a placeholders NCM y la operación confirma ContentMeta y ApplicationRecord
solo al final. Los contenidos creados por una operación fallida se revierten.

La versión 0.5.2 corrige el orden de finalización observado en la primera prueba NSP:
el resultado MTP se responde a Windows antes de cerrar las sesiones NCM/NS/ES.

La versión 0.5.3 conserva el ObjectHandle virtual completado hasta `SessionEnded`.
Esto permite que Explorer consulte `ObjectInfo` después de `SendObject` sin interpretar
la desaparición inmediata del objeto como una desconexión.

La versión 0.5.4 mantiene alineada la transacción MTP cuando el receptor rechaza un
bloque, muestra el motivo original y admite colecciones de hasta 32 CNMT.

- Instalación NSP confirmada; pendiente repetir la confirmación final y validar XCI.
- NSZ/XCZ, paquetes combinados, objetos de 4 GiB o más y destino NAND no se publican.

## 0.8 - SD completa

- Creación, movimiento y borrado con confirmaciones.
- Política de carpetas protegidas.
- Manejo de archivos mayores de 4 GiB en FAT32 mediante partes verificables.
- Registro de operaciones y recuperación tras desconexión.

## 0.6 - Vistas virtuales personales

- Album de capturas y videos.
- Exportación de partidas propias, inicialmente de solo lectura.
- Restauración de partidas mediante staging y validación posterior.

## 0.7 - Vistas avanzadas

- NAND USER y NAND SYSTEM exclusivamente de solo lectura.
- Identificación visible del entorno activo: sysMMC o emuMMC.
- Exportación de respaldo de un cartucho físico propio.

Las funciones avanzadas no se habilitarán hasta que la capa anterior tenga pruebas
unitarias, integración MTP y validación satisfactoria con hardware real.
