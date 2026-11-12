"""The photo prepare tool (#136), with generated images only."""
import sys
import tempfile
import unittest
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import prepare_photos as pp  # noqa: E402


def sideways_photo(path: Path, date: str = "2019:05:20 18:30:00") -> None:
    """2000 x 1000 stored, left half black, EXIF orientation 6 (upright: 1000 x 2000, top half black)."""
    image = Image.new("RGB", (2000, 1000), "white")
    image.paste((0, 0, 0), (0, 0, 1000, 1000))
    exif = Image.Exif()
    exif[0x0112] = 6
    exif[0x8769] = {0x9003: date}
    image.save(path, quality=90, exif=exif.tobytes())


class PreparePhotosTest(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.root = Path(self.folder.name)
        self.src = self.root / "src"
        self.out = self.root / "out"
        self.src.mkdir()
        self.out.mkdir()

    def tearDown(self):
        self.folder.cleanup()

    def test_upright_small_baseline_and_named_by_date(self):
        sideways_photo(self.src / "IMG_1234.jpg")
        target = pp.prepare(self.src / "IMG_1234.jpg", self.out)
        self.assertEqual(target.name, "2019-05-20.jpg")
        with Image.open(target) as result:
            self.assertEqual(result.size, (480, 960))
            self.assertLess(result.convert("L").getpixel((240, 100)), 60)    # top: black
            self.assertGreater(result.convert("L").getpixel((240, 900)), 200)  # bottom: white
            self.assertFalse(result.info.get("progressive"))
            self.assertNotIn(0x0112, result.getexif())

    def test_names(self):
        self.assertEqual(pp.output_name(Path("2019-05-20 First date.jpg"), ""), "2019-05-20 First date.jpg")
        self.assertEqual(pp.output_name(Path("20190520_first_date.jpg"), ""), "2019-05-20 first date.jpg")
        self.assertEqual(pp.output_name(Path("Beach day.png"), "2021-07-04"), "2021-07-04 Beach day.jpg")
        self.assertEqual(pp.output_name(Path("PXL_20230115_183012.jpg"), "2023-01-15"), "2023-01-15.jpg")
        self.assertEqual(pp.output_name(Path("Paris.jpg"), ""), "Paris.jpg")
        self.assertEqual(pp.output_name(Path("IMG_1234.jpg"), ""), "IMG_1234.jpg")
        self.assertEqual(pp.output_name(Path('a/b:c?.jpg'), ""), "bc.jpg")

    def test_duplicates_get_a_number(self):
        for name in ("one.png", "two.png"):
            Image.new("RGB", (100, 100), "gray").save(self.src / name)
        pp.prepare(self.src / "one.png", self.out)
        (self.src / "one.png").rename(self.src / "x.png")
        Image.new("RGB", (100, 100), "gray").save(self.src / "one.png")
        second = pp.prepare(self.src / "one.png", self.out)
        self.assertEqual(second.name, "one (2).jpg")

    def test_transparent_png_on_white(self):
        Image.new("RGBA", (50, 50), (0, 0, 0, 0)).save(self.src / "clear.png")
        target = pp.prepare(self.src / "clear.png", self.out)
        with Image.open(target) as result:
            self.assertGreater(result.convert("L").getpixel((25, 25)), 240)

    def test_refuses_to_write_into_the_repo(self):
        sideways_photo(self.src / "a.jpg")
        self.assertEqual(pp.main([str(self.src), str(pp.REPO / "photos-out")]), 2)
        self.assertFalse((pp.REPO / "photos-out").exists())

    def test_whole_folder(self):
        sideways_photo(self.src / "a.jpg")
        (self.src / "notes.txt").write_text("not a photo")
        (self.src / "broken.jpg").write_bytes(b"not a jpeg")
        self.assertEqual(pp.main([str(self.src), str(self.out)]), 1)  # one failed, the rest done
        self.assertEqual(sorted(p.name for p in self.out.iterdir()), ["2019-05-20 a.jpg"])


if __name__ == "__main__":
    unittest.main()
