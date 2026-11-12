"""Talk to a Keepsake board plugged in over USB (board tests, #15).

The firmware's test console (firmware/main/test_console_runtime.cpp, CONFIG_KEEPSAKE_TEST_CONSOLE)
reads "kst <command>" lines from the USB serial port and answers in the log stream:

    @@KST {json}              one reply per command
    @@KSTD <index> <base64>   data lines, when the reply has "data_lines"

Everything else on the port is ordinary log output, kept in Board.log for tests that look at it.

Usage as a script:
    python3 tools/board/keepsake_board.py selftest
    python3 tools/board/keepsake_board.py screenshot home.png
    python3 tools/board/keepsake_board.py raw "press down click"
"""
from __future__ import annotations

import base64
import json
import struct
import sys
import time
import zlib
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

REPLY_PREFIX = "@@KST "
DATA_PREFIX = "@@KSTD "
ESPRESSIF_VID = 0x303A
READY_MARKER = "Board test console ready"


class BoardError(RuntimeError):
    pass


def find_port() -> str:
    """The first Espressif USB serial port (the ESP32-S3's built-in USB)."""
    from serial.tools import list_ports  # imported here so the parsing helpers need no pyserial

    ports = [p for p in list_ports.comports() if p.vid == ESPRESSIF_VID]
    if not ports:
        raise BoardError("no Espressif USB port found: is the board plugged in (and not in USB "
                         "storage mode)? Pass --port to pick one.")
    return ports[0].device


def quote(word: str) -> str:
    if word and all(c not in word for c in ' \t"\\'):
        return word
    return '"' + word.replace("\\", "\\\\").replace('"', '\\"') + '"'


def command_line(*words: str) -> str:
    return "kst " + " ".join(quote(w) for w in words) + "\n"


@dataclass
class Reply:
    data: dict
    payload: bytes = b""

    @property
    def ok(self) -> bool:
        return bool(self.data.get("ok"))

    def __getitem__(self, key):
        return self.data[key]

    def get(self, key, default=None):
        return self.data.get(key, default)


@dataclass
class ReplyReader:
    """Turns the raw line stream into replies (and keeps the log). Separate from the port so
    it can be tested without a board."""

    log: list = field(default_factory=list)
    _pending: Optional[dict] = None
    _chunks: dict = field(default_factory=dict)
    replies: list = field(default_factory=list)

    def feed_line(self, line: str) -> None:
        line = line.rstrip("\r\n")
        at = line.find(REPLY_PREFIX)
        data_at = line.find(DATA_PREFIX)
        if data_at >= 0 and self._pending is not None:
            parts = line[data_at + len(DATA_PREFIX):].split(" ", 1)
            if len(parts) == 2 and parts[0].isdigit():
                self._chunks[int(parts[0])] = base64.b64decode(parts[1])
                self._finish_if_complete()
            return
        if at >= 0:
            try:
                data = json.loads(line[at + len(REPLY_PREFIX):])
            except json.JSONDecodeError:
                self.log.append(line)
                return
            if data.get("data_lines", 0) > 0:
                self._pending = data
                self._chunks = {}
            else:
                self.replies.append(Reply(data))
            return
        self.log.append(line)

    def _finish_if_complete(self) -> None:
        assert self._pending is not None
        count = self._pending["data_lines"]
        if len(self._chunks) == count and all(i in self._chunks for i in range(count)):
            payload = b"".join(self._chunks[i] for i in range(count))
            self.replies.append(Reply(self._pending, payload))
            self._pending = None
            self._chunks = {}


