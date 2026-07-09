import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / "pc_backend"))

from dbi_open.application.transfer_file_range import TransferFileRange
from dbi_open.domain.coverage import RangeCoverage
from dbi_open.domain.models import (
    FileDescriptor,
    InvalidTransferRangeError,
    TransferCancelledError,
    TransferError,
    TransferRequest,
)


class MemoryRepository:
    def __init__(self, name: str, contents: bytes) -> None:
        self.name = name
        self.contents = contents

    def describe(self, name: str) -> FileDescriptor:
        if name != self.name:
            raise TransferError("Archivo no disponible")
        return FileDescriptor(name, len(self.contents))

    def iter_chunks(self, name: str, offset: int, size: int, chunk_size: int):
        data = self.contents[offset : offset + size]
        for position in range(0, len(data), chunk_size):
            yield data[position : position + chunk_size]


class RecordingWriter:
    def __init__(self, maximum_write: int | None = None) -> None:
        self.payloads: list[bytes] = []
        self.maximum_write = maximum_write

    def write(self, payload: bytes) -> int:
        self.payloads.append(payload)
        return len(payload) if self.maximum_write is None else min(len(payload), self.maximum_write)


class RecordingEvents:
    def __init__(self) -> None:
        self.items: list[dict] = []

    def publish(self, event_type: str, **payload) -> None:
        self.items.append({"type": event_type, **payload})


class Cancellation:
    def __init__(self, cancelled: bool = False) -> None:
        self.is_cancelled = cancelled


class FakeClock:
    def __init__(self) -> None:
        self.value = 0.0

    def monotonic(self) -> float:
        self.value += 0.5
        return self.value


class TransferFileRangeTests(unittest.TestCase):
    def build_use_case(
        self,
        contents: bytes,
        *,
        writer: RecordingWriter | None = None,
        cancellation: Cancellation | None = None,
        chunk_size: int = 4,
    ):
        repository = MemoryRepository("demo.bin", contents)
        writer = writer or RecordingWriter()
        events = RecordingEvents()
        coverage = {"demo.bin": RangeCoverage(len(contents))}
        use_case = TransferFileRange(
            repository,
            writer,
            events,
            cancellation or Cancellation(),
            FakeClock(),
            coverage,
            chunk_size,
        )
        return use_case, writer, events, coverage

    def test_transfers_in_chunks_and_reports_completion(self) -> None:
        use_case, writer, events, coverage = self.build_use_case(b"abcdefghij")

        result = use_case.execute(TransferRequest("demo.bin", 0, 10))

        self.assertEqual(writer.payloads, [b"abcd", b"efgh", b"ij"])
        self.assertEqual(result.sent, 10)
        self.assertTrue(result.complete)
        self.assertTrue(coverage["demo.bin"].complete)
        self.assertEqual(events.items[0]["type"], "file_started")
        self.assertEqual(events.items[-1]["type"], "file_progress")
        self.assertTrue(events.items[-1]["complete"])

    def test_rejects_a_range_outside_the_file(self) -> None:
        use_case, _, _, _ = self.build_use_case(b"1234")

        with self.assertRaises(InvalidTransferRangeError):
            use_case.execute(TransferRequest("demo.bin", 3, 2))

    def test_honors_cancellation_before_writing(self) -> None:
        use_case, writer, _, _ = self.build_use_case(
            b"1234", cancellation=Cancellation(cancelled=True)
        )

        with self.assertRaises(TransferCancelledError):
            use_case.execute(TransferRequest("demo.bin", 0, 4))
        self.assertEqual(writer.payloads, [])

    def test_rejects_partial_writes(self) -> None:
        use_case, _, _, _ = self.build_use_case(
            b"1234", writer=RecordingWriter(maximum_write=2)
        )

        with self.assertRaises(TransferError):
            use_case.execute(TransferRequest("demo.bin", 0, 4))


if __name__ == "__main__":
    unittest.main()
