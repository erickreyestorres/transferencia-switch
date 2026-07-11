"""Analiza install.log de Transferencia Switch.

Convierte el log de hardware en un resumen claro para estabilizar transferencias:

- correctos;
- ya instalados/reutilizados;
- fallidos;
- cancelados;
- motivos de error.
"""

from __future__ import annotations

import argparse
import re
from collections import Counter
from dataclasses import dataclass
from pathlib import Path


KEY_VALUE_RE = re.compile(r'(\w+)=("[^"]*"|\S+)')


@dataclass
class TransferRecord:
    file: str
    package_type: str = ""
    status: str = "INICIADO"
    detail: str = ""
    received: str = ""
    cnmt: int = 0
    new_nca: int = 0
    existing_nca: int = 0

    @property
    def already_installed(self) -> bool:
        if self.existing_nca > 0 and self.new_nca == 0 and self.status == "OK":
            return True
        if self.status == "OK" and self.new_nca == 0 and self.cnmt > 0:
            return True
        return "ya instalado" in self.detail or "contenido existente" in self.detail


def _parse_key_values(line: str) -> dict[str, str]:
    result: dict[str, str] = {}
    for key, value in KEY_VALUE_RE.findall(line):
        if value.startswith('"') and value.endswith('"'):
            value = value[1:-1]
        result[key] = value
    return result


def _as_int(value: str | None) -> int:
    if not value:
        return 0
    try:
        return int(value)
    except ValueError:
        return 0


def analyze_install_log(path: Path) -> list[TransferRecord]:
    records: list[TransferRecord] = []
    current: TransferRecord | None = None

    for raw_line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "[transferencia-switch]" not in raw_line:
            continue
        line = raw_line.split("] ", 1)[-1]
        values = _parse_key_values(line)

        if line.startswith("START "):
            current = TransferRecord(
                file=values.get("file", "(sin nombre)"),
                package_type=values.get("type", ""),
                status="INICIADO",
            )
            records.append(current)
            continue

        if current is None and "file" in values:
            current = TransferRecord(file=values["file"])
            records.append(current)

        if current is None:
            continue

        if line.startswith("OK "):
            current.status = "OK"
            current.file = values.get("file", current.file)
            current.package_type = values.get("type", current.package_type)
            current.received = values.get("received", current.received)
            current.cnmt = _as_int(values.get("cnmt"))
            current.new_nca = _as_int(values.get("nca"))
            current.existing_nca = _as_int(values.get("existing_nca"))
            current.detail = values.get("detail", current.detail)
        elif line.startswith("ERROR "):
            current.status = "FALLIDO"
            current.file = values.get("file", current.file)
            current.package_type = values.get("type", current.package_type)
            current.received = values.get("received", current.received)
            current.detail = values.get("detail", current.detail)
        elif line.startswith("CANCEL "):
            if current.status != "FALLIDO":
                current.status = "CANCELADO"
            current.file = values.get("file", current.file)
            current.package_type = values.get("type", current.package_type)
            current.received = values.get("received", current.received)
            current.detail = values.get("detail", current.detail)

    return records


def format_summary(records: list[TransferRecord]) -> str:
    ok = [record for record in records if record.status == "OK" and not record.already_installed]
    already = [record for record in records if record.status == "OK" and record.already_installed]
    failed = [record for record in records if record.status == "FALLIDO"]
    cancelled = [record for record in records if record.status == "CANCELADO"]
    started = [record for record in records if record.status == "INICIADO"]
    reasons = Counter(record.detail or "(sin detalle)" for record in failed + cancelled)

    lines = [
        "Resumen install.log",
        "===================",
        f"Total registros: {len(records)}",
        f"OK nuevos: {len(ok)}",
        f"Ya instalados/reutilizados: {len(already)}",
        f"Fallidos: {len(failed)}",
        f"Cancelados: {len(cancelled)}",
        f"Iniciados sin cierre: {len(started)}",
        "",
    ]

    def add_group(title: str, group: list[TransferRecord]) -> None:
        if not group:
            return
        lines.append(title)
        lines.append("-" * len(title))
        for record in group:
            suffix = f" | {record.detail}" if record.detail else ""
            if record.status == "OK":
                suffix += f" | nuevas={record.new_nca} existentes={record.existing_nca}"
                if record.already_installed and record.existing_nca == 0:
                    suffix += " | posible contenido ya instalado (log antiguo sin existing_nca)"
            lines.append(f"- [{record.package_type or '?'}] {record.file}{suffix}")
        lines.append("")

    add_group("OK nuevos", ok)
    add_group("Ya instalados/reutilizados", already)
    add_group("Fallidos", failed)
    add_group("Cancelados", cancelled)
    add_group("Iniciados sin cierre", started)

    if reasons:
        lines.append("Motivos agrupados")
        lines.append("----------------")
        for reason, count in reasons.most_common():
            lines.append(f"- {count}x {reason}")
        lines.append("")

    return "\n".join(lines).rstrip() + "\n"


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("path", type=Path)
    args = parser.parse_args()

    records = analyze_install_log(args.path)
    print(format_summary(records))
    return 0 if not any(record.status == "FALLIDO" for record in records) else 2


if __name__ == "__main__":
    raise SystemExit(main())
