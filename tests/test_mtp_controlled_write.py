from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).parents[1]
SWITCH_APP = PROJECT_ROOT / "switch_app"


class MtpControlledWriteArchitectureTests(unittest.TestCase):
    def test_inbox_is_separate_and_has_a_free_space_reserve(self) -> None:
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn('"sdmc:/switch/transferencia-switch/inbox/"', main)
        self.assertIn("kInboxReserveBytes = 512ull * 1024ull * 1024ull", main)
        self.assertIn('"10: Safe Inbox"', main)

    def test_write_operations_are_opt_in(self) -> None:
        server_header = (
            SWITCH_APP / "vendor" / "mtp" / "include" / "MtpServer.h"
        ).read_text(encoding="utf-8")
        server_source = (
            SWITCH_APP / "vendor" / "mtp" / "source" / "MtpServer.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("bool writeEnabled = false", server_header)
        self.assertIn("if (mWriteEnabled)", server_source)
        self.assertIn("MTP_OPERATION_SEND_OBJECT_INFO", server_source)
        self.assertIn("MTP_OPERATION_SEND_OBJECT", server_source)
        self.assertIn("MTP_RESPONSE_STORE_READ_ONLY", server_source)

    def test_received_files_are_staged_and_validated_before_publish(self) -> None:
        server = (
            SWITCH_APP / "vendor" / "mtp" / "source" / "MtpServer.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn('".transferencia-switch-staging"', server)
        self.assertIn("O_CREAT | O_EXCL", server)
        self.assertIn("fstat(fd, &information)", server)
        self.assertIn("fsync(fd)", server)
        self.assertIn("rename(mSendObjectTempPath.c_str(), mSendObjectFilePath.c_str())", server)
        self.assertNotIn("open(mSendObjectFilePath.c_str(), O_RDWR | O_CREAT", server)

    def test_database_rejects_writes_unless_explicitly_enabled(self) -> None:
        database = (
            SWITCH_APP / "source" / "adapters" / "read_only_sd_database.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("storage_root == storage_roots_.end()", database)
        self.assertIn("!storage_root->second.writable", database)
        self.assertIn("ts_is_safe_direct_child", database)
        self.assertIn("pending_handles_", database)

    def test_database_supports_multiple_storages_with_independent_policy(self) -> None:
        header = (
            SWITCH_APP / "include" / "transfer_switch" / "adapters"
            / "read_only_sd_database.hpp"
        ).read_text(encoding="utf-8")
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn("addStoragePathWithPolicy", header)
        self.assertIn("std::map<android::MtpStorageID, StorageRoot> storage_roots_", header)
        self.assertIn('"MTP principal"', main)
        self.assertIn("run_mtp(false, true)", main)
        self.assertIn("server.addStorage(&sd_storage)", main)
        self.assertIn("server.addStorage(&inbox_storage)", main)

    def test_console_presents_individual_and_session_results(self) -> None:
        presenter = (
            SWITCH_APP / "source" / "adapters" / "console_transfer_presenter.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn('"OK"', presenter)
        self.assertIn('"CANCELADO"', presenter)
        self.assertIn('"ERROR"', presenter)
        self.assertIn("correctos, %u fallidos, %u cancelados", presenter)


if __name__ == "__main__":
    unittest.main()
