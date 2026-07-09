import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
GRAPHICAL_MENU = ROOT / "switch_app" / "source" / "adapters" / "graphical_menu.cpp"


class MenuInputTests(unittest.TestCase):
    def test_menu_accepts_dpad_and_both_sticks(self):
        source = GRAPHICAL_MENU.read_text(encoding="utf-8")
        self.assertIn("HidNpadButton_AnyUp", source)
        self.assertIn("HidNpadButton_AnyDown", source)
        self.assertIn("HidNpadButton_AnyLeft", source)
        self.assertIn("HidNpadButton_AnyRight", source)


if __name__ == "__main__":
    unittest.main()
