"""Modelos y errores del dominio de transferencias."""

from dataclasses import dataclass


class TransferError(RuntimeError):
    """Error esperado durante una transferencia."""


class FileNotAvailableError(TransferError):
    """El archivo solicitado no pertenece a la sesión."""


class InvalidTransferRangeError(TransferError):
    """El rango solicitado queda fuera del archivo."""


class TransferCancelledError(TransferError):
    """La persona usuaria canceló la transferencia."""


@dataclass(frozen=True, slots=True)
class FileDescriptor:
    name: str
    size: int

    def __post_init__(self) -> None:
        if not self.name:
            raise ValueError("El archivo debe tener un nombre")
        if self.size < 0:
            raise ValueError("El tamaño no puede ser negativo")


@dataclass(frozen=True, slots=True)
class TransferRequest:
    name: str
    offset: int
    size: int

    def validate_for(self, file: FileDescriptor) -> None:
        if self.name != file.name:
            raise FileNotAvailableError(f"Archivo no disponible: {self.name}")
        if self.offset < 0 or self.size < 0 or self.offset + self.size > file.size:
            raise InvalidTransferRangeError(
                f"Rango inválido para {file.name}: offset={self.offset}, tamaño={self.size}"
            )


@dataclass(frozen=True, slots=True)
class TransferProgress:
    name: str
    file_size: int
    transferred: int
    speed: float
    complete: bool


@dataclass(frozen=True, slots=True)
class TransferResult:
    name: str
    requested: int
    sent: int
    complete: bool
