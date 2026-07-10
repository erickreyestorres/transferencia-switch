"""Genera e inspecciona fixtures NSP/XCI sintéticos para pruebas locales.

Los archivos creados son *sparse* cuando el sistema de archivos lo permite: pueden
simular tamaños de varios GiB sin ocupar ese espacio real en disco.

No instala nada y no modifica archivos existentes salvo la ruta de salida indicada.
"""

from __future__ import annotations

import argparse
import os
import struct
from dataclasses import dataclass
from pathlib import Path


PFS0_MAGIC = b"PFS0"
HFS0_MAGIC = b"HFS0"
XCI_ROOT_OFFSET = 0xF000


@dataclass(frozen=True)
class PackageInfo:
    kind: str
    inferred_size: int
    entries: tuple[str, ...]


def _write_sparse(path: Path, chunks: list[tuple[int, bytes]], size: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as handle:
        for offset, payload in sorted(chunks):
            handle.seek(offset)
            handle.write(payload)
        handle.truncate(size)


def _pfs0_header(entries: list[tuple[str, int, int]]) -> bytes:
    names = b""
    encoded: list[tuple[int, int, int]] = []
    for name, offset, size in entries:
        name_offset = len(names)
        names += name.encode("utf-8") + b"\0"
        encoded.append((offset, size, name_offset))

    header = struct.pack("<4sIII", PFS0_MAGIC, len(entries), len(names), 0)
    table = b"".join(struct.pack("<QQII", offset, size, name_offset, 0) for offset, size, name_offset in encoded)
    return header + table + names


def _hfs0_header(entries: list[tuple[str, int, int]]) -> bytes:
    names = b""
    encoded: list[tuple[int, int, int]] = []
    for name, offset, size in entries:
        name_offset = len(names)
        names += name.encode("utf-8") + b"\0"
        encoded.append((offset, size, name_offset))

    header = struct.pack("<4sIII", HFS0_MAGIC, len(entries), len(names), 0)
    table = b"".join(
        struct.pack("<QQIIQ32s", offset, size, name_offset, 0, 0, b"\0" * 32)
        for offset, size, name_offset in encoded
    )
    return header + table + names


def create_nsp(path: Path, payload_size: int) -> PackageInfo:
    content_id = "00112233445566778899aabbccddeeff"
    entries = [(f"{content_id}.cnmt.nca", 0, payload_size)]
    header = _pfs0_header(entries)
    total_size = len(header) + payload_size
    _write_sparse(path, [(0, header), (len(header), b"NCA3")], total_size)
    return PackageInfo("NSP", total_size, tuple(entry[0] for entry in entries))


def create_xci(path: Path, secure_payload_size: int, trailing_padding: int = 0) -> PackageInfo:
    content_id = "00112233445566778899aabbccddeeff"
    secure_entries = [(f"{content_id}.cnmt.nca", 0, secure_payload_size)]
    secure_header = _hfs0_header(secure_entries)
    secure_size = len(secure_header) + secure_payload_size
    root_header = _hfs0_header([("secure", 0, secure_size)])
    secure_offset = XCI_ROOT_OFFSET + len(root_header)
    inferred_size = secure_offset + secure_size
    total_size = inferred_size + trailing_padding
    _write_sparse(
        path,
        [
            (XCI_ROOT_OFFSET, root_header),
            (secure_offset, secure_header),
            (secure_offset + len(secure_header), b"NCA3"),
        ],
        total_size,
    )
    return PackageInfo("XCI", inferred_size, tuple(entry[0] for entry in secure_entries))


def _read_c_string(table: bytes, offset: int) -> str:
    end = table.find(b"\0", offset)
    if end < 0:
        raise ValueError("nombre sin terminador")
    return table[offset:end].decode("utf-8")


def _read_pfs0(path: Path, offset: int = 0, hfs: bool = False) -> tuple[int, list[tuple[str, int, int]]]:
    entry_size = 0x40 if hfs else 0x18
    magic_expected = HFS0_MAGIC if hfs else PFS0_MAGIC
    with path.open("rb") as handle:
        handle.seek(offset)
        raw = handle.read(16)
        magic, count, string_size, _ = struct.unpack("<4sIII", raw)
        if magic != magic_expected:
            raise ValueError(f"magic inválido: {magic!r}")
        table = handle.read(count * entry_size)
        strings = handle.read(string_size)

    entries: list[tuple[str, int, int]] = []
    for index in range(count):
        raw_entry = table[index * entry_size : (index + 1) * entry_size]
        data_offset, size, string_offset = struct.unpack("<QQI", raw_entry[:20])
        entries.append((_read_c_string(strings, string_offset), data_offset, size))
    header_size = 16 + count * entry_size + string_size
    return header_size, entries


def inspect_package(path: Path) -> PackageInfo:
    with path.open("rb") as handle:
        magic = handle.read(4)
    if magic == PFS0_MAGIC:
        header_size, entries = _read_pfs0(path)
        inferred = header_size + max((offset + size for _, offset, size in entries), default=0)
        return PackageInfo("NSP", inferred, tuple(name for name, _, _ in entries))

    root_header_size, root_entries = _read_pfs0(path, XCI_ROOT_OFFSET, hfs=True)
    data_start = XCI_ROOT_OFFSET + root_header_size
    inferred = data_start + max((offset + size for _, offset, size in root_entries), default=0)
    secure = next((entry for entry in root_entries if entry[0] == "secure"), None)
    if secure is None:
        raise ValueError("XCI sin partición secure")
    secure_offset = data_start + secure[1]
    _, secure_entries = _read_pfs0(path, secure_offset, hfs=True)
    return PackageInfo("XCI", inferred, tuple(name for name, _, _ in secure_entries))


def simulate_stream(path: Path, chunk_size: int = 16 * 1024) -> dict[str, int | str | bool]:
    """Simula una recepción por chunks y detecta cuándo el parser sabría detenerse.

    Esta función no interpreta NCAs ni llama servicios de Switch. Su objetivo es probar
    la parte mecánica que más nos interesa para MTP grande: cuánto se debe leer hasta
    conocer el tamaño real del paquete y si el archivo del host trae bytes extra.
    """
    if chunk_size <= 0:
        raise ValueError("chunk_size debe ser positivo")

    host_size = path.stat().st_size
    inspected = inspect_package(path)
    transferred = 0
    while transferred < host_size and transferred < inspected.inferred_size:
        transferred += min(chunk_size, inspected.inferred_size - transferred)

    return {
        "kind": inspected.kind,
        "host_size": host_size,
        "inferred_size": inspected.inferred_size,
        "transferred_until_complete": transferred,
        "complete": transferred == inspected.inferred_size,
        "trailing_bytes": max(0, host_size - inspected.inferred_size),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    make_nsp = sub.add_parser("make-nsp")
    make_nsp.add_argument("path", type=Path)
    make_nsp.add_argument("--payload-size", type=int, default=2 * 1024 * 1024)

    make_xci = sub.add_parser("make-xci")
    make_xci.add_argument("path", type=Path)
    make_xci.add_argument("--secure-payload-size", type=int, default=2 * 1024 * 1024)
    make_xci.add_argument("--trailing-padding", type=int, default=0)

    inspect = sub.add_parser("inspect")
    inspect.add_argument("path", type=Path)

    simulate = sub.add_parser("simulate-stream")
    simulate.add_argument("path", type=Path)
    simulate.add_argument("--chunk-size", type=int, default=16 * 1024)

    args = parser.parse_args()
    if args.command == "make-nsp":
        info = create_nsp(args.path, args.payload_size)
    elif args.command == "make-xci":
        info = create_xci(args.path, args.secure_payload_size, args.trailing_padding)
    elif args.command == "inspect":
        info = inspect_package(args.path)
    else:
        result = simulate_stream(args.path, args.chunk_size)
        for key, value in result.items():
            print(f"{key}={value}")
        return 0

    disk_size = os.path.getsize(args.path)
    print(f"kind={info.kind}")
    print(f"inferred_size={info.inferred_size}")
    print(f"file_size={disk_size}")
    print("entries=" + ",".join(info.entries))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
