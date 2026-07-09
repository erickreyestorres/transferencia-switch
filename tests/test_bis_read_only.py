from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).parents[1]
SWITCH_APP = PROJECT_ROOT / "switch_app"


class BisReadOnlyArchitectureTests(unittest.TestCase):
    def test_user_and_system_use_official_bis_filesystems(self) -> None:
        source = (
            SWITCH_APP / "source" / "adapters" / "bis_mounts.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("fsOpenBisFileSystem", source)
        self.assertIn("FsBisPartitionId_User", source)
        self.assertIn("FsBisPartitionId_System", source)
        self.assertIn("fsdevUnmountDevice", source)

    def test_bis_adapter_contains_no_write_or_delete_calls(self) -> None:
        source = (
            SWITCH_APP / "source" / "adapters" / "bis_mounts.cpp"
        ).read_text(encoding="utf-8")
        forbidden = (
            "fsFsCreateFile",
            "fsFsDeleteFile",
            "fsFsDeleteDirectory",
            "fsFsWriteFile",
            "fsFileWrite",
            "fsFsCommit",
        )
        for operation in forbidden:
            self.assertNotIn(operation, source)

    def test_bis_storages_are_optional_and_read_only(self) -> None:
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn('"2: Nand USER"', main)
        self.assertIn('"3: Nand SYSTEM"', main)
        self.assertIn("if (bis_mounts.userMounted())", main)
        self.assertIn("if (bis_mounts.systemMounted())", main)
        self.assertIn("server.addStorage(&nand_user_storage)", main)
        self.assertIn("server.addStorage(&nand_system_storage)", main)

        user_storage = main[main.index("android::MtpStorage nand_user_storage("):]
        user_storage = user_storage[: user_storage.index(");")]
        system_storage = main[main.index("android::MtpStorage nand_system_storage("):]
        system_storage = system_storage[: system_storage.index(");")]
        self.assertTrue(user_storage.rstrip().endswith("false"))
        self.assertTrue(system_storage.rstrip().endswith("false"))

    def test_safe_inbox_uses_slot_ten(self) -> None:
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("kInboxStorageId = 0x000A0001", main)
        self.assertIn('"10: Safe Inbox"', main)


if __name__ == "__main__":
    unittest.main()
