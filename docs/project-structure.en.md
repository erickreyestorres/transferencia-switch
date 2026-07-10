# Project structure

This document explains what each folder contains and why it exists.

```text
transferencia-switch/
├─ switch_app/      Real Nintendo Switch homebrew application.
├─ pc_backend/      Legacy USB backend/prototype, kept for tests.
├─ tests/           Python automated tests.
├─ docs/            Technical and product documentation.
├─ scripts/         Convenience scripts for build, tests, and legacy tools.
├─ tools/           Maintenance helper assets.
├─ README*.md       Main documentation in Spanish and English.
└─ .gitignore       Rules to avoid committing binaries and temporary files.
```

## `switch_app/`

The actual Switch application. This is what builds the `.nro`.

- `source/`: C/C++ implementation.
- `include/`: public headers.
- `romfs/`: final runtime assets packaged into the NRO.
- `data/`: source/design assets.
- `vendor/`: third-party code.
- `tests/`: native C tests.
- `Makefile`: devkitPro build recipe.
- `icon.jpg`: Homebrew Menu icon.

## Hexagonal layout

- `domain/`: pure domain rules.
- `application/`: use cases.
- `adapters/`: integration with MTP, SD, libnx, UI, saves, install receiver.
- `main.cpp`: Switch app entry point and wiring.

## Assets

- `switch_app/data/` contains editable/source assets.
- `switch_app/romfs/` contains final assets included in the NRO.

## `tests/`

Python tests that verify architecture, MTP behavior, read-only rules, virtual
storages, logs, and SD install safety.

```powershell
python -m unittest discover -s tests -q
```

## `tools/`

Auxiliary project maintenance tools. These are not part of the final Switch app.
