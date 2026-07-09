"""Adaptador de archivos locales."""

from collections.abc import Iterable, Iterator
from pathlib import Path

from dbi_open.domain.models import FileDescriptor, FileNotAvailableError, TransferError


class LocalFileRepository:
    def __init__(self, files: Iterable[Path]) -> None:
        paths = [Path(path).resolve() for path in files]
        if not paths:
            raise TransferError("No hay archivos en la cola")
        if any(not path.is_file() for path in paths):
            raise TransferError("La cola contiene una ruta que no es un archivo")
        names = [path.name for path in paths]
        duplicates = sorted({name for name in names if names.count(name) > 1})
        if duplicates:
            raise TransferError(f"Hay nombres duplicados en la cola: {', '.join(duplicates)}")
        self.paths = {path.name: path for path in paths}

    def describe(self, name: str) -> FileDescriptor:
        path = self.paths.get(name)
        if path is None:
            raise FileNotAvailableError(f"Archivo no disponible: {name}")
        return FileDescriptor(name, path.stat().st_size)

    def iter_chunks(self, name: str, offset: int, size: int, chunk_size: int) -> Iterator[bytes]:
        descriptor = self.describe(name)
        if offset < 0 or size < 0 or offset + size > descriptor.size:
            raise TransferError(f"Rango fuera del archivo {name}")
        remaining = size
        with self.paths[name].open("rb") as handle:
            handle.seek(offset)
            while remaining:
                chunk = handle.read(min(chunk_size, remaining))
                if not chunk:
                    break
                remaining -= len(chunk)
                yield chunk
