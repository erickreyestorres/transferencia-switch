from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
SWITCH_APP = ROOT / "switch_app"


class SdInstallReceiverTests(unittest.TestCase):
    def setUp(self) -> None:
        self.main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.receiver = (
            SWITCH_APP / "source" / "adapters" / "sd_install_receiver.cpp"
        ).read_text(encoding="utf-8")
        self.server = (
            SWITCH_APP / "vendor" / "mtp" / "source" / "MtpServer.cpp"
        ).read_text(encoding="utf-8")

    def test_sd_install_is_a_real_action_storage(self) -> None:
        self.assertIn('"5: SD Card install"', self.main)
        self.assertIn("addActionStorage(kSdInstallStorageId", self.main)
        self.assertIn("SdInstallReceiverFactory", self.main)
        self.assertIn("android::MtpStorage sd_install_storage", self.main)
        self.assertIn('"5: SD Card install",\n        kInboxReserveBytes,\n        true,\n        0,', self.main)

    def test_mtp_streams_to_an_injected_sink(self) -> None:
        self.assertIn("IncomingObjectSinkFactory", self.server)
        self.assertIn("receive_sink(", self.server)
        self.assertIn("mSendObjectSink->finish()", self.server)
        self.assertIn("mSendObjectSink->abort()", self.server)

    def test_rejected_sink_drains_mtp_and_preserves_specific_error(self) -> None:
        self.assertIn("Keep draining the MTP data container", self.server)
        self.assertIn("bool* sinkRejected", self.server)
        self.assertIn("detail = mSendObjectSink->detail()", self.server)

    def test_mtp_acknowledges_completion_before_service_teardown(self) -> None:
        response_write = self.server.index("ret = mResponse.write(usb)")
        sink_reset = self.server.index("mSendObjectSink.reset()", response_write)
        self.assertLess(response_write, sink_reset)
        self.assertIn("mSendObjectSinkCompleted", self.server)

    def test_completed_action_handle_survives_until_session_end(self) -> None:
        database = (
            SWITCH_APP / "source" / "adapters" / "read_only_sd_database.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("Keep the completed virtual object alive until SessionEnded", database)
        self.assertIn("entry->second.scanned = true", database)
        self.assertIn("void ReadOnlySdDatabase::sessionEnded()", database)

    def test_nsp_goes_directly_to_ncm_placeholders(self) -> None:
        self.assertIn("ncmContentStorageCreatePlaceHolder", self.receiver)
        self.assertIn("ncmContentStorageWritePlaceHolder", self.receiver)
        self.assertIn("ncmContentStorageRegister", self.receiver)
        self.assertIn("appendInstallLog", self.receiver)
        self.assertIn("kInstallLogPath", self.receiver)
        self.assertNotIn("staging", self.receiver.lower())
        self.assertNotIn("std::fopen(path_", self.receiver)

    def test_install_log_rotates_before_growing_without_limit(self) -> None:
        self.assertIn("kMaxInstallLogSize = 512 * 1024", self.receiver)
        self.assertIn("install.previous.log", self.receiver)
        self.assertIn("rotateInstallLogIfNeeded", self.receiver)
        self.assertIn("std::rename(kInstallLogPath, kPreviousInstallLogPath)", self.receiver)

    def test_install_commits_metadata_and_application_record(self) -> None:
        self.assertIn("ncmContentMetaDatabaseSet", self.receiver)
        self.assertIn("ncmContentMetaDatabaseCommit", self.receiver)
        self.assertIn("application_manager_", self.receiver)
        self.assertIn("baseApplicationId", self.receiver)

    def test_receiver_accepts_uncompressed_nsp_and_xci(self) -> None:
        self.assertIn('".nsp"', self.receiver)
        self.assertIn('".xci"', self.receiver)
        self.assertIn("solo NSP o XCI sin compresion", self.receiver)
        self.assertIn("for (const NcmContentId& cnmt_id : cnmt_ids_)", self.receiver)

    def test_large_unknown_mtp_size_is_reported_explicitly(self) -> None:
        self.assertIn("expected_size_ == 0xFFFFFFFFull", self.receiver)
        self.assertIn("tamano MTP desconocido aun no soportado", self.receiver)
        self.assertNotIn("expected_size_ < kMinPackageSize || expected_size_ >=", self.receiver)

    def test_collections_support_multiple_cnmt_records(self) -> None:
        self.assertIn("cnmt_ids_.empty() || cnmt_ids_.size() > 32", self.receiver)
        self.assertIn("std::vector<NcmContentMetaKey> metadata_keys_", self.receiver)
        self.assertIn("metadata_keys_.rbegin()", self.receiver)

    def test_xci_validates_both_hfs0_levels(self) -> None:
        self.assertIn("kXciRootOffset = 0xF000", self.receiver)
        self.assertIn("findSecurePartition", self.receiver)
        self.assertIn("parseXciSecureHeader", self.receiver)
        self.assertIn("particion secure no encontrada", self.receiver)

    def test_xci_converts_gamecard_nca_headers_before_registration(self) -> None:
        self.assertIn("splCryptoGenerateAesKek", self.receiver)
        self.assertIn("convertGamecardNcaHeader", self.receiver)
        self.assertIn("plain[0x204] = 0", self.receiver)
        self.assertIn("aes128XtsEncrypt", self.receiver)
        self.assertIn("package_type_ == PackageType::xci && !entry.cnmt", self.receiver)

    def test_failure_rolls_back_new_content(self) -> None:
        self.assertIn("void rollback()", self.receiver)
        self.assertIn("ncmContentMetaDatabaseRemove", self.receiver)
        self.assertIn("ncmContentStorageDelete", self.receiver)


if __name__ == "__main__":
    unittest.main()
