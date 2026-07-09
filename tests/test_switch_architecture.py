from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).parents[1]
SWITCH_SOURCE = PROJECT_ROOT / "switch_app" / "source"


class SwitchArchitectureTests(unittest.TestCase):
    def test_domain_and_application_do_not_depend_on_libnx(self) -> None:
        core_files = list((SWITCH_SOURCE / "domain").glob("*.c"))
        core_files += list((SWITCH_SOURCE / "application").glob("*.c"))

        self.assertTrue(core_files)
        for path in core_files:
            source = path.read_text(encoding="utf-8")
            self.assertNotIn("<switch.h>", source, path)
            self.assertNotIn("usbComms", source, path)

    def test_only_libnx_adapter_uses_usb_comms(self) -> None:
        users = []
        for path in SWITCH_SOURCE.rglob("*.c"):
            if "usbComms" in path.read_text(encoding="utf-8"):
                users.append(path.relative_to(SWITCH_SOURCE).as_posix())

        self.assertEqual(users, ["adapters/libnx_usb_transport.c"])


if __name__ == "__main__":
    unittest.main()
