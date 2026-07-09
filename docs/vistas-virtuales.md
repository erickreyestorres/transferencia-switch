# Diseño de vistas virtuales MTP

## Principio

Una vista MTP no tiene que ser una partición física. Cada storage se implementará
mediante un proveedor que enumera objetos, asigna `ObjectHandle` y entrega una fuente
de lectura o un destino controlado. Si una API o montaje no está disponible, la vista
no se publica; la sesión MTP debe continuar funcionando con las demás.

## Registro previsto

| Número | Vista | Fuente | Política inicial |
|---:|---|---|---|
| 1 | SD Card | `sdmc:/` | Solo lectura |
| 2 | Nand USER | `fsOpenBisFileSystem(User)` | Solo lectura |
| 3 | Nand SYSTEM | `fsOpenBisFileSystem(System)` | Solo lectura y advertencia |
| 4 | Installed games | NS + catálogo generado | Metadatos de solo lectura |
| 5 | SD install | Receptor de acción NCM | NSP/XCI experimental, destino SD |
| 6 | NAND install | Receptor de acción | Deshabilitado hasta validación |
| 7 | Saves | lector + montaje individual | Respaldo de solo lectura |
| 8 | Album | servicios `caps:*` | Solo lectura |
| 9 | Gamecard | filesystem del cartucho | Respaldo de cartucho propio |
| 10 | Safe Inbox | `sdmc:/switch/transferencia-switch/inbox/` | Escritura controlada |

La numeración visible seguirá la referencia funcional cuando sea posible. El Inbox es
una extensión propia y permanecerá separado de los storages sensibles.

## Tipos de proveedor

### Directorio montado

Enumera una ruta POSIX real. SD e Inbox usan actualmente este modelo. NAND podrá usarlo
solo después de abrir y montar el filesystem BIS correspondiente.

### Árbol dinámico

Genera carpetas y archivos que no existen como un único árbol físico. Saves, Album e
Installed games requieren este modelo. Los identificadores serán monotónicos y válidos
únicamente durante la sesión MTP.

### Receptor de acción

Recibe un objeto, lo valida y ejecuta una operación distinta de una copia ordinaria.
Las vistas de instalación pertenecen a esta categoría y no se habilitarán como simples
directorios grabables.

## Orden de implementación

1. Registro multi-storage para SD e Inbox en una misma sesión.
2. `8: Album` de solo lectura mediante los filesystems oficiales de imagen. Primera
   versión implementada como `Album (SD)` y `Album (NAND)` opcionales.
3. `7: Saves` con enumeración y montajes de solo lectura. Primera versión implementada
   con una raíz virtual y un máximo conservador de 20 montajes simultáneos.
4. Detección del entorno activo y NAND USER/SYSTEM de solo lectura.
5. `4: Installed games` como catálogo read-only de metadatos e iconos. Primera
   versión implementada sin exponer ni extraer binarios instalados.
6. Gamecard físico para respaldo personal.
7. `5: SD Card install` mediante streaming PFS0 hacia placeholders NCM. Primera
   versión implementada para NSP/XCI sin compresión, un CNMT y menos de 4 GiB.
8. Formatos comprimidos y objetos MTP grandes; después `6: NAND install` reutilizando el mismo caso
   de uso con otro destino.

## Reglas de seguridad

- NAND SYSTEM nunca se publica con capacidad de escritura.
- Los saves comienzan como exportación; restaurar será una operación separada.
- Si existen más de 20 saves, la interfaz informa cuántos quedaron fuera del primer
  montaje. Una versión posterior reemplazará el límite con montaje bajo demanda.
- Un error al montar un proveedor no debe detener SD ni Inbox.
- No se reutilizan `ObjectHandle` durante una sesión.
- Ningún proveedor puede construir rutas fuera de su raíz o montaje.
- Los archivos virtuales grandes deben transmitirse por streaming, sin cargarse completos
  en memoria.

## APIs base verificadas

- [libnx `fs.h`: BIS, saves, image directory y gamecard](https://switchbrew.github.io/libnx/fs_8h.html)
- [libnx `caps.h`: álbum, capturas y videos](https://switchbrew.github.io/libnx/caps_8h.html)
- [DBI: lista funcional de storages MTP](https://github.com/rashevskyv/dbi#run-mtp-responder)
