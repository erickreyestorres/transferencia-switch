import sys
from pathlib import Path
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / "pc_backend"))

from dbi_open.coverage import RangeCoverage


class RangeCoverageTests(unittest.TestCase):
    def test_contiguous_ranges_are_merged(self) -> None:
        coverage = RangeCoverage(100)
        coverage.add(0, 40)
        coverage.add(40, 75)
        self.assertEqual(coverage.transferred, 75)

    def test_overlapping_ranges_are_not_counted_twice(self) -> None:
        coverage = RangeCoverage(100)
        coverage.add(0, 60)
        coverage.add(20, 80)
        self.assertEqual(coverage.transferred, 80)

    def test_ranges_are_clamped_to_file_size(self) -> None:
        coverage = RangeCoverage(100)
        coverage.add(-10, 150)
        self.assertTrue(coverage.complete)
        self.assertEqual(coverage.transferred, 100)


if __name__ == "__main__":
    unittest.main()

