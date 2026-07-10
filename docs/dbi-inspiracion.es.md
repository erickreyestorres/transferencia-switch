# DBI como inspiración funcional

Este documento resume, a nivel de producto y arquitectura, funciones públicas
descritas por el proyecto DBI y cómo se comparan con `transferencia-switch`.

La intención no es copiar código ni clonar internamente DBI. El objetivo es entender
qué problemas resuelve, qué módulos deberíamos considerar y dónde podemos aportar un
plus propio: mejor documentación, pruebas automáticas, interfaz más clara, reportes de
fallos y flujos seguros.

Fuente pública revisada:

- <https://github.com/rashevskyv/dbi>

## Principios para nuestro proyecto

- Mantener implementación independiente.
- No usar código privado/no licenciado de terceros.
- Priorizar seguridad: NAND write deshabilitado hasta tener validación fuerte.
- Documentar cada módulo en español e inglés cuando madure.
- Agregar pruebas locales antes de depender de hardware.
- Mejorar experiencia visual: resultados claros, lista de fallos, logs y reportes.

## Matriz funcional

| Área DBI | Función observada | Estado en transferencia-switch | Prioridad sugerida | Plus propio posible |
|---|---|---:|---:|---|
| Instalación local | Instalar NSP/NSZ/XCI/XCZ desde SD/USB | Parcial: NSP/XCI sin compresión hacia SD | Alta | Reporte por archivo, lista de fallos, simulación previa |
| Instalación por PC | Instalación por USB con DBIbackend | Parcial histórico en `pc_backend`, foco actual MTP | Media | Backend moderno opcional con progreso claro |
| MTP responder | Exponer SD, NAND, juegos instalados, saves, gamecard | Parcial: SD, Inbox, NAND read-only, Installed, Saves, Album, Install SD | Alta | Storages seguros por defecto y UI más amigable |
| Gamecard | Dump/install desde cartucho | No implementado | Media | Modo read-only primero, logs y verificación |
| Servidor HTTP/red | Instalar desde servidor HTTP local/remoto | No implementado | Media | Catálogo web simple con validación previa |
| FTP | Servidor FTP para SD/instalación | No implementado | Baja/Media | Tal vez no prioritario si MTP funciona bien |
| Gestor de archivos | Copiar/mover/borrar/crear carpetas | No implementado como gestor completo | Baja | Mantener enfoque: transferencia segura, no file manager total |
| Juegos instalados | Ver apps, updates, DLC, tiempo, errores, mover/borrar | Parcial read-only: Installed games | Media | Diagnóstico visual y export de reporte |
| Tickets | Ver/borrar tickets | No implementado; instalación importa tickets | Baja/Media | Solo diagnóstico inicialmente, sin borrar |
| Saves | Ver, backup, restore | Parcial read-only por MTP | Alta | Sistema simple de backup/restore con nombres legibles |
| Activity log | Ver actividad por aplicación | No implementado | Baja | Export CSV/JSON futuro |
| Configuración | `dbi.config` con muchas opciones | No implementado formalmente | Media | `transferencia-switch.ini` simple y documentado |
| Archivos comprimidos | NSZ/XCZ, ZIP/RAR/CBR/CBZ | No implementado | Media | NSZ/XCZ antes que archivos genéricos |
| Visor de archivos | JPG/PNG/PSD/texto/hex | No implementado | Baja | Hex/log viewer solo para diagnóstico |
| Interfaz | Menús funcionales estilo consola | Parcial: menú gráfico propio | Alta | Tarjetas grandes, táctil, estado claro, animación |
| Logs/errores | Mensajes operativos | Parcial: `install.log` y resumen UI | Alta | Reporte por sesión, fallos exportables, rotación diaria |

## Módulos candidatos para nuestra arquitectura hexagonal

### 1. `install`

Responsable de instalación SD:

- NSP/XCI actual.
- Futuro NSZ/XCZ.
- Validación previa de paquete.
- Resultado por archivo.
- Rollback seguro.

### 2. `mtp`

Responsable de exponer vistas virtuales:

- SD read-only.
- Inbox controlado.
- Install SD.
- Installed games read-only.
- Saves read-only / backup futuro.
- Gamecard read-only futuro.

### 3. `package_inspection`

Responsable de analizar paquetes sin instalar:

- PFS0/HFS0.
- Tamaño inferido.
- CNMT count.
- Padding/trailing bytes.
- Simulación de streams grandes.

Este módulo ya empezó en `tools/package_fixtures.py` y `tools/mtp_stream_simulator.py`;
debería evolucionar hacia lógica compartida o especificación de comportamiento.

### 4. `saves`

Responsable de partidas guardadas:

- Listar saves.
- Backup a SD/PC.
- Restore con confirmación.
- Validar tamaño/ruta.
- Nombrar carpetas legibles por juego/usuario/fecha.

### 5. `installed_catalog`

Responsable de inventario:

- Juegos instalados.
- Updates/DLC.
- Tamaño.
- Title ID.
- Estado/diagnóstico.
- Export futuro.

### 6. `ui`

Responsable de experiencia en consola:

- Menú gráfico.
- Transferencia con progreso.
- Lista de éxitos/fallos.
- Mensajes grandes y legibles.
- Táctil como deseable.

### 7. `diagnostics`

Responsable de reportes:

- `install.log`.
- Resumen de sesión.
- Rotación.
- Export JSON/TXT.
- Próximo paso sugerido según error.

## Plus propios que podrían diferenciarnos

1. **Simulador antes de hardware**
   - DBI es muy completo, pero nosotros podemos hacer que cada cambio pase por
     fixtures y reportes locales antes de probar en Switch.

2. **Reporte claro de fallos**
   - No solo “fallaron 2”: listar archivo, causa, tamaño, punto de falla y acción
     sugerida.

3. **Modo seguro por defecto**
   - NAND read-only y NAND install ausente hasta tener pruebas reales.

4. **Documentación bilingüe**
   - Español primero para el uso diario del proyecto, inglés para colaboración pública.

5. **Backup de saves simple**
   - Un flujo tipo “Respaldar todo / Restaurar seleccionado”, más amable para usuarios
     no técnicos.

6. **UI moderna para consola**
   - Tarjetas grandes, iconos claros, mensajes grandes, progreso entendible.

## Roadmap propuesto inspirado por DBI

### Corto plazo

- Mantener MTP + Install SD estable.
- Mejorar simulaciones con casos reales inspeccionados.
- Reporte visual/exportable de fallos.
- Validar XCI grandes con hardware.

### Mediano plazo

- Backup de saves.
- Gamecard read-only/dump.
- Configuración `.ini` simple.
- Inventario de installed games más completo.

### Largo plazo

- NSZ/XCZ.
- Restore de saves.
- Instalación por red/HTTP.
- Táctil completo.
- Posible cliente PC opcional con UX propia.
