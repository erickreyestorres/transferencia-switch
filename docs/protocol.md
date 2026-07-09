# Protocolo DBIbackend observado

El canal USB utiliza dispositivos con `VID 057E` y `PID 3000`. Cada comando comienza
con una cabecera little-endian de 16 bytes:

| Campo | Tipo | Descripción |
| --- | --- | --- |
| Firma | 4 bytes | `DBI0` |
| Tipo | `uint32` | solicitud `0`, respuesta `1`, confirmación `2` |
| Comando | `uint32` | salir `0`, rango `2`, lista `3` |
| Tamaño | `uint32` | bytes del contenido asociado |

Una solicitud de rango contiene `uint32 tamaño`, `uint64 desplazamiento`,
`uint32 longitud del nombre` y el nombre UTF-8. El backend envía los datos en bloques
de hasta 1 MiB; el ejecutable DBI analizado divide las operaciones USB internas en
bloques de hasta 4 MiB.

El protocolo observable no devuelve un resultado inequívoco de instalación. Por eso
la interfaz distingue entre **archivo enviado completamente** e **instalación
confirmada**; esta última no se afirma.

