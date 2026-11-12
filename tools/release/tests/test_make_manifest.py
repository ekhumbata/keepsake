"""The release manifest matches what the device accepts (core/firmware_update)."""
import hashlib
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import make_manifest as mm  # noqa: E402


class MakeManifestTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.dir = Path(self.folder.name)
        self.image = self.dir / "keepsake.bin"
        self.image.write_bytes(b"\xe9" + b"x" * 1000)

    def tearDown(self):
        self.folder.cleanup()

    def test_fields(self):
        manifest = mm.make_manifest(self.image, "v1.22.0", "https://example.com/k.bin", "New: reminders.")
        self.assertEqual(manifest["version"], "1.22.0")
        self.assertEqual(manifest["size"], 1001)
        self.assertEqual(manifest["sha256"], hashlib.sha256(self.image.read_bytes()).hexdigest())
        self.assertEqual(manifest["notes"], "New: reminders.")
        self.assertNotIn("notes", mm.make_manifest(self.image, "1.22.0", "https://example.com/k.bin"))

    def test_refusals(self):
        for version, url in (("1.2", "https://e.com/a"), ("01.2.3", "https://e.com/a"),
                             ("1.2.3", "http://e.com/a"), ("1.2.3", "https://e.com/a b")):
            with self.assertRaises(SystemExit):
                mm.make_manifest(self.image, version, url)

    def test_notes_file(self):
        notes = self.dir / "whats_new.txt"
        notes.write_text("# a comment\n\nReminders are here:\nset one from Home.\n\nSecond paragraph.\n")
        self.assertEqual(mm.read_notes(notes), "Reminders are here: set one from Home.")
        self.assertEqual(mm.read_notes(self.dir / "missing.txt"), "")
        notes.write_text("x" * 300)
        with self.assertRaises(SystemExit):
            mm.read_notes(notes)


if __name__ == "__main__":
    unittest.main()
