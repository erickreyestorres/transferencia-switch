from pathlib import Path
import re
import unittest


PROJECT_ROOT = Path(__file__).parents[1]
SWITCH_APP = PROJECT_ROOT / "switch_app"


class MtpReadOnlyArchitectureTests(unittest.TestCase):
    def test_vendor_component_excludes_gpl_database(self) -> None:
        vendor = SWITCH_APP / "vendor" / "mtp"
        self.assertTrue((vendor / "LICENSE-APACHE-2.0.txt").is_file())
        self.assertTrue((vendor / "NOTICE.md").is_file())
        self.assertFalse((vendor / "include" / "SwitchMtpDatabase.h").exists())

    def test_storage_is_advertised_as_strictly_read_only(self) -> None:
        storage = (
            SWITCH_APP / "vendor" / "mtp" / "source" / "MtpStorage.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("MTP_STORAGE_READ_ONLY_WITHOUT_DELETE", storage)

    def test_mtp_device_does_not_advertise_mutating_operations(self) -> None:
        server = (
            SWITCH_APP / "vendor" / "mtp" / "source" / "MtpServer.cpp"
        ).read_text(encoding="utf-8")
        operations = re.search(
            r"kSupportedOperationCodes\[\]\s*=\s*\{(?P<body>.*?)\};",
            server,
            re.DOTALL,
        )
        self.assertIsNotNone(operations)
        advertised = operations.group("body")
        forbidden = (
            "MTP_OPERATION_SEND_OBJECT",
            "MTP_OPERATION_DELETE_OBJECT",
            "MTP_OPERATION_MOVE_OBJECT",
            "MTP_OPERATION_SET_OBJECT_PROP_VALUE",
            "MTP_OPERATION_SEND_PARTIAL_OBJECT",
        )
        for operation in forbidden:
            self.assertNotIn(operation, advertised)

    def test_main_exposes_standard_mtp_interface(self) -> None:
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        descriptor = (
            SWITCH_APP / "vendor" / "mtp" / "include" / "USBMtpInterface.h"
        ).read_text(encoding="utf-8")
        self.assertIn(".idProduct = 0x4000", main)
        self.assertIn('"1: SD Card"', main)
        self.assertIn(".bInterfaceClass = 6", descriptor)
        self.assertIn(".bInterfaceSubClass = 1", descriptor)
        self.assertIn(".bInterfaceProtocol = 1", descriptor)

    def test_core_mtp_operation_codes_match_the_specification(self) -> None:
        protocol = (
            SWITCH_APP / "vendor" / "mtp" / "include" / "mtp.h"
        ).read_text(encoding="utf-8")
        expected = {
            "MTP_OPERATION_OPEN_SESSION": "0x1002",
            "MTP_OPERATION_GET_STORAGE_IDS": "0x1004",
            "MTP_OPERATION_GET_OBJECT_INFO": "0x1008",
            "MTP_OPERATION_GET_OBJECT": "0x1009",
            "MTP_OPERATION_SEND_OBJECT_INFO": "0x100C",
            "MTP_OPERATION_SEND_OBJECT": "0x100D",
        }
        for name, value in expected.items():
            self.assertRegex(protocol, rf"#define\s+{name}\s+{value}\b")


if __name__ == "__main__":
    unittest.main()
