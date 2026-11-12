---
name: board-check
description: Run the Keepsake board tests on a board plugged into this computer over USB (self-test, screenshots, UI tour, optional crash/provisioning checks) and summarise the report. Use when the user says the board is plugged in, asks to test/check the board or device, or asks for on-device screenshots.
---

# Board check

The board is a Waveshare ESP32-S3-ePaper-3.97 running Keepsake, plugged into this computer by USB-C.
Everything is driven by `tools/board/` (see `docs/board-tests.md`).

1. **Setup (once):** `pip install -r tools/board/requirements.txt`. If the user wants the current
   checkout on the board, ESP-IDF v5.5 must be sourced (`idf.py --version` works).
2. **Is it there?** `python3 tools/board/keepsake_board.py version`. If no port is found: ask the user
   to check the cable is a data cable, the board is awake, and it is not in Settings → Storage →
   USB storage mode. If it answers with an old version, ask whether to flash.
3. **Run:**
   - standard: `python3 tools/board/run_board_tests.py`
   - with a fresh build: add `--flash` (only when the user asked to flash or agreed to; the first flash
     after #39 must be a full `idf.py flash` over USB, which `--flash` does)
   - add `--interactive` when the user is at the board and can press buttons
   - add `--destructive` only when the user agrees: it crashes the board on purpose and reboots it
4. **Report:** read `build/board-tests/<latest>/report.md`. Summarise for the user: what failed, what
   warned and why it might (e.g. RTC not set, no Wi-Fi saved, SHTC3 rail), and look at the screenshots
   in `screens/` (open the PNGs) to say whether the screens look right. Mention anything from
   `serial.log` that looks like an error around a failure.
5. **Follow up:** for each real failure, find the cause in the code, and when it's a firmware bug,
   fix it on a branch (one issue per branch, see CLAUDE.md), then re-run the relevant check.
   Comment on the matching GitHub issue (`--repo ekhumbata/keepsake`) with what the board showed.

Never put Wi-Fi passwords or the friend's details in commands, files or reports that get committed.
