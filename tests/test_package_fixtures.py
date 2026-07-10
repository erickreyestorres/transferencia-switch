from pathlib import Path
import tempfile
import unittest

from tools.package_fixtures import create_nsp, create_xci, inspect_package, simulate_stream


class PackageFixtureTests(unittest.TestCase):
    def test_generates_and_inspects_sparse_nsp(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "demo.nsp"
            created = create_nsp(path, payload_size=5 * 1024 * 1024)
            inspected = inspect_package(path)

            self.assertEqual(created.kind, "NSP")
            self.assertEqual(inspected.kind, "NSP")
            self.assertEqual(inspected.inferred_size, path.stat().st_size)
            self.assertIn(".cnmt.nca", inspected.entries[0])

    def test_generates_and_inspects_sparse_large_xci_without_allocating_gigabytes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "large.xci"
            created = create_xci(path, secure_payload_size=5 * 1024 * 1024 * 1024)
            inspected = inspect_package(path)

            self.assertEqual(created.kind, "XCI")
            self.assertEqual(inspected.kind, "XCI")
            self.assertEqual(inspected.inferred_size, path.stat().st_size)
            self.assertGreater(inspected.inferred_size, 0xFFFFFFFF)
            self.assertIn(".cnmt.nca", inspected.entries[0])

    def test_detects_xci_trailing_padding_as_host_size_mismatch_risk(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "padded.xci"
            created = create_xci(
                path,
                secure_payload_size=2 * 1024 * 1024,
                trailing_padding=1024 * 1024,
            )
            inspected = inspect_package(path)

            self.assertEqual(inspected.kind, "XCI")
            self.assertEqual(inspected.inferred_size, created.inferred_size)
            self.assertLess(inspected.inferred_size, path.stat().st_size)

    def test_simulates_unknown_size_stream_until_inferred_package_end(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "large.xci"
            create_xci(path, secure_payload_size=5 * 1024 * 1024 * 1024)

            result = simulate_stream(path, chunk_size=256 * 1024)

            self.assertEqual(result["kind"], "XCI")
            self.assertTrue(result["complete"])
            self.assertEqual(result["trailing_bytes"], 0)
            self.assertEqual(result["transferred_until_complete"], result["inferred_size"])

    def test_simulation_reports_trailing_bytes_that_need_hardware_validation(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "padded.xci"
            create_xci(
                path,
                secure_payload_size=2 * 1024 * 1024,
                trailing_padding=4096,
            )

            result = simulate_stream(path, chunk_size=64 * 1024)

            self.assertTrue(result["complete"])
            self.assertEqual(result["trailing_bytes"], 4096)


if __name__ == "__main__":
    unittest.main()
