import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SWITCH_APP = ROOT / "switch_app"


class MtpExitWithoutHostTests(unittest.TestCase):
    def test_usb_ready_wait_uses_caller_timeout(self):
        source = (SWITCH_APP / "vendor" / "mtp" / "source" / "usb.c").read_text(
            encoding="utf-8"
        )
        self.assertIn("usbDsWaitReady(timeout)", source)
        self.assertNotIn("usbDsWaitReady(UINT64_MAX)", source)
        self.assertIn("size_t transferredSize=0", source)

    def test_server_stop_flag_is_thread_safe(self):
        header = (SWITCH_APP / "vendor" / "mtp" / "include" / "MtpServer.h").read_text(
            encoding="utf-8"
        )
        self.assertIn("#include <atomic>", header)
        self.assertIn("std::atomic_bool", header)

    def test_empty_timeout_packet_is_ignored(self):
        source = (SWITCH_APP / "vendor" / "mtp" / "source" / "MtpServer.cpp").read_text(
            encoding="utf-8"
        )
        self.assertIn("if (ret <= 0)", source)


if __name__ == "__main__":
    unittest.main()
