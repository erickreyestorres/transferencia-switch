# `switch_app/source`

Código fuente de la app de Switch.

## Organización

- `domain/`: reglas puras y pequeñas. Idealmente sin dependencias de Switch/libnx.
- `application/`: casos de uso.
- `adapters/`: implementaciones concretas: MTP, SD, saves, NAND en lectura, UI,
  instalador SD.
- `main.cpp`: arma la app completa y conecta menú + MTP + storages.

## Archivos importantes en `adapters/`

- `sd_install_receiver.cpp`: recibe NSP/XCI por MTP e instala en SD usando NCM.
- `read_only_sd_database.cpp`: base de datos MTP y storages virtuales.
- `graphical_menu.cpp`: menú principal.
- `graphical_transfer_presenter.cpp`: pantalla de transferencia/resumen.
- `save_mounts.cpp`: montaje/listado de saves.
- `bis_mounts.cpp`: NAND USER/SYSTEM en modo lectura.
- `installed_catalog.cpp`: catálogo de juegos instalados.

