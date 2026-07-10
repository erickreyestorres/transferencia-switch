# Transferencia Switch

Transferencia Switch is an experimental Nintendo Switch homebrew application.
Its goal is to provide a clearer and safer MTP experience for transferring,
backing up, and, in a controlled way, installing content from Windows.

This project is being built as an independent implementation. DBI is used only as
a public behavioral and user-experience reference; no private code is copied or
required.

## Current status

- The console appears in Windows as an MTP device.
- `1: SD Card` exposes the SD card as read-only.
- `10: Safe Inbox` receives files into a controlled folder.
- `5: SD Card install` installs uncompressed NSP/XCI packages to SD through NCM
  placeholders.
- NAND install remains disabled.
- NAND USER, NAND SYSTEM, Saves, Album, and Installed games remain non-writable.
- The graphical UI includes icons, transfer animation, per-file summaries, and a
  visible failure list.
- Installation errors are logged to:

  `sdmc:/switch/transferencia-switch/logs/install.log`

- The log rotates automatically after 512 KB:

  `install.log` → `install.previous.log`

## Collaboration

The project is open to technical collaboration and review. We are especially
interested in contributions around:

- stable MTP transfers for large files;
- robust support for XCI files above 4 GB;
- XCI/CXCI/trimmed parsing;
- NSZ/XCZ;
- preventing sleep during long transfers;
- console UI and touch usability improvements.

If the DBI team or other homebrew developers find any of these ideas useful for
existing projects, we are open to coordinating, learning, or adapting the approach.
This repository aims to be an independent and documented implementation, not a
copy of private code.

## Known limitations

- Large XCI files above 4 GB still require MTP reception with unknown size
  (`0xFFFFFFFF`). The app now prepares the install storage so Windows can start
  those transfers, but it still rejects them with an explicit error until safe
  streaming reception is implemented.
- NSZ/XCZ are not supported yet.
- NAND installation is intentionally disabled for safety.
- Console auto-sleep can interrupt long transfers.
- Touch input exists in the graphical menu, but it is not the main priority yet.

## Structure

- `switch_app/`: Nintendo Switch homebrew application.
- `switch_app/vendor/mtp/`: MTP core with Apache 2.0 license and local notices.
- `pc_backend/`: legacy private USB backend kept for tests and diagnostics.
- `tests/`: Python tests for architecture, MTP behavior, and safety rules.
- `docs/`: technical and product documentation.

For a folder-by-folder explanation, see
[`docs/project-structure.en.md`](docs/project-structure.en.md).

## Build

Requires Docker Desktop and the `devkitpro/devkita64` image:

```powershell
docker run --rm -v "${PWD}:/work" -w /work/switch_app `
  devkitpro/devkita64:latest bash -lc "source /opt/devkitpro/devkita64.sh && make clean && make -j2"
```

The output is generated at:

`switch_app/transferencia_switch.nro`

## Tests

```powershell
python -m unittest discover -s tests -q
powershell -ExecutionPolicy Bypass -File .\test_switch_core.ps1
```

## Safety stance

This project prioritizes safe paths:

- NAND is not written.
- General SD access is exposed as read-only.
- Real installation is limited to `5: SD Card install`.
- Failed installs attempt to roll back newly created content when possible.
- Logs are used to diagnose failures with data instead of guesses.

This project is intended for development, research, and use with owned content or
legitimate backups. Users are responsible for complying with the laws and terms
that apply in their jurisdiction.
