"""Runs the board checks against a fake board that speaks the console protocol, so the
runner itself is tested without hardware."""
import sys
import tempfile
import types
import unittest
from unittest import mock
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import keepsake_board as kb  # noqa: E402
import run_board_tests as runner  # noqa: E402


class FakeBoard:
    """Home -> (down, fn) opens Books; back returns home. Files live in a dict."""

    def __init__(self):
        self.port_name = "/dev/fake"
        self.log = ["I (1) TestConsole: Board test console ready"]
        self.screen = "home"
        self.files = {}

    def _frame(self):
        # 800x480 landscape, mostly white with a black band (a "drawn" screen).
        stride = 800 // 8
        raw = bytearray([0xFF] * (stride * 480))
        for y in range(100, 140):
            for x in range(stride):
                raw[y * stride + x] = 0x00
        return bytes(raw)

    def command(self, *words, timeout=10.0):
        cmd = words[0]
        if cmd == "version":
            version = (runner.REPO / "firmware" / "version.txt").read_text().strip()
            return kb.Reply({"cmd": cmd, "ok": True, "version": version, "partition": "ota_0", "uptime_ms": 5000})
        if cmd == "selftest":
            return kb.Reply({"cmd": cmd, "ok": True, "checks": [
                {"name": "sd", "status": "pass", "detail": "write/read ok"},
                {"name": "climate", "status": "warn", "detail": "SHTC3 unreadable"}]})
        if cmd == "screen":
            return kb.Reply({"cmd": cmd, "ok": True, "screen": self.screen})
        if cmd == "press":
            if words[1] == "action":
                self.screen = "home"
            elif words[1] == "fn" and self.screen == "home":
                self.screen = "book_list"
            return kb.Reply({"cmd": cmd, "ok": True, "screen": self.screen})
        if cmd == "rm":
            self.files.pop(words[1], None)
            return kb.Reply({"cmd": cmd, "ok": True})
        return kb.Reply({"cmd": cmd, "ok": False, "error": "unknown"})

    def screenshot(self, path=None):
        reply = kb.Reply({"cmd": "screenshot", "ok": True, "width": 800, "height": 480, "screen": self.screen},
                         self._frame())
        width, height, pixels = kb.raw_to_portrait(reply.payload, 800, 480)
        if path is not None:
            Path(path).parent.mkdir(parents=True, exist_ok=True)
            Path(path).write_bytes(kb.png_bytes(width, height, pixels))
        return reply, pixels

    def put_file(self, path, data):
        self.files[path] = data
        return kb.Reply({"cmd": "put", "ok": True})

    def get_file(self, path):
        return self.files.get(path)

    def close(self):
        pass


class RunnerTest(unittest.TestCase):
    def test_standard_checks_against_a_fake_board(self):
        with tempfile.TemporaryDirectory() as folder:
            out = Path(folder)
            (out / "screens").mkdir()
            run = runner.Run(FakeBoard(), out, types.SimpleNamespace())
            with mock.patch.object(runner.time, "sleep", lambda _seconds: None):  # no waiting
                for check in (runner.check_version, runner.check_selftest, runner.check_screenshot,
                              runner.check_card_round_trip, runner.check_ui_tour):
                    check(run)
            runner.write_report(run)
            statuses = {r.name: r.status for r in run.results}
            self.assertEqual(statuses["version"], "pass")
            self.assertEqual(statuses["selftest/climate"], "warn")
            self.assertEqual(statuses["screenshot"], "pass")
            self.assertEqual(statuses["sd/round-trip"], "pass")
            self.assertEqual(statuses["ui/tour"], "pass")
            report = (out / "report.md").read_text()
            self.assertIn("ui/tour", report)
            self.assertTrue(list((out / "screens").glob("*.png")))


if __name__ == "__main__":
    unittest.main()
