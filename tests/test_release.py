"""Release version-stamping contract, independent of GitHub credentials."""

from pathlib import Path
import runpy
import tempfile
import unittest

stamp_version = runpy.run_path(
    str(Path(__file__).resolve().parents[1] / ".github/scripts/stamp-version.py")
)["stamp_version"]

DEVELOPMENT = (
    '#define HWIRE_VERSION "development"\n'
    '#define HWIRE_VERSION_IS_DEVELOPMENT 1\n'
)


class ReleaseVersionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.header = self.root / "src/hwire.h"
        self.header.parent.mkdir()
        self.header.write_text(DEVELOPMENT, encoding="utf-8")

    def test_valid_versions(self):
        for version in ("2026.01.0", "2026.10.1", "2026.12.123"):
            with self.subTest(version=version):
                self.header.write_text(DEVELOPMENT, encoding="utf-8")
                stamp_version(self.root, version)
                self.assertEqual(
                    self.header.read_text(encoding="utf-8"),
                    f'#define HWIRE_VERSION "{version}"\n'
                    '#define HWIRE_VERSION_IS_DEVELOPMENT 0\n',
                )

    def test_invalid_versions_do_not_modify_header(self):
        for version in ("dev", "v2026.10.0", "2026.1.0", "2026.00.0",
                        "2026.13.0", "2026.10.01", "2026.10.-1",
                        "2026.10.0\n", '2026.10.0"', "$(id)"):
            with self.subTest(version=version):
                with self.assertRaises(ValueError):
                    stamp_version(self.root, version)
                self.assertEqual(self.header.read_text(encoding="utf-8"), DEVELOPMENT)

    def test_missing_duplicate_or_stamped_definitions_are_rejected(self):
        for text in ("", DEVELOPMENT + DEVELOPMENT,
                     DEVELOPMENT.replace("DEVELOPMENT 1", "DEVELOPMENT 0"),
                     DEVELOPMENT.replace('"development"', '"2026.10.0"')):
            with self.subTest(text=text):
                self.header.write_text(text, encoding="utf-8")
                with self.assertRaises(ValueError):
                    stamp_version(self.root, "2026.10.0")
                self.assertEqual(self.header.read_text(encoding="utf-8"), text)


if __name__ == "__main__":
    unittest.main()
