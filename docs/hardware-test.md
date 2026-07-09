# Pruebas con la consola

## Lectura de SD — validada

1. Iniciar **Explorar SD - solo lectura**.
2. Abrir `Transferencia Switch > 1: SD Card` en Windows.
3. Navegar carpetas y copiar un archivo pequeño desde la Switch al PC.
4. Confirmar que crear, borrar y renombrar continúan bloqueados.

La enumeración y navegación fueron confirmadas en hardware real el 6 de julio de 2026.

## Recepción controlada — pendiente

La aplicación conserva una reserva de 512 MiB. Si la SD tiene menos espacio libre,
Windows mostrará el Inbox sin capacidad y la copia será rechazada deliberadamente.

1. Liberar al menos 1 GiB en la SD para realizar la prueba con margen.
2. Copiar `switch_app/transferencia_switch.nro` actualizado a la consola.
3. Iniciar **Recibir archivos - Inbox seguro**.
4. Abrir `Transferencia Switch > 10: Safe Inbox` en Windows.
5. Copiar un archivo personal pequeño, idealmente entre 1 y 10 MiB.
6. Comprobar en la pantalla de Switch el nombre, porcentaje y resultado `OK`.
7. Cerrar MTP con B o +.
8. Verificar el archivo en `sd:/switch/transferencia-switch/inbox/` y comparar tamaño
   o SHA-256 con el original.
9. Confirmar que no apareció ningún archivo `.partial` fuera del staging oculto.

No probar todavía archivos mayores de 4 GiB, sobrescritura, borrado ni destinos fuera
del Inbox. MTP usa el controlador de dispositivo portátil de Windows; no instalar
libusbK ni WinUSB para estas pruebas.

## Modo principal, NAND, Saves y Album — pendiente

1. Instalar el NRO 0.4.7 y seleccionar **MTP principal - vistas disponibles**.
2. Fotografiar la línea `Vistas` mostrada en la pantalla de Switch. Indicará `OK` o un
   código de error independiente para Album SD y Album NAND.
3. Confirmar que Windows muestre `1: SD Card` y `10: Safe Inbox`.
4. Si los montajes BIS están disponibles, confirmar `2: Nand USER` y
   `3: Nand SYSTEM`.
5. Abrir ambas NAND y copiar únicamente un archivo pequeño hacia el PC.
6. Confirmar que Windows no permita crear, borrar, mover ni renombrar dentro de NAND.
7. Si los montajes están disponibles, confirmar que aparezcan `8: Album (SD)` y/o
   `8: Album (NAND)`.
8. Abrir cada Album, navegar sus carpetas y copiar una captura pequeña al PC.
9. Confirmar que Windows no permita crear, borrar o renombrar contenido del Album.
10. Verificar que SD e Inbox continúen funcionando aunque algún montaje no aparezca.
11. En la línea `Vistas`, registrar el bloque `Saves: montados/detectados`, además de
    los contadores `fail`, `limite` y el código final.
12. Si aparece `7: Saves`, abrir una carpeta identificada por Application ID y usuario.
13. Copiar un archivo pequeño de la partida hacia el PC y verificar que no pueda
    escribirse, borrarse ni renombrarse nada dentro de `7: Saves`.

La primera implementación monta como máximo 20 saves primarios de usuario. Si el
contador `limite` es mayor que cero, no es una pérdida de datos: indica que los restantes
no se publicaron en esa sesión y debemos implementar montaje bajo demanda.

Esta versión todavía muestra SD y NAND como dos storages Album independientes. La
unificación visual en una sola vista dinámica se realizará después de validar ambos
montajes y su estructura real en hardware.

## Catálogo de aplicaciones instaladas — pendiente

1. En la pantalla de Switch, registrar `Installed: estado, apps, fail y código`.
2. Si el estado es `OK`, confirmar que Windows muestre `4: Installed games`.
3. Abrir varias carpetas y comprobar que el nombre incluya el Application ID.
4. Abrir o copiar un `info.txt` y comprobar nombre, versión y estados de contenido.
5. Abrir o copiar `icon.jpg` cuando exista.
6. Confirmar que no se expongan binarios instalados y que Windows no permita crear,
   borrar, mover ni renombrar objetos dentro de esta vista.

El catálogo se reconstruye en
`sdmc:/switch/transferencia-switch/cache/installed/`. Solo esa caché dedicada puede
ser reemplazada; las aplicaciones instaladas no se modifican.

## Salida sin host USB — pendiente

1. Instalar el NRO 0.4.7 y dejar el cable USB desconectado.
2. Entrar en **Explorar SD - solo lectura** y pulsar **B**; debe volver al menú en
   aproximadamente dos segundos como máximo.
3. Repetir con **Recibir archivos - Inbox seguro** usando **+**.
4. Repetir una vez con el cable conectado para confirmar que MTP continúa enumerando.

## Instalación SD 0.5.4 — validación parcial

Esta prueba modifica el catálogo de contenido del entorno activo. Utilizar primero un
NSP o XCI pequeño, propio, sin compresión, con un solo CNMT y menor de 4 GiB. Mantener un
respaldo y espacio libre suficiente en la SD.

1. Instalar el NRO 0.5.4 y abrir **MTP principal - vistas disponibles**.
2. Confirmar que Windows muestre `5: SD Card install`.
3. Copiar primero un archivo de texto renombrado como `.nsp`. Debe fallar claramente
   por cabecera PFS0 inválida y no debe aparecer ninguna aplicación nueva.
4. Cerrar y volver a abrir MTP para iniciar una sesión limpia.
5. Copiar el NSP de prueba a `5: SD Card install`.
6. No desconectar el cable durante la transferencia ni durante la fase final de
   registro. Esperar el resultado `NSP instalado correctamente en SD`.
7. Cerrar MTP y comprobar que la aplicación aparezca en el menú HOME y en
   `4: Installed games`.
8. Abrir la aplicación para verificar que Horizon reconoce el registro completo.
9. Repetir con un XCI propio menor de 4 GiB. Debe validar HFS0, instalar desde la
   partición `secure` y mostrar `XCI instalado correctamente en SD`.

No probar todavía NSZ, XCZ, paquetes con varios CNMT ni archivos de 4 GiB o más.
`6: NAND install` sigue intencionalmente ausente en esta versión.

Las primeras pruebas instalaron correctamente el NSP y la consola indicó `1 correcto`,
pero Windows informó que el dispositivo dejó de responder al terminar. La versión
0.5.4 responde antes del cierre de servicios y conserva temporalmente el ObjectHandle;
repetir con un paquete pequeño y confirmar que Windows finalice sin mostrar el aviso.

El XCI `Dragon Quest 1 and 2 and 3 Collection.xci` fue inspeccionado sin modificarlo:
su HFS0 `secure` comienza al 19,33 % y contiene tres CNMT. Las pruebas 0.5.1/0.5.3
fallaron al comenzar la primera NCA y ocultaron el motivo. En 0.5.4 repetir una sola vez
y fotografiar la línea `[ERROR]`; ahora debe indicar el resultado exacto de la cabecera
NCA o del placeholder, no el mensaje genérico de recepción.
