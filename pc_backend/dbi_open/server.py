"""Servidor USB compatible con el flujo básico de DBIbackend."""

from __future__ import annotations

from collections.abc import Callable, Iterable
from pathlib import Path
import threading
import time
from typing import Any

from .adapters.local_files import LocalFileRepository
from .adapters.runtime import CallbackEventPublisher, EndpointWriter, EventCancellationToken, SystemClock
from .application.transfer_file_range import TransferFileRange
from .domain.coverage import RangeCoverage
from .domain.models import TransferError, TransferRequest
from .protocol import CommandHeader, CommandId, CommandType, FileRangeRequest, ProtocolError

try:
    import usb.core
    import usb.util
    try:
        import libusb_package

        USB_BACKEND = libusb_package.get_libusb1_backend()
    except ImportError:
        USB_BACKEND = None
except ImportError:  # Permite ejecutar las pruebas sin PyUSB.
    usb = None  # type: ignore[assignment]
    USB_BACKEND = None


NINTENDO_VENDOR_ID = 0x057E
DBI_PRODUCT_ID = 0x3000
TRANSFER_CHUNK_SIZE = 0x100000


class BackendError(TransferError):
    """Error legible del servidor USB."""


EventCallback = Callable[[dict[str, Any]], None]


class DbiBackendServer:
    def __init__(
        self,
        files: Iterable[Path],
        callback: EventCallback,
        stop_event: threading.Event | None = None,
    ) -> None:
        try:
            self.repository = LocalFileRepository(files)
        except TransferError as exc:
            raise BackendError(str(exc)) from exc
        self.files = self.repository.paths
        self.coverage = {name: RangeCoverage(path.stat().st_size) for name, path in self.files.items()}
        self.callback = callback
        self.events = CallbackEventPublisher(callback)
        self.stop_event = stop_event or threading.Event()
        self.device: Any = None
        self.read_endpoint: Any = None
        self.write_endpoint: Any = None

    def _emit(self, event_type: str, **payload: Any) -> None:
        self.events.publish(event_type, **payload)

    def _connect(self) -> None:
        if usb is None:
            raise BackendError("PyUSB no está instalado")

        self._emit("status", message="Esperando la consola en modo DBIbackend…")
        while not self.stop_event.is_set():
            device = usb.core.find(
                idVendor=NINTENDO_VENDOR_ID,
                idProduct=DBI_PRODUCT_ID,
                backend=USB_BACKEND,
            )
            if device is None:
                self.stop_event.wait(0.5)
                continue

            try:
                device.reset()
                time.sleep(1)
                device.set_configuration()
                configuration = device.get_active_configuration()
                interface = configuration[(0, 0)]
                self.write_endpoint = usb.util.find_descriptor(
                    interface,
                    custom_match=lambda endpoint: usb.util.endpoint_direction(endpoint.bEndpointAddress)
                    == usb.util.ENDPOINT_OUT,
                )
                self.read_endpoint = usb.util.find_descriptor(
                    interface,
                    custom_match=lambda endpoint: usb.util.endpoint_direction(endpoint.bEndpointAddress)
                    == usb.util.ENDPOINT_IN,
                )
                if self.read_endpoint is None or self.write_endpoint is None:
                    raise BackendError("No se encontraron los endpoints USB de DBI")
                self.device = device
                self._emit("status", message="Consola conectada. Esperando selección en DBI…")
                return
            except usb.core.USBError as exc:
                raise BackendError(
                    "No se pudo abrir la consola. Verifica el cable y el controlador libusbK/WinUSB."
                ) from exc
        raise BackendError("Conexión cancelada")

    def _read(self, size: int, timeout: int = 0) -> bytes:
        return bytes(self.read_endpoint.read(size, timeout=timeout))

    def _write(self, payload: bytes, timeout: int = 0) -> int:
        return int(self.write_endpoint.write(payload, timeout=timeout))

    def _read_header(self) -> CommandHeader:
        return CommandHeader.decode(self._read(16, timeout=0))

    def _send_header(self, command_type: CommandType, command_id: CommandId, size: int) -> None:
        self._write(CommandHeader(command_type, command_id, size).encode())

    def _handle_list(self) -> None:
        payload = "".join(f"{name}\n" for name in sorted(self.files)).encode("utf-8")
        self._send_header(CommandType.RESPONSE, CommandId.LIST, len(payload))
        if payload:
            acknowledgement = self._read_header()
            if acknowledgement.command_type != CommandType.ACK:
                raise ProtocolError("DBI no confirmó la recepción de la lista")
            self._write(payload)
        self._emit("status", message=f"Lista enviada: {len(self.files)} archivo(s)")

    def _handle_file_range(self, data_size: int) -> None:
        self._send_header(CommandType.ACK, CommandId.FILE_RANGE, data_size)
        request = FileRangeRequest.decode(self._read(data_size))
        transfer_request = TransferRequest(request.name, request.offset, request.size)
        try:
            descriptor = self.repository.describe(request.name)
            transfer_request.validate_for(descriptor)
        except TransferError as exc:
            raise BackendError(str(exc)) from exc

        self._send_header(CommandType.RESPONSE, CommandId.FILE_RANGE, request.size)
        acknowledgement = self._read_header()
        if acknowledgement.command_type != CommandType.ACK:
            raise ProtocolError("DBI no confirmó el inicio de la transferencia")

        use_case = TransferFileRange(
            repository=self.repository,
            writer=EndpointWriter(self.write_endpoint),
            events=self.events,
            cancellation=EventCancellationToken(self.stop_event),
            clock=SystemClock(),
            coverage=self.coverage,
            chunk_size=TRANSFER_CHUNK_SIZE,
        )
        use_case.execute(transfer_request)

    def _emit_summary(self) -> None:
        files = []
        for name, path in sorted(self.files.items()):
            coverage = self.coverage[name]
            if coverage.complete:
                status = "enviado"
            elif coverage.transferred:
                status = "parcial"
            else:
                status = "no_solicitado"
            files.append(
                {
                    "name": name,
                    "size": path.stat().st_size,
                    "transferred": coverage.transferred,
                    "status": status,
                }
            )
        self._emit("session_finished", files=files)

    def serve(self) -> None:
        try:
            self._connect()
            while not self.stop_event.is_set():
                header = self._read_header()
                if header.command_id == CommandId.EXIT:
                    self._send_header(CommandType.RESPONSE, CommandId.EXIT, 0)
                    self._emit_summary()
                    return
                if header.command_id == CommandId.LIST:
                    self._handle_list()
                elif header.command_id == CommandId.FILE_RANGE:
                    self._handle_file_range(header.data_size)
                else:
                    self._emit("status", message=f"DBI envió un comando no compatible: {header.command_id}")
        except Exception as exc:
            self._emit("error", message=str(exc), exception=type(exc).__name__)
            raise
        finally:
            if usb is not None and self.device is not None:
                try:
                    usb.util.dispose_resources(self.device)
                except Exception:
                    pass
