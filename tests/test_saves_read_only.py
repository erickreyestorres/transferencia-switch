from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).parents[1]
SWITCH_APP = PROJECT_ROOT / "switch_app"


class SavesReadOnlyArchitectureTests(unittest.TestCase):
    def test_saves_are_enumerated_and_mounted_read_only(self) -> None:
        source = (
            SWITCH_APP / "source" / "adapters" / "save_mounts.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("fsOpenSaveDataInfoReader", source)
        self.assertIn("fsSaveDataInfoReaderRead", source)
        self.assertIn("FsSaveDataType_Account", source)
        self.assertIn("FsSaveDataRank_Primary", source)
        self.assertIn("fsdevMountSaveDataReadOnly", source)

    def test_saves_adapter_contains_no_mutating_operations(self) -> None:
        source = (
            SWITCH_APP / "source" / "adapters" / "save_mounts.cpp"
        ).read_text(encoding="utf-8")
        forbidden = (
            "fsdevMountSaveData(",
            "fsdevCommitDevice",
            "fsFileWrite",
            "fsFsCreateFile",
            "fsFsDeleteFile",
            "fsDeleteSaveDataFileSystem",
        )
        for operation in forbidden:
            self.assertNotIn(operation, source)

    def test_saves_use_a_bounded_mount_count(self) -> None:
        header = (
            SWITCH_APP / "include" / "transfer_switch" / "adapters"
            / "save_mounts.hpp"
        ).read_text(encoding="utf-8")
        source = (
            SWITCH_APP / "source" / "adapters" / "save_mounts.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("kMaximumMountedSaves = 20", header)
        self.assertIn("truncated_count_", source)

    def test_saves_are_aggregated_under_one_virtual_storage(self) -> None:
        database_header = (
            SWITCH_APP / "include" / "transfer_switch" / "adapters"
            / "read_only_sd_database.hpp"
        ).read_text(encoding="utf-8")
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("addVirtualStorage", database_header)
        self.assertIn("addVirtualRootDirectory", database_header)
        self.assertIn('"7: Saves"', main)
        self.assertIn("database.addVirtualStorage(kSavesStorageId)", main)
        self.assertIn("server.addStorage(&saves_storage)", main)

    def test_save_backup_plan_is_portable_domain(self) -> None:
        header = (
            SWITCH_APP / "include" / "transfer_switch" / "domain"
            / "save_backup_plan.h"
        ).read_text(encoding="utf-8")
        source = (
            SWITCH_APP / "source" / "domain" / "save_backup_plan.c"
        ).read_text(encoding="utf-8")

        self.assertIn("TS_SAVE_BACKUP_ROOT", header)
        self.assertIn("ts_save_backup_plan_create", header)
        self.assertIn("transferencia-switch.save-backup.v1", source)
        self.assertNotIn("<switch.h>", source)
        self.assertNotIn("fsdevMountSaveData", source)
        self.assertNotIn("fsFileWrite", source)


if __name__ == "__main__":
    unittest.main()
