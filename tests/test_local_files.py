import sys
from pathlib import Path
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / "pc_backend"))

from dbi_open.adapters.local_files import LocalFileRepository
from dbi_open.domain.models import FileNotAvailableError, TransferError


class LocalFileRepositoryTests(unittest.TestCase):
    def test_reads_only_the_requested_range_in_chunks(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "demo.bin"
            path.write_bytes(b"0123456789")
            repository = LocalFileRepository([path])

            chunks = list(repository.iter_chunks("demo.bin", 2, 6, 4))

            self.assertEqual(chunks, [b"2345", b"67"])

    def test_rejects_duplicate_names(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            first = Path(directory) / "a" / "demo.bin"
            second = Path(directory) / "b" / "demo.bin"
            first.parent.mkdir()
            second.parent.mkdir()
            first.write_bytes(b"a")
            second.write_bytes(b"b")

            with self.assertRaises(TransferError):
                LocalFileRepository([first, second])

    def test_unknown_file_has_a_domain_error(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "demo.bin"
            path.write_bytes(b"demo")
            repository = LocalFileRepository([path])

            with self.assertRaises(FileNotAvailableError):
                repository.describe("missing.bin")


if __name__ == "__main__":
    unittest.main()
