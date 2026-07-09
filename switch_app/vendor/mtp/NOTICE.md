# MTP third-party component

This directory contains selected files derived from the Android Open Source Project
MTP implementation and its Nintendo Switch port in `retronx-team/mtp-server-nx`.

- Upstream: https://github.com/retronx-team/mtp-server-nx
- License: Apache License 2.0
- Local changes: mode-dependent operation advertisement, device identity, controlled
  inbox writes, staged file reception, progress reporting and storage access
  capability were adapted for Transferencia Switch.

The GPL-3.0 `SwitchMtpDatabase.h` file from the upstream repository is deliberately
not included. This project provides its own storage database adapter.
