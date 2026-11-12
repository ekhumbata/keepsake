"""Host tests for tools/board (no board needed): run with
    python3 -m unittest discover -s tools/board/tests
"""
import base64
import json
import sys
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import keepsake_board as kb  # noqa: E402


class ReplyReaderTest(unittest.TestCase):
    def test_replies_and_log_are_separated(self):
        reader = kb.ReplyReader()
        reader.feed_line("I (100) Main: booting\r\n")
        reader.feed_line('@@KST {"cmd":"version","ok":true,"version":"1.17.0"}')
        reader.feed_line("W (200) Other: something")
        self.assertEqual(len(reader.replies), 1)
        self.assertEqual(reader.replies[0]["version"], "1.17.0")
        self.assertTrue(reader.replies[0].ok)
        self.assertEqual(reader.log, ["I (100) Main: booting", "W (200) Other: something"])

    def test_data_lines_are_collected_in_any_order(self):
        payload = bytes(range(256)) * 5
        chunks = [payload[i:i + 512] for i in range(0, len(payload), 512)]
        reader = kb.ReplyReader()
        reader.feed_line("@@KST " + json.dumps({"cmd": "get", "ok": True, "data_lines": len(chunks)}))
        self.assertEqual(reader.replies, [])
        order = list(range(len(chunks)))[::-1]
        for i in order:
            reader.feed_line("I (5) log in between")
            reader.feed_line(f"@@KSTD {i} {base64.b64encode(chunks[i]).decode()}")
        self.assertEqual(len(reader.replies), 1)
        self.assertEqual(reader.replies[0].payload, payload)

    def test_a_reply_glued_to_a_log_line_still_counts(self):
        reader = kb.ReplyReader()
        reader.feed_line('I (1) X: hi@@KST {"cmd":"screen","ok":true,"screen":"home"}')
        self.assertEqual(reader.replies[0]["screen"], "home")

    def test_broken_json_is_just_log(self):
        reader = kb.ReplyReader()
        reader.feed_line("@@KST {not json")
        self.assertEqual(reader.replies, [])
        self.assertEqual(len(reader.log), 1)


class CommandLineTest(unittest.TestCase):
    def test_quoting_matches_the_firmware(self):
        self.assertEqual(kb.command_line("press", "down", "click"), "kst press down click\n")
        self.assertEqual(kb.command_line("put", "/a b", 'say "hi" \\'), 'kst put "/a b" "say \\"hi\\" \\\\"\n')


class ScreenshotTest(unittest.TestCase):
    def test_landscape_framebuffer_becomes_portrait(self):
        raw_w, raw_h = 16, 8  # tiny "panel": 2 bytes per row
        raw = bytearray([0xFF] * (raw_w // 8 * raw_h))  # all white
        # Black pixel at raw (x=3, y=raw_h-1-5): portrait (x=5, y=3).
        raw_y = raw_h - 1 - 5
        raw[raw_y * 2 + 0] &= ~(0x80 >> 3) & 0xFF
        width, height, pixels = kb.raw_to_portrait(bytes(raw), raw_w, raw_h)
        self.assertEqual((width, height), (8, 16))
        self.assertEqual(pixels[3][5], 1)
        self.assertEqual(sum(sum(r) for r in pixels), 1)
        self.assertAlmostEqual(kb.black_fraction(pixels), 1 / 128)

    def test_png_is_valid(self):
        png = kb.png_bytes(3, 2, [b"\x00\x01\x00", b"\x01\x01\x01"])
        self.assertTrue(png.startswith(b"\x89PNG\r\n\x1a\n"))
        idat_at = png.index(b"IDAT")
        length = int.from_bytes(png[idat_at - 4:idat_at], "big")
        raw = zlib.decompress(png[idat_at + 4:idat_at + 4 + length])
        self.assertEqual(raw, b"\x00\xff\x00\xff\x00\x00\x00\x00")

    def test_short_screenshot_is_an_error(self):
        with self.assertRaises(kb.BoardError):
            kb.raw_to_portrait(b"\x00", 16, 8)


if __name__ == "__main__":
    unittest.main()
