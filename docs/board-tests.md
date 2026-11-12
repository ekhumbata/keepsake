# Board tests

Plug the Keepsake board into a computer with a USB-C **data** cable and run:

```bash
pip install -r tools/board/requirements.txt     # once: pyserial
python3 tools/board/run_board_tests.py           # the standard checks (about a minute)
```

The runner finds the board (the ESP32-S3's built-in USB, vendor `303A`), talks to the firmware's
**test console**, prints a line per check and writes a report to `build/board-tests/<time>/`:
`report.md`, `results.json`, `serial.log` and `screens/*.png` (real screenshots of the panel).
It exits non-zero if anything failed.

| Option | What it adds |
|---|---|
| `--flash` | builds and flashes this checkout first (needs ESP-IDF: `idf.py` on `PATH`) |
| `--interactive` | asks you to press each real button and checks the firmware saw it |
| `--ota` | rehearses an update from GitHub Releases: a forced rollback, then a real install (needs Wi-Fi and a release newer than the board's firmware, so flash an older build first) |
| `--destructive` | crashes the board on purpose (checks the crash report, #41) and tries a broken provisioning file (#42); both reboot it |
| `--port /dev/…` | when there's more than one board, or detection picks the wrong port |

The board must be awake, on a normal screen (not in **USB storage** mode, which takes over the USB
port), with an SD card in.

## What's checked

- **version**: the firmware's version, and whether it matches this checkout (`firmware/version.txt`).
- **selftest** (run on the board): firmware + coredump partition, free memory and PSRAM, NVS
  write/read, PMIC and battery, RTC (readable, ticking, set), IMU (about 1 g at rest), temperature
  and humidity sensor, SD card write/read/free space, display (drawn), Wi-Fi state and saved
  networks, update slots and state (`warn` while a fresh update is still being checked). Each is `pass`, `warn` (works but worth a look, e.g. the clock isn't set yet) or `fail`.
- **screenshot**: the panel's current image, saved as a PNG; checks it isn't blank.
- **sd/round-trip**: writes, reads back and deletes a file through the console.
- **ui/tour**: from Home, simulated rocker presses open a few screens (each screenshotted), and
  Back (a quick Record tap) brings it home again.
- `--interactive` **buttons**: each physical button reaches the firmware.
- `--destructive` **crash-report**: `kst crash yes` → after the restart a new
  `/.keepsake/crash/crash-NNN.txt` says "Reset reason: panic" (and usually a `.elf` core dump).
- `--ota` **ota/rollback**: `kst ota fail-next yes`, then `kst ota check` downloads the newest
  release and `kst ota restart` starts it. Its health check fails on purpose, so after ~90 s the
  board must be back on the version it had, with that release marked not to install
  (`kst ota clear-bad` then lifts the mark).
- `--ota` **ota/install**: the same without the forced failure. The board must end up on the new
  version, marked valid.
- `--destructive` **provisioning/refusal**: a provisioning file with a too-short password is
  left on the card, nothing is imported, and `provision-problems.txt` names the field.

## One command at a time

```bash
python3 tools/board/keepsake_board.py selftest
python3 tools/board/keepsake_board.py screenshot home.png
python3 tools/board/keepsake_board.py raw "press down click"
python3 tools/board/keepsake_board.py raw "ls /.keepsake"
```

## The test console

`firmware/main/test_console_runtime.cpp`, built with `CONFIG_KEEPSAKE_TEST_CONSOLE` (on by default;
menuconfig → Chroninkle Settings). It reads lines from the USB serial port; anything that doesn't
start with `kst` is ignored. Replies come back in the log stream, prefixed so they can't be
mistaken for logs (`core/board_test` has the format):

```text
> kst version
< @@KST {"cmd":"version","ok":true,"version":"1.18.0",...}
> kst screenshot
< @@KST {"cmd":"screenshot","ok":true,"width":800,"height":480,"data_lines":94,...}
< @@KSTD 0 <base64>          (94 data lines, 512 bytes each)
```

| Command | Does |
|---|---|
| `help`, `version`, `screen` | what's there, firmware info, the current screen's name |
| `selftest` | the checks above, as `{"checks":[{"name","status","detail"}]}` |
| `screenshot` | the raw 1-bit framebuffer (800×480 landscape, set bit = white) as data lines |
| `press <action\|fn\|up\|down> [click\|hold\|double]` | simulates a button (logged as `INJECT`) |
| `ls [/path]`, `get /path`, `put /path <base64>`, `rm /path` | small files on the SD card (16 KB max; no `..`) |
| `reboot`, `crash yes` | restart; crash on purpose (for the crash report test) |
| `ota`, `ota check`, `ota restart`, `ota fail-next yes`, `ota clear-bad` | update state; check/download now; start a downloaded update; make the next update roll back on purpose; offer a rolled-back version again |

Anyone with the board and a cable can use these. They can't reach anything the person holding
the board couldn't already, but turn the console off for a build that shouldn't accept commands.

## Adding a check

On the board: add a `Check…()` function to `test_console_runtime.cpp` and list it in
`CommandSelftest()`. On the computer: add a `check_…(run)` function to
`tools/board/run_board_tests.py` and add it to `checks` in `main()`. Host tests for the tools
(`python3 -m unittest discover -s tools/board/tests`) run in CI with a fake board.
