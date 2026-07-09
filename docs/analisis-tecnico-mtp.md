# 1. ANÁLISIS DE HARDWARE Y ARQUITECTURA DEL SUBSISTEMA USB (`usb:ds`)

## 1.1 Límite verificable del análisis de DBI

El repositorio público de DBI contiene documentación, configuración y el backend de
PC, pero no el código C/C++ del responder MTP. Por ello, el comportamiento visible de
DBI puede documentarse, pero no es técnicamente válido atribuirle un algoritmo interno,
una topología de hilos o descriptores exactos sin una captura USB o análisis de su
binario. Las estructuras exactas indicadas a continuación corresponden al estándar,
a las APIs públicas de Horizon/libnx y a la implementación 0.3.0 de Transferencia
Switch. [Repositorio público de DBI](https://github.com/rashevskyv/dbi)

## 1.2 Ruta física y lógica

Una aplicación homebrew ejecutada bajo Horizon OS no configura directamente los
registros XUSB, la PHY ni el controlador USB-PD del Tegra X1. La ruta efectiva es:

```text
Responder MTP en espacio de usuario
        │ llamadas libnx
        ▼
HIPC/CMIF → servicio usb:ds
        │ IPC + handles de eventos
        ▼
sysmodule USB de Horizon
        │ controlador/driver interno
        ▼
XUSB Device Controller + PHY USB
        │ negociación física y de rol
        ▼
USB-C → host Windows/Linux/macOS
```

`usb:ds` es el servicio de dispositivo: permite registrar hasta cuatro interfaces,
añadir descriptores de cadenas, instalar descriptores de dispositivo por velocidad,
registrar endpoints y habilitar el dispositivo. La gestión de rol USB-C, alimentación
y estados físicos pertenece al sysmodule USB y a los servicios de port manager; no al
responder MTP. `libnx` encapsula las sesiones IPC, dominios CMIF y handles de eventos.
[USB services](https://switchbrew.org/wiki/USB_services),
[usbDs de libnx](https://switchbrew.github.io/libnx/usbds_8h.html)

## 1.3 Secuencia cronológica de inicialización

1. El `.nro` inicializa su interfaz y monta `sdmc:/` mediante el runtime de libnx.
2. Construye el descriptor USB de dispositivo y el descriptor de interfaz MTP.
3. `usbDsInitialize()` abre una sesión IPC con `usb:ds`.
4. Se registran idioma, fabricante, producto y número de serie.
5. Se instalan descriptores de dispositivo para Full, High y Super Speed.
6. Se registra una interfaz Still Image/MTP y tres endpoints.
7. Para cada velocidad se agregan los descriptores de interfaz y endpoints.
8. `usbDsEnable()` publica el dispositivo al host.
9. `usbDsWaitReady()` bloquea hasta que el host termina la enumeración/configuración.
10. Cada transferencia crea un URB mediante `usbDsEndpoint_PostBufferAsync()`.
11. El proceso espera el evento de finalización, recupera `UsbDsReportData` y valida
    el número real de bytes mediante `usbDsParseReportData()`.
12. Al salir se cancelan operaciones pendientes y `usbDsExit()` restaura la
    configuración USB por defecto.

Los buffers entregados directamente a operaciones asíncronas deben estar alineados a
una página de `0x1000` bytes. La capa incorporada al proyecto utiliza buffers de rebote
alineados cuando el origen o destino no cumple ese requisito.

## 1.4 Descriptores USB de Transferencia Switch 0.3.0

Descriptor de dispositivo lógico antes de que `usb:ds` asigne índices de cadenas:

| Offset | Tamaño | Campo | Valor |
|---:|---:|---|---:|
| `0x00` | 1 | `bLength` | `0x12` |
| `0x01` | 1 | `bDescriptorType` | `0x01` |
| `0x02` | 2 | `bcdUSB` | `0x0110`, `0x0200` o `0x0300` |
| `0x04` | 1 | `bDeviceClass` | `0x00`, definido por interfaz |
| `0x05` | 1 | `bDeviceSubClass` | `0x00` |
| `0x06` | 1 | `bDeviceProtocol` | `0x00` |
| `0x07` | 1 | `bMaxPacketSize0` | `0x40`; `0x09` en SuperSpeed |
| `0x08` | 2 | `idVendor` | `0x057E` |
| `0x0A` | 2 | `idProduct` | `0x4000` |
| `0x0C` | 2 | `bcdDevice` | `0x0300` |
| `0x0E` | 1 | `iManufacturer` | asignado por `usb:ds` |
| `0x0F` | 1 | `iProduct` | asignado por `usb:ds` |
| `0x10` | 1 | `iSerialNumber` | asignado por `usb:ds` |
| `0x11` | 1 | `bNumConfigurations` | `0x01` |

La interfaz tiene nueve bytes y usa `bInterfaceClass=0x06`,
`bInterfaceSubClass=0x01`, `bInterfaceProtocol=0x01` y tres endpoints. Los layouts de
estas estructuras están definidos por libnx. [Descriptores USB de libnx](https://switchbrew.github.io/libnx/usb_8h_source.html)

| Endpoint | Dirección | Tipo | Full Speed | High Speed | SuperSpeed |
|---|---|---|---:|---:|---:|
| Bulk OUT | host → Switch | `0x02` | 64 B | 512 B | 1024 B |
| Bulk IN | Switch → host | `0x02` | 64 B | 512 B | 1024 B |
| Interrupt IN | Switch → host | `0x03` | 28 B | 28 B | 28 B |

El tamaño máximo de paquete no es el tamaño del bloque de aplicación. Un objeto puede
procesarse en bloques de KiB o MiB y el controlador lo segmenta en paquetes USB. El
endpoint interrupt IN transporta eventos como incorporación o eliminación de objetos;
los datos de archivo circulan por los endpoints bulk.

# 2. PROTOCOLO MTP A BAJO NIVEL (ESPECIFICACIÓN, OPERACIONES Y TRANSACCIONES)

## 2.1 Contenedores y fases

MTP hereda el modelo de PTP. Todo contenedor comienza con:

| Campo | Tipo | Descripción |
|---|---|---|
| Longitud | `uint32 LE` | bytes totales del contenedor |
| Tipo | `uint16 LE` | comando `1`, datos `2`, respuesta `3`, evento `4` |
| Código | `uint16 LE` | operación, respuesta o evento |
| Transaction ID | `uint32 LE` | correlación iniciador–responder |
| Parámetros/datos | variable | hasta cinco parámetros en comandos/respuestas |

Una transacción sigue `Command → Data opcional → Response`. Solo el host iniciador
emite comandos. El responder no inicia operaciones de archivos; únicamente puede
enviar eventos por el endpoint interrupt.

## 2.2 StorageID y ObjectHandle

Un `StorageID` identifica una vista lógica. No implica una partición física. Por
ejemplo, `SD Card`, `Album` y `Saves` pueden ser proveedores distintos aunque algunos
objetos terminen leyéndose desde el mismo medio.

Un `ObjectHandle` es un identificador de 32 bits, no una dirección de memoria ni un
hash obligatorio. La especificación permite que su validez se limite a la sesión. La
implementación actual usa:

```text
next_handle = 1

al descubrir un objeto:
    handle = next_handle
    next_handle += 1
    tabla[handle] = {
        storage_id,
        parent_handle,
        ruta canónica,
        nombre,
        formato,
        tamaño,
        fecha,
        estado_de_escaneo
    }
```

La tabla ordenada permite `handle → objeto`. Los directorios se escanean de forma
diferida cuando Windows solicita sus hijos. Al iniciar una nueva sesión se vacía la
tabla y se reinicia el contador; por tanto, no se promete estabilidad entre sesiones.
Para vistas sintéticas futuras conviene reservar rangos de handles por proveedor o
mantener un índice compuesto `(storage_id, clave_lógica)`.

No puede afirmarse que DBI use este mismo algoritmo: su fuente MTP no es pública.

## 2.3 Operaciones corregidas

El documento de entrada asignaba `0x1002` a `GetStorageIDs`; ese código corresponde a
`OpenSession`. La tabla correcta es:

| Operación | Opcode | Dirección de datos |
|---|---:|---|
| `GetDeviceInfo` | `0x1001` | responder → iniciador |
| `OpenSession` | `0x1002` | sin fase de datos |
| `CloseSession` | `0x1003` | sin fase de datos |
| `GetStorageIDs` | `0x1004` | responder → iniciador |
| `GetStorageInfo` | `0x1005` | responder → iniciador |
| `GetObjectHandles` | `0x1007` | responder → iniciador |
| `GetObjectInfo` | `0x1008` | responder → iniciador |
| `GetObject` | `0x1009` | responder → iniciador |
| `DeleteObject` | `0x100B` | sin fase de datos |
| `SendObjectInfo` | `0x100C` | iniciador → responder |
| `SendObject` | `0x100D` | iniciador → responder |
| `GetPartialObject` | `0x101B` | responder → iniciador |

[Comandos PTP requeridos por Windows](https://learn.microsoft.com/en-us/windows-hardware/drivers/image/ptp-required-commands),
[guía de implementación MTP](https://www.nxp.com/docs/en/supporting-information/MTP_RESP_DEV_GUIDE.pdf)

### `GetStorageIDs` (`0x1004`)

Requiere una sesión abierta. La fase de datos contiene un `uint32` con la cantidad y
un array de `uint32 StorageID`. Después se devuelve `MTP_RESPONSE_OK`. En la versión
0.3.0 se entrega un único ID, `0x00010001`, asociado a `1: SD Card`.

### `GetObjectInfo` (`0x1008`)

El comando lleva el handle como parámetro. El dataset de respuesta incluye, en orden,
StorageID, formato, protección, tamaño comprimido de 32 bits, metadatos de miniatura,
dimensiones, parent handle, tipo/descripción de asociación, número de secuencia,
nombre UTF-16 MTP, fecha de creación, fecha de modificación y palabras clave. Un
directorio se representa con formato `Association` y asociación `GenericFolder`.

### `GetObject` (`0x1009`)

El responder resuelve el handle, abre la ruta solo para lectura, obtiene su tamaño y
envía un contenedor de datos. El motor lee por bloques y `usb:ds` fragmenta cada bloque
en transacciones bulk. El tamaño de paquete físico —512 bytes en USB High Speed— no
obliga a efectuar llamadas al sistema de archivos de 512 bytes.

### `SendObjectInfo` (`0x100C`) y `SendObject` (`0x100D`)

`SendObjectInfo` declara destino, padre, nombre, formato y tamaño. El responder debe
validar almacenamiento, espacio, ruta y política antes de reservar un handle. Solo si
responde correctamente puede seguir `SendObject`, que transporta el contenido binario.
La versión 0.3.0 no anuncia estas operaciones y marca el storage como
`READ_ONLY_WITHOUT_DELETE`.

La implementación futura debe usar este ciclo:

```text
SendObjectInfo
  → normalizar nombre/ruta
  → verificar política y espacio
  → crear archivo .partial exclusivo
  → reservar handle provisional

SendObject
  → escribir secuencialmente
  → actualizar SHA-256 y contador
  → flush + close
  → comprobar tamaño/hash
  → renombrar al destino
  → publicar ObjectAdded
```

## 2.4 Integridad y atomicidad

MTP no impide por sí mismo la corrupción. Su ventaja sobre UMS es que el host no
modifica directamente la FAT, el bitmap de asignación ni sectores arbitrarios: pide al
responder operaciones de objetos y Horizon conserva el control del filesystem. Sin
embargo, una desconexión durante `SendObject` puede dejar un archivo parcial y una
caída durante una actualización de metadatos todavía puede afectar FAT32/exFAT.

La integridad es una propiedad del diseño del responder:

- escritura en un nombre temporal no visible;
- contador exacto y límites máximos;
- `flush`, cierre y comprobación del resultado FS;
- validación de tamaño y, cuando exista, hash;
- renombrado final únicamente tras completar;
- eliminación del temporal al abortar o al iniciar la siguiente sesión;
- backpressure para no aceptar datos que aún no pueden persistirse.

En savedata, además, libnx documenta que las escrituras requieren un commit explícito;
el cierre o desmontaje no sustituye ese commit.
[fsdev de libnx](https://switchbrew.github.io/libnx/fs__dev_8h.html)

# 3. CAPA DE ABSTRACCIÓN DEL SISTEMA DE ARCHIVOS Y VIRTUALIZACIÓN (`fs`)

## 3.1 sysMMC, emuMMC y `fs.mitm`

La afirmación “`fs.mitm` implementa emuMMC” mezcla dos mecanismos diferentes:

- emuMMC redirige el acceso NAND hacia una partición o conjunto de archivos en SD a
  un nivel inferior del subsistema de almacenamiento;
- `fs.mitm` intercepta IPC de filesystem para funciones como LayeredFS, redirecciones
  de contenido y políticas adicionales.

Atmosphère documenta emuMMC como redirección de NAND a una partición o imágenes en la
SD, mientras que sus notas describen `fs.mitm` como interceptor de sesiones y montajes
de contenido. [Changelog de Atmosphère](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/changelog.md),
[arquitectura de Atmosphère](https://github.com/Atmosphere-NX/Atmosphere)

Una aplicación MTP normalmente llama a `fsp-srv` mediante libnx. Si se ejecuta dentro
de emuMMC, la capa del sistema ya presenta ese entorno como activo. El responder no
debe deducirlo observando nombres de carpetas; debe consultar la configuración/estado
del entorno y mostrarlo explícitamente antes de ofrecer vistas NAND.

## 3.2 Montaje de partidas guardadas

Los saves no son directorios planos de la SD. El flujo de ingeniería es:

1. Abrir un `FsSaveDataInfoReader` para el espacio deseado.
2. Enumerar `FsSaveDataInfo`, que contiene ID, aplicación, UID, tipo, espacio e índice.
3. Crear nodos MTP sintéticos para tipo, aplicación y usuario.
4. Al abrir un nodo, construir `FsSaveDataAttribute`.
5. Solicitar `fsOpenReadOnlySaveDataFileSystem()` o
   `fsdevMountSaveDataReadOnly()` para exportación.
6. Traducir el `FsFileSystem` devuelto a operaciones de enumeración y lectura.
7. Cerrar/desmontar el filesystem al liberar la vista o terminar la sesión.

El proceso no descifra manualmente el contenedor: Horizon FS y sus capas de seguridad
resuelven claves, integridad y representación interna antes de devolver un
`IFileSystem`. Para una restauración, se abre con escritura, se aplica staging, se
verifican todos los resultados y se ejecuta el commit requerido. Libnx expone lectores,
filtros y aperturas read-only específicas.
[Filesystem de libnx](https://switchbrew.github.io/libnx/fs_8h.html)

## 3.3 Vistas virtuales dinámicas

Una vista MTP no necesita corresponder a una ruta. Debe implementarse como proveedor:

```text
VirtualStorageProvider
├── enumerate(parent_key) → ObjectDescriptor[]
├── get_info(object_key) → ObjectInfo
├── open_read(object_key, offset, length) → stream
├── begin_write(...) → transacción o ReadOnly
├── commit_write(transaction)
└── abort_write(transaction)
```

El registro MTP global asigna handles a pares `(provider_id, object_key)`. Así:

- `SD Card` usa rutas reales;
- `Album` consulta el servicio de álbum y materializa capturas como objetos;
- `Saves` monta contenedores bajo demanda;
- una vista de aplicaciones consulta NCM y genera agrupaciones lógicas;
- un log sintético puede producir su contenido en RAM al atender `GetObject`.

Cuando el host pide hijos de una carpeta virtual, el responder invoca al proveedor,
genera descriptores, reserva handles y devuelve el array. `GetObject` no abre
necesariamente un archivo: puede leer un storage, serializar una estructura o producir
un stream. Este patrón evita codificar reglas de NAND, álbum y saves dentro del motor
MTP.

# 4. GESTIÓN DE RECURSOS DEL SISTEMA: OPTIMIZACIÓN DE MEMORIA Y CONCURRENCIA

## 4.1 Applet frente a aplicación

El valor de “~32 MB para applet” del documento de entrada no corresponde a la Switch.
En configuraciones comunes, el pool disponible para applets está en el orden de
cientos de MiB —frecuentemente citado alrededor de 442 MiB— y el de aplicaciones en
el orden de varios GiB —aproximadamente 3.2 GiB—. No son constantes contractuales:
dependen de firmware, redistribución de pools, sysmodules activos y título anfitrión.

La forma técnicamente correcta de dimensionar buffers es consultar en ejecución los
límites y uso mediante `svcGetSystemInfo`:

| Subtipo | Métrica |
|---:|---|
| 0 | memoria física total del pool Application |
| 1 | memoria física total del pool Applet |
| 0 con tipo Used | memoria usada por Application |
| 1 con tipo Used | memoria usada por Applet |

[SVC y límites de recursos](https://switchbrew.org/wiki/SVC)

El modo applet sigue siendo más restrictivo, pero un responder MTP no necesita caches
de GiB. Debe usar memoria acotada, escaneo diferido y bloques reutilizables. La página
relevante para buffers USB asíncronos es `0x1000` bytes. Un diseño inicial razonable
usa entre dos y cuatro buffers de 1–4 MiB, sujeto a medición; el paquete USB físico se
segmenta independientemente.

## 4.2 Modelo de concurrencia verificable

La distribución interna de DBI entre núcleos no está publicada. No debe presentarse
como hecho que DBI use ring buffers masivos o afinidades específicas.

Transferencia Switch 0.3.0 utiliza actualmente:

- hilo principal: máquina de estados MTP y llamadas FS;
- hilo de entrada: controles B/+ para detener el servidor;
- operaciones USB: URBs asíncronos en `usb:ds`, esperados de forma síncrona por la
  capa de transporte.

Para escritura y mayor rendimiento, la evolución recomendada es un pipeline acotado:

```text
Hilo USB RX
  → recibe bloques en buffers alineados
  → cola bounded / backpressure

Hilo de operación MTP
  → valida transacción, offset y estado
  → coordina proveedor y respuestas

Hilo de almacenamiento
  → escribe/lee FS
  → flush, hash, commit y finalización

Hilo UI/eventos
  → progreso, cancelación y estado
```

La cola debe tener estados `FREE`, `USB_FILLED`, `FS_BUSY`, `DONE`; ninguna capa puede
reusar un buffer hasta recibir confirmación de la siguiente. Las afinidades de CPU no
deben fijarse inicialmente: primero se miden tiempos USB, espera FS, fallos de cache y
contención. Solo después se evalúan `threadCreate`, prioridad y máscara de afinidad.

La concurrencia aumenta throughput cuando solapa SDMMC con USB, pero también amplía la
superficie de carrera. Cada transacción necesita propietario único, cancelación
idempotente y una transición terminal exactamente una vez.

# 5. MATRIZ DE ESPECIFICACIÓN TÉCNICA COMPARATIVA (DBI MTP VS. HEKATE UMS)

| Dimensión | DBI/Responder MTP | Hekate UMS |
|---|---|---|
| Entorno | Homebrew bajo Horizon/Atmosphère | Bootloader Nyx antes de Horizon |
| Protocolo host | PTP/MTP orientado a objetos | USB Mass Storage Bulk-Only + SCSI |
| Unidad de identidad | StorageID y ObjectHandle | LUN, LBA y sectores de 512 B |
| Propiedad del filesystem | Switch/Horizon conserva el montaje | Host monta y administra el volumen |
| Acceso físico | Mediante servicios FS/proveedores | Lectura/escritura SDMMC por sector |
| Vistas virtuales | Sí: SD, álbum, saves, vistas sintéticas | No: expone un dispositivo/partición |
| Letra de unidad | Normalmente no; dispositivo portátil | Normalmente sí, si el filesystem es reconocido |
| NAND/emuMMC | Puede ofrecer vistas lógicas y montajes | Puede exponer BOOT0/BOOT1/GPP como LUN |
| Control de permisos | Por operación y objeto | Read-only o write-protect a nivel LUN |
| Fallo durante escritura | Puede aislarse con staging | Puede interrumpir escrituras directas de sectores |
| Compatibilidad de apps | APIs WPD/MTP | APIs normales de archivos/bloques |
| Rendimiento | Overhead de objetos y metadatos | Flujo SCSI/BOT más directo |
| Caso óptimo | Navegación, vistas dinámicas, transferencias selectivas | Mantenimiento, clonación y acceso directo al medio |

Hekate implementa command wrappers BOT, comandos SCSI, LBA de 512 bytes y buffers
concurrentes entre USB y SDMMC. Es una arquitectura fundamentalmente distinta: el host
opera un disco, mientras MTP opera objetos administrados por el responder.
[Implementación UMS de Hekate](https://github.com/CTCaer/hekate/blob/master/bdk/usb/usb_gadget_ums.c)

## Fuentes técnicas principales

- [DBI: documentación pública del responder MTP](https://github.com/rashevskyv/dbi#run-mtp-responder)
- [USB Media Transfer Protocol Specification](https://www.pjrc.com/tmp/Media_Transfer_Protocol.pdf)
- [Windows Portable Devices](https://learn.microsoft.com/en-us/windows-hardware/drivers/portable/wpd-drivers-overview)
- [libnx `usb:ds`](https://switchbrew.github.io/libnx/usbds_8h.html)
- [Switchbrew USB services](https://switchbrew.org/wiki/USB_services)
- [libnx filesystem API](https://switchbrew.github.io/libnx/fs_8h.html)
- [Atmosphère](https://github.com/Atmosphere-NX/Atmosphere)
- [Hekate](https://github.com/CTCaer/hekate)
