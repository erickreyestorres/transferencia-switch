"""Caso de uso para transferir un rango de archivo."""

from dbi_open.application.ports import ByteWriter, CancellationToken, Clock, EventPublisher, FileRepository
from dbi_open.domain.coverage import RangeCoverage
from dbi_open.domain.models import TransferCancelledError, TransferError, TransferProgress, TransferRequest, TransferResult


class TransferFileRange:
    def __init__(self, repository: FileRepository, writer: ByteWriter, events: EventPublisher,
                 cancellation: CancellationToken, clock: Clock,
                 coverage: dict[str, RangeCoverage], chunk_size: int) -> None:
        if chunk_size <= 0:
            raise ValueError("El tamaño de bloque debe ser positivo")
        self.repository = repository
        self.writer = writer
        self.events = events
        self.cancellation = cancellation
        self.clock = clock
        self.coverage = coverage
        self.chunk_size = chunk_size

    def execute(self, request: TransferRequest) -> TransferResult:
        descriptor = self.repository.describe(request.name)
        request.validate_for(descriptor)
        file_coverage = self.coverage[request.name]
        sent = 0
        started = self.clock.monotonic()
        self.events.publish("file_started", name=request.name, size=descriptor.size,
                            transferred=file_coverage.transferred)

        for chunk in self.repository.iter_chunks(request.name, request.offset, request.size, self.chunk_size):
            if self.cancellation.is_cancelled:
                raise TransferCancelledError("Transferencia cancelada por el usuario")
            written = self.writer.write(chunk)
            if written != len(chunk):
                raise TransferError(f"Escritura incompleta: {written}/{len(chunk)} bytes")
            start = request.offset + sent
            sent += written
            file_coverage.add(start, start + written)
            elapsed = max(self.clock.monotonic() - started, 0.001)
            progress = TransferProgress(request.name, descriptor.size, file_coverage.transferred,
                                        sent / elapsed, file_coverage.complete)
            self.events.publish("file_progress", name=progress.name, size=progress.file_size,
                                transferred=progress.transferred, speed=progress.speed,
                                complete=progress.complete)

        if sent != request.size:
            raise TransferError(f"Lectura incompleta de {request.name}: {sent}/{request.size} bytes")
        return TransferResult(request.name, request.size, sent, file_coverage.complete)
