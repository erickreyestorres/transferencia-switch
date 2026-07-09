# Arquitectura hexagonal

## Regla principal

Las dependencias apuntan hacia el centro. El dominio no conoce USB, ventanas,
archivos locales ni detalles del protocolo externo.

```text
Interfaz / USB / archivos
          │
          ▼
      Adaptadores
          │ implementan
          ▼
        Puertos
          │ usados por
          ▼
     Casos de uso
          │
          ▼
        Dominio
```

## Capas actuales

- `domain/`: modelos, errores y cálculo de cobertura de rangos.
- `application/ports.py`: contratos para archivos, escritura, eventos, reloj y cancelación.
- `application/transfer_file_range.py`: caso de uso para transferir un rango.
- `adapters/local_files.py`: lectura desde archivos reales del PC.
- `adapters/runtime.py`: callbacks, reloj, cancelación y endpoint de escritura.
- `server.py`: adaptador conductor del protocolo DBI y composición de dependencias.
- `ui.py`: adaptador gráfico; no contiene reglas de transferencia.

## Estrategia de pruebas

- Dominio: reglas puras y casos límite.
- Aplicación: dobles en memoria para los puertos.
- Adaptadores: archivos temporales y endpoints falsos.
- Integración: servidor y protocolo completo sin hardware.
- Hardware: prueba posterior con consola y cable USB.

## Aplicación de Switch

La aplicación usa C/C++ y aplica el mismo diseño:

- `include/transfer_switch/domain` y `source/domain`: catálogo y errores sin libnx.
- `adapters/read_only_sd_database`: traduce archivos y directorios a objetos MTP con
  identificadores estables durante la sesión. Es de solo lectura por defecto; la
  escritura requiere una habilitación explícita y queda confinada al root recibido.
- `domain/safe_path`: regla pura que valida que cada destino sea hijo directo del
  directorio MTP solicitado y evita recorridos fuera del storage.
- `ports/transfer_observer` y `adapters/console_transfer_presenter`: separan los eventos
  del motor de transferencia de su presentación en la pantalla de Switch.
- `vendor/mtp`: motor Apache 2.0 para paquetes, sesiones, operaciones y USB MTP.
- `main.cpp`: composición de dependencias, menú y ciclo de vida del responder.
- Los módulos `dbi_catalog` y `libnx_usb_transport` pertenecen al diagnóstico privado
  anterior y no participan en el modo MTP.

El flujo registra una interfaz USB Still Image/MTP con endpoints bulk IN, bulk OUT e
interrupt IN. En lectura, `1: SD Card` se anuncia como `READ_ONLY_WITHOUT_DELETE` y no
publica operaciones mutables. El modo de recepción publica un storage independiente,
`10: Safe Inbox`, y habilita únicamente `SendObjectInfo` y `SendObject`. Cada archivo se
recibe en staging, se valida y luego se renombra a su destino final.

Desde 0.4.1 la base mantiene un mapa `StorageID -> StorageRoot`: cada raíz conserva su
propia política de escritura y estado de enumeración. Esto permite publicar SD e Inbox
simultáneamente sin conceder escritura a la SD y prepara la incorporación de proveedores
virtuales descritos en [`vistas-virtuales.md`](vistas-virtuales.md).

Nand USER y Nand SYSTEM usan adaptadores de montaje BIS separados. Aunque el filesystem
subyacente pueda ofrecer más capacidades al proceso, ambos roots se registran con
política no escribible, su `MtpStorage` anuncia solo lectura y el servidor no publica
borrado ni movimiento. El montaje corresponde al entorno activo que Horizon presenta
al proceso, por lo que debe registrarse si la sesión se ejecuta en sysMMC o emuMMC.

El archivo GPL `SwitchMtpDatabase.h` del proyecto de referencia no se incorporó. El
repositorio SD de este proyecto es una implementación propia.
