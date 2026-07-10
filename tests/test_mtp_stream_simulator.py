from pathlib import Path
import tempfile
import unittest

from tools.mtp_stream_simulator import UNKNOWN_MTP_SIZE, format_report, simulate_mtp_send
from tools.package_fixtures import create_nsp, create_xci


class MtpStreamSimulatorTests(unittest.TestCase):
    def test_known_size_nsp_completes(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "demo.nsp"
            info = create_nsp(path, payload_size=2 * 1024 * 1024)

            result = simulate_mtp_send(path, advertised_size=info.inferred_size)

            self.assertTrue(result.succeeded)
            self.assertEqual(result.transferred, info.inferred_size)
            self.assertEqual(result.trailing_bytes, 0)

    def test_unknown_size_large_xci_completes_when_no_trailing_bytes_exist(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "large.xci"
            create_xci(path, secure_payload_size=5 * 1024 * 1024 * 1024)

            result = simulate_mtp_send(
                path,
                advertised_size=UNKNOWN_MTP_SIZE,
                chunk_size=512 * 1024,
            )

            self.assertTrue(result.succeeded)
            self.assertGreater(result.inferred_size, UNKNOWN_MTP_SIZE)
            self.assertEqual(result.transferred, result.inferred_size)

    def test_unknown_size_xci_with_trailing_padding_is_flagged(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "padded.xci"
            create_xci(
                path,
                secure_payload_size=2 * 1024 * 1024,
                trailing_padding=4096,
            )

            result = simulate_mtp_send(path, advertised_size=UNKNOWN_MTP_SIZE)

            self.assertFalse(result.succeeded)
            self.assertEqual(result.trailing_bytes, 4096)
            self.assertIn("requiere validacion hardware", result.detail)

    def test_cancelled_transfer_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "demo.nsp"
            create_nsp(path, payload_size=2 * 1024 * 1024)

            result = simulate_mtp_send(path, cancel_after=64 * 1024)

            self.assertFalse(result.succeeded)
            self.assertTrue(result.cancelled)
            self.assertEqual(result.transferred, 64 * 1024)

    def test_known_size_mismatch_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "demo.nsp"
            info = create_nsp(path, payload_size=2 * 1024 * 1024)

            result = simulate_mtp_send(path, advertised_size=info.inferred_size - 1)

            self.assertFalse(result.succeeded)
            self.assertIn("no coincide", result.detail)

    def test_formats_human_readable_report(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "padded.xci"
            create_xci(path, secure_payload_size=2 * 1024 * 1024, trailing_padding=4096)
            result = simulate_mtp_send(path, advertised_size=UNKNOWN_MTP_SIZE)

            report = format_report(result)

            self.assertIn("Reporte simulacion MTP", report)
            self.assertIn("Estado: FALLO", report)
            self.assertIn("Bytes extra al final: 4096", report)
            self.assertIn("Accion sugerida", report)


if __name__ == "__main__":
    unittest.main()
