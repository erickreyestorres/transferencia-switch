import struct
import sys
from pathlib import Path
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / "pc_backend"))

from dbi_open.protocol import CommandHeader, CommandId, CommandType
from dbi_open.server import DbiBackendServer


class FakeReadEndpoint:
    def __init__(self, payloads: list[bytes]) -> None:
        self.payloads = payloads

    def read(self, size: int, timeout: int = 0) -> bytes:
        payload = self.payloads.pop(0)
        if len(payload) != size:
            raise AssertionError(f"Lectura esperada de {size}, recibida de {len(payload)}")
        return payload


class FakeWriteEndpoint:
    def __init__(self) -> None:
        self.payloads: list[bytes] = []

    def write(self, payload: bytes, timeout: int = 0) -> int:
        self.payloads.append(bytes(payload))
        return len(payload)


class BackendServerTests(unittest.TestCase):
    def test_complete_list_range_exit_session(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "demo.nsp"
            contents = b"PFS0-demo"
            path.write_bytes(contents)
            events: list[dict] = []
            server = DbiBackendServer([path], events.append)

            name = path.name.encode("utf-8")
            range_request = struct.pack("<IQI", 1, 0, len(name)) + name
            server.read_endpoint = FakeReadEndpoint(
                [
                    CommandHeader(CommandType.REQUEST, CommandId.LIST, 0).encode(),
                    CommandHeader(CommandType.ACK, CommandId.LIST, 0).encode(),
                    CommandHeader(
                        CommandType.REQUEST,
                        CommandId.FILE_RANGE,
                        len(range_request),
                    ).encode(),
                    range_request,
                    CommandHeader(CommandType.ACK, CommandId.FILE_RANGE, 0).encode(),
                    CommandHeader(CommandType.REQUEST, CommandId.EXIT, 0).encode(),
                ]
            )
            writer = FakeWriteEndpoint()
            server.write_endpoint = writer
            server._connect = lambda: None  # type: ignore[method-assign]

            server.serve()

            self.assertEqual(writer.payloads[1], b"demo.nsp\n")
            self.assertEqual(writer.payloads[4], contents[:1])
            self.assertTrue(any(event["type"] == "session_finished" for event in events))

    def test_file_range_is_sent_and_reported(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "demo.nsp"
            contents = bytes(range(256)) * 32
            path.write_bytes(contents)
            events: list[dict] = []
            server = DbiBackendServer([path], events.append)

            name = path.name.encode("utf-8")
            request = struct.pack("<IQI", len(contents), 0, len(name)) + name
            acknowledgement = CommandHeader(CommandType.ACK, CommandId.FILE_RANGE, 0).encode()
            server.read_endpoint = FakeReadEndpoint([request, acknowledgement])
            writer = FakeWriteEndpoint()
            server.write_endpoint = writer

            server._handle_file_range(len(request))

            self.assertEqual(writer.payloads[-1], contents)
            self.assertTrue(server.coverage[path.name].complete)
            progress = [event for event in events if event["type"] == "file_progress"]
            self.assertTrue(progress[-1]["complete"])
            self.assertEqual(progress[-1]["transferred"], len(contents))

    def test_list_response_contains_every_file(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "base.nsp"
            second = Path(directory) / "update.nsp"
            first.write_bytes(b"base")
            second.write_bytes(b"update")
            server = DbiBackendServer([second, first], lambda event: None)
            acknowledgement = CommandHeader(CommandType.ACK, CommandId.LIST, 0).encode()
            server.read_endpoint = FakeReadEndpoint([acknowledgement])
            writer = FakeWriteEndpoint()
            server.write_endpoint = writer

            server._handle_list()

            self.assertEqual(writer.payloads[-1], b"base.nsp\nupdate.nsp\n")


if __name__ == "__main__":
    unittest.main()
