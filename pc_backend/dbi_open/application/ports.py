"""Puertos requeridos por el núcleo de la aplicación."""

from collections.abc import Iterator
from typing import Any, Protocol

from dbi_open.domain.models import FileDescriptor


class FileRepository(Protocol):
    def describe(self, name: str) -> FileDescriptor: ...
    def iter_chunks(self, name: str, offset: int, size: int, chunk_size: int) -> Iterator[bytes]: ...


class ByteWriter(Protocol):
    def write(self, payload: bytes) -> int: ...


class EventPublisher(Protocol):
    def publish(self, event_type: str, **payload: Any) -> None: ...


class CancellationToken(Protocol):
    @property
    def is_cancelled(self) -> bool: ...


class Clock(Protocol):
    def monotonic(self) -> float: ...
