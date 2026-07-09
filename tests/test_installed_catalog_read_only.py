from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).parents[1]
SWITCH_APP = PROJECT_ROOT / "switch_app"


class InstalledCatalogReadOnlyTests(unittest.TestCase):
    def test_catalog_uses_read_only_ns_queries(self) -> None:
        source = (
            SWITCH_APP / "source" / "adapters" / "installed_catalog.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("nsListApplicationRecord", source)
        self.assertIn("nsGetApplicationControlData", source)
        self.assertIn("nacpGetLanguageEntry", source)
        self.assertIn("nsListApplicationContentMetaStatus", source)

    def test_catalog_does_not_modify_installed_content(self) -> None:
        source = (
            SWITCH_APP / "source" / "adapters" / "installed_catalog.cpp"
        ).read_text(encoding="utf-8")
        forbidden = (
            "nsDeleteApplication",
            "nsDeleteApplicationEntity",
            "ncmCreateContentStorage",
            "ncmOpenContentStorage",
            "ncmContentStorageWriteContent",
            "ncmContentStorageDelete",
            "nsRequestDownloadApplication",
        )
        for operation in forbidden:
            self.assertNotIn(operation, source)

    def test_generated_files_are_limited_to_private_cache(self) -> None:
        header = (
            SWITCH_APP / "include" / "transfer_switch" / "adapters"
            / "installed_catalog.hpp"
        ).read_text(encoding="utf-8")
        source = (
            SWITCH_APP / "source" / "adapters" / "installed_catalog.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn('"sdmc:/switch/transferencia-switch/cache/installed/"', header)
        self.assertIn('path + ".partial"', source)
        self.assertIn("fsync(descriptor)", source)
        self.assertIn('"/info.txt"', source)
        self.assertIn('"/icon.jpg"', source)

    def test_installed_games_storage_is_optional_and_read_only(self) -> None:
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn('"4: Installed games"', main)
        self.assertIn("installed_catalog_available = installed_catalog.generate()", main)
        self.assertIn("if (installed_catalog_available)", main)
        self.assertIn("server.addStorage(&installed_storage)", main)

        storage = main[main.index("android::MtpStorage installed_storage("):]
        storage = storage[: storage.index(");")]
        self.assertTrue(storage.rstrip().endswith("false"))


if __name__ == "__main__":
    unittest.main()
