"""Estructuras del protocolo DBIbackend observables en el cliente público."""

from __future__ import annotations

from dataclasses import dataclass
from enum import IntEnum
import struct


MAGIC = b"DBI0"
HEADER = struct.Struct("<4sIII")
FILE_RANGE = struct.Struct("<IQI")


class CommandType(IntEnum):
    REQUEST = 0
    RESPONSE = 1
    ACK = 2


class CommandId(IntEnum):
    EXIT = 0
    LIST_OLD = 1
    FILE_RANGE = 2
    LIST = 3


class ProtocolError(RuntimeError):
    """Indica una trama inválida o incompatible."""


@dataclass(frozen=True, slots=True)
class CommandHeader:
    command_type: int
    command_id: int
    data_size: int

    def encode(self) -> bytes:
        if self.data_size < 0 or self.data_size > 0xFFFFFFFF:
            raise ProtocolError("Tamaño de datos fuera del rango de 32 bits")
        return HEADER.pack(MAGIC, self.command_type, self.command_id, self.data_size)

    @classmethod
    def decode(cls, payload: bytes) -> "CommandHeader":
        if len(payload) != HEADER.size:
            raise ProtocolError(f"Cabecera incompleta: {len(payload)} bytes")
        magic, command_type, command_id, data_size = HEADER.unpack(payload)
        if magic != MAGIC:
            raise ProtocolError(f"Firma desconocida: {magic!r}")
        return cls(command_type, command_id, data_size)


@dataclass(frozen=True, slots=True)
class FileRangeRequest:
    size: int
    offset: int
    name: str

    @classmethod
    def decode(cls, payload: bytes) -> "FileRangeRequest":
        if len(payload) < FILE_RANGE.size:
            raise ProtocolError("Solicitud de rango incompleta")
        size, offset, name_size = FILE_RANGE.unpack_from(payload)
        raw_name = payload[FILE_RANGE.size :]
        if name_size > len(raw_name):
            raise ProtocolError("El nombre declarado supera el tamaño de la trama")
        raw_name = raw_name[:name_size]
        try:
            name = raw_name.rstrip(b"\0").decode("utf-8")
        except UnicodeDecodeError as exc:
            raise ProtocolError("El nombre del archivo no es UTF-8 válido") from exc
        if not name:
            raise ProtocolError("La solicitud no contiene un nombre de archivo")
        return cls(size=size, offset=offset, name=name)

