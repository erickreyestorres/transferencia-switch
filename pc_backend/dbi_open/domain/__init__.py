"""Modelos y reglas sin dependencias de infraestructura."""

from .coverage import RangeCoverage
from .models import (
    FileDescriptor,
    FileNotAvailableError,
    InvalidTransferRangeError,
    TransferCancelledError,
    TransferError,
    TransferProgress,
    TransferRequest,
    TransferResult,
)

__all__ = [
    "FileDescriptor",
    "FileNotAvailableError",
    "InvalidTransferRangeError",
    "RangeCoverage",
    "TransferCancelledError",
    "TransferError",
    "TransferProgress",
    "TransferRequest",
    "TransferResult",
]
