import struct
import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / "pc_backend"))

from dbi_open.protocol import (
    CommandHeader,
    CommandId,
    CommandType,
    FileRangeRequest,
    ProtocolError,
)


class ProtocolTests(unittest.TestCase):
    def test_command_header_round_trip(self) -> None:
        original = CommandHeader(CommandType.REQUEST, CommandId.FILE_RANGE, 123)
        self.assertEqual(CommandHeader.decode(original.encode()), original)

    def test_invalid_magic_is_rejected(self) -> None:
        with self.assertRaises(ProtocolError):
            CommandHeader.decode(struct.pack("<4sIII", b"NOPE", 0, 0, 0))

    def test_file_range_request(self) -> None:
        name = "ejemplo.nsp".encode("utf-8")
        payload = struct.pack("<IQI", 4096, 8192, len(name)) + name
        request = FileRangeRequest.decode(payload)
        self.assertEqual(request.size, 4096)
        self.assertEqual(request.offset, 8192)
        self.assertEqual(request.name, "ejemplo.nsp")


if __name__ == "__main__":
    unittest.main()
