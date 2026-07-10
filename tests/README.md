# `tests`

Pruebas automáticas Python.

Sirven para proteger reglas del proyecto sin tener que probar todo manualmente en
la consola.

## Grupos principales

- `test_switch_architecture.py`: revisa arquitectura hexagonal.
- `test_sd_install_receiver.py`: reglas del instalador NSP/XCI en SD.
- `test_server.py`: comportamiento MTP.
- `test_mtp_read_only.py`: storages de solo lectura.
- `test_mtp_controlled_write.py`: escritura controlada en Inbox.
- `test_*_read_only.py`: seguridad de Album, BIS/NAND, Installed games, Saves.
- `test_menu_input.py`: navegación del menú.
- `test_package_fixtures.py`: genera NSP/XCI sintéticos para probar tamaños normales,
  mayores de 4 GiB y XCI con padding sin depender de archivos reales.
- `test_mtp_stream_simulator.py`: simula transferencias MTP por chunks, tamaño
  desconocido, padding, cancelación y errores de tamaño anunciado.

Comando:

```powershell
python -m unittest discover -s tests -q
```

Para una comprobación funcional completa de PC antes de probar en hardware:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\functional_smoke.ps1
```

