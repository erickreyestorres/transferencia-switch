# Backup de saves — diseño inicial seguro

Este documento describe la base del futuro sistema de respaldo de partidas guardadas.

Estado actual:

- `7: Saves` ya expone saves como lectura mediante MTP.
- La app monta saves con `fsdevMountSaveDataReadOnly`.
- No existe restore.
- No se borran ni modifican saves.

## Objetivo de la primera fase

Crear backups legibles y seguros en la SD, sin modificar la partida original.

Ruta propuesta:

```text
sdmc:/switch/transferencia-switch/backups/saves/
  YYYY-MM-DD/
    0100ABCDEF123456_user_55667788/
      manifest.json
      files/
```

## Módulo portable agregado

Dominio:

- `switch_app/include/transfer_switch/domain/save_backup_plan.h`
- `switch_app/source/domain/save_backup_plan.c`

Este módulo no depende de libnx ni de MTP. Solo genera:

- nombre seguro de carpeta;
- ruta base del backup;
- ruta `files/`;
- ruta `manifest.json`;
- contenido JSON básico del manifiesto.

Caso de uso portable:

- `switch_app/include/transfer_switch/application/plan_save_backups.h`
- `switch_app/source/application/plan_save_backups.c`

Este caso de uso recibe una lista de saves candidatos y genera planes de backup,
incluyendo un resumen:

- cantidad solicitada;
- cantidad planificada;
- cantidad fallida;
- primer error encontrado.

Todavía no copia archivos. Es una fase intermedia para que la UI pueda mostrar qué
se va a respaldar antes de ejecutar el backup real.

## Manifest v1

Ejemplo:

```json
{
  "schema": "transferencia-switch.save-backup.v1",
  "application_id": "0100ABCDEF123456",
  "user_id_low": "1122334455667788",
  "user_id_high": "99AABBCCDDEEFF00",
  "date": "2026-07-10",
  "files_dir": "files"
}
```

## Reglas de seguridad

- El origen de save debe seguir siendo read-only.
- El destino debe estar bajo `sdmc:/switch/transferencia-switch/backups/saves`.
- Restore queda fuera de esta fase.
- No se agregan llamadas a `fsFileWrite` dentro del adaptador read-only de saves.
- La escritura futura del backup deberá vivir en un adaptador separado y controlado.

## Próximos pasos

1. Agregar un puerto abstracto para copiar árboles de archivos.
2. Implementar adaptador Switch que copie desde el mount read-only hacia la SD.
3. Agregar resumen de resultado por save.
4. Mostrar confirmación previa en UI.
5. Solo después evaluar restore con confirmación explícita.
