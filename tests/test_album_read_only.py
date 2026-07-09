from pathlib import Path
import unittest


PROJECT_ROOT = Path(__file__).parents[1]
SWITCH_APP = PROJECT_ROOT / "switch_app"


class AlbumReadOnlyArchitectureTests(unittest.TestCase):
    def test_album_mounts_use_image_directory_filesystems(self) -> None:
        source = (
            SWITCH_APP / "source" / "adapters" / "album_mounts.cpp"
        ).read_text(encoding="utf-8")
        self.assertIn("fsOpenImageDirectoryFileSystem", source)
        self.assertIn("FsImageDirectoryId_Sd", source)
        self.assertIn("FsImageDirectoryId_Nand", source)
        self.assertIn("fsdevUnmountDevice", source)

    def test_album_does_not_use_mutating_album_operations(self) -> None:
        source = (
            SWITCH_APP / "source" / "adapters" / "album_mounts.cpp"
        ).read_text(encoding="utf-8")
        forbidden = (
            "capsaDeleteAlbumFile",
            "capsaStorageCopyAlbumFile",
            "fsFsCreateFile",
            "fsFsDeleteFile",
        )
        for operation in forbidden:
            self.assertNotIn(operation, source)

    def test_album_storages_are_registered_only_after_successful_mount(self) -> None:
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn('"8: Album (SD)"', main)
        self.assertIn('"8: Album (NAND)"', main)
        self.assertIn("if (album_mounts.sdMounted())", main)
        self.assertIn("if (album_mounts.nandMounted())", main)
        self.assertIn("server.addStorage(&album_sd_storage)", main)
        self.assertIn("server.addStorage(&album_nand_storage)", main)

    def test_album_mount_failures_are_visible_but_non_fatal(self) -> None:
        main = (SWITCH_APP / "source" / "main.cpp").read_text(encoding="utf-8")
        self.assertIn('"no disponible"', main)
        self.assertIn("album_mounts.sdResult()", main)
        self.assertIn("album_mounts.nandResult()", main)


if __name__ == "__main__":
    unittest.main()