class Board:
    def __init__(self, port: Optional[str] = None, baud: int = 115200):
        self.port_name = port
        self.baud = baud
        self.reader = ReplyReader()
        self._serial = None
        self._partial = b""

    @property
    def log(self) -> list:
        return self.reader.log

    # --- connection ---------------------------------------------------------------------
    def open(self, timeout: float = 20.0) -> "Board":
        import serial

        deadline = time.monotonic() + timeout
        last_error = None
        while time.monotonic() < deadline:
            try:
                name = self.port_name or find_port()
                self._serial = serial.Serial(name, self.baud, timeout=0.1)
                self.port_name = name
                return self
            except Exception as error:  # the port comes and goes while the board reboots
                last_error = error
                time.sleep(0.5)
        raise BoardError(f"couldn't open the board's port: {last_error}")

    def close(self) -> None:
        if self._serial is not None:
            self._serial.close()
            self._serial = None

    def reconnect(self, timeout: float = 30.0) -> None:
        """After a reboot: the USB port drops and comes back."""
        self.close()
        time.sleep(1.0)
        self.open(timeout)

    def _pump(self, wait: float) -> None:
        if self._serial is None:
            raise BoardError("not connected")
        end = time.monotonic() + wait
        while True:
            try:
                chunk = self._serial.read(4096)
            except Exception as error:
                raise BoardError(f"lost the board: {error}") from error
            if chunk:
                self._partial += chunk
                while b"\n" in self._partial:
                    line, self._partial = self._partial.split(b"\n", 1)
                    self.reader.feed_line(line.decode("utf-8", errors="replace"))
            if time.monotonic() >= end:
                return

    def wait_for_log(self, predicate: Callable[[str], bool], timeout: float, start: int = 0) -> Optional[str]:
        deadline = time.monotonic() + timeout
        seen = start
        while time.monotonic() < deadline:
            self._pump(0.1)
            for line in self.log[seen:]:
                if predicate(line):
                    return line
            seen = len(self.log)
        return None

    def wait_ready(self, timeout: float = 40.0) -> bool:
        """Waits for the console to say it's ready (after a reset), or answer a command."""
        if self.wait_for_log(lambda l: READY_MARKER in l, min(timeout, 15.0)):
            return True
        try:
            return self.command("version", timeout=max(1.0, timeout - 15.0)).ok
        except BoardError:
            return False

    # --- commands ---------------------------------------------------------------------
    def command(self, *words: str, timeout: float = 10.0) -> Reply:
        if self._serial is None:
            raise BoardError("not connected")
        self.reader.replies.clear()
        self._serial.write(command_line(*words).encode())
        self._serial.flush()
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            self._pump(0.05)
            if self.reader.replies:
                return self.reader.replies.pop(0)
        raise BoardError(f"no reply to '{' '.join(words)}' within {timeout:.0f} s")

    def screenshot(self, path: Optional[Path] = None) -> tuple:
        reply = self.command("screenshot", timeout=30.0)
        if not reply.ok:
            raise BoardError(reply.get("error", "screenshot failed"))
        width, height, pixels = raw_to_portrait(reply.payload, reply["width"], reply["height"])
        if path is not None:
            Path(path).parent.mkdir(parents=True, exist_ok=True)
            Path(path).write_bytes(png_bytes(width, height, pixels))
        return reply, pixels

    def put_file(self, card_path: str, data: bytes) -> Reply:
        return self.command("put", card_path, base64.b64encode(data).decode(), timeout=20.0)

    def get_file(self, card_path: str) -> Optional[bytes]:
        reply = self.command("get", card_path, timeout=20.0)
        return reply.payload if reply.ok else None


# --- screenshots ----------------------------------------------------------------------------

def raw_to_portrait(raw: bytes, raw_width: int, raw_height: int) -> tuple:
    """The panel's landscape 1-bit framebuffer (set bit = white) -> portrait rows of 0/1
    (1 = black), the way the UI is drawn: portrait (x, y) is raw (y, raw_height - 1 - x)."""
    stride = raw_width // 8
    if len(raw) < stride * raw_height:
        raise BoardError("screenshot is short")
    width, height = raw_height, raw_width
    pixels = []
    for y in range(height):
        row = bytearray(width)
        raw_x = y
        byte_index = raw_x // 8
        mask = 0x80 >> (raw_x % 8)
        for x in range(width):
            raw_y = raw_height - 1 - x
            white = raw[raw_y * stride + byte_index] & mask
            row[x] = 0 if white else 1
        pixels.append(bytes(row))
    return width, height, pixels


def png_bytes(width: int, height: int, pixels: list) -> bytes:
    """A grayscale PNG from rows of 0/1 (1 = black), with only the standard library."""
    raw = b"".join(b"\x00" + bytes(255 if p == 0 else 0 for p in row) for row in pixels)

    def chunk(kind: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def black_fraction(pixels: list) -> float:
    total = sum(len(row) for row in pixels)
    return sum(sum(row) for row in pixels) / total if total else 0.0


# --- command line ------------------------------------------------------------------------

def main(argv: list) -> int:
    import argparse

    parser = argparse.ArgumentParser(description="Send one command to a plugged-in Keepsake board.")
    parser.add_argument("--port")
    parser.add_argument("command", choices=["selftest", "version", "screen", "screenshot", "raw"])
    parser.add_argument("args", nargs="*")
    options = parser.parse_args(argv)
    board = Board(options.port).open()
    try:
        if options.command == "screenshot":
            path = Path(options.args[0] if options.args else "screenshot.png")
            reply, _ = board.screenshot(path)
            print(f"saved {path} ({reply['screen']})")
            return 0
        words = options.args if options.command == "raw" else [options.command]
        if options.command == "raw" and len(words) == 1:
            words = words[0].split()
        reply = board.command(*words, timeout=30.0)
        print(json.dumps(reply.data, indent=2))
        return 0 if reply.ok else 1
    finally:
        board.close()


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
