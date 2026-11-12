# Changelog

All notable changes to Keepsake (forked from Chroninkle at 1.0.0; Chroninkle
was renamed from Followup at 0.6.0) are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning follows
[Semantic Versioning](docs/versioning.md). History prior to 0.2.0 is available
via `git log`, not backfilled here.

## [Unreleased]

### Added

- **Settings → Saved Wi-Fi** (#43). A new heading lists the networks Keepsake
  remembers, most recent first, with "(connected)" on the current one. Picking one asks
  "Forget this Wi-Fi?", and **Forget** removes it (`wifi_service::ForgetNetwork`), with a
  toast either way. With none saved, a toast points to Network.

- **Updates over the air** (#40). Once a day (and from Settings → About → **Check for
  updates**) Keepsake looks at the latest GitHub release, downloads a newer firmware into
  the other app slot over HTTPS, and checks its size, SHA-256 and app header before it can
  start. The update starts the next time the device goes to sleep (quietly, without the
  start-up sound), on any restart, or from **Restart to update**. A new firmware has to
  reach Home with a working display, and a working SD card if it had one, and keep running
  for 20 s. Otherwise the device goes back to the previous firmware (rollback was already
  enabled) and never offers that version again. After a good update, Home shows a
  "Keepsake updated" card with Mochi and the release's notes (`firmware/whats_new.txt`).
  Background checks wait for 30 % battery (15 % when asked) or USB.
- **Release workflow** (`.github/workflows/release.yml`): pushing a `vX.Y.Z` tag that
  matches `firmware/version.txt` builds with the tracked defaults only and publishes the
  OTA image, a full-flash image, `latest.json` (`tools/release/make_manifest.py`) and
  `SHA256SUMS`. A tag with a suffix (`v1.22.0-rc1`) makes a prerelease, which devices
  never install on their own.
- Board tests: the self-test reports the app slots and update state, and the console gains
  `kst ota [check|restart|fail-next yes|clear-bad]`. `run_board_tests.py --ota` rehearses
  a forced rollback and then a real install.

- **Special days with the mascots** (#85, #20). Besides the birthday: the anniversary
  and the first date ("Happy 5th anniversary!", "7 years since your first date" when the
  year is given), days of your own from the provisioning file (`special_days`: date,
  title, message), and Christmas, New Year's Day, Valentine's Day and Halloween (choose
  with `holidays`). On the day, Home's title says so, and the first time Home shows that
  day a card with a pixel mascot wishes them well.
- **Bookworm and Mochi**, placeholder 1-bit pixel-art mascots (#20), drawn by
  `tools/draw_mascots.py` into `assets/mascots/`: their everyday look and festive
  versions (party hat, Santa hat, fireworks, hearts, the two together, a flower, a
  pumpkin, sparkles).

### Added

- **"Let's reconnect"** (#43). When the saved Wi-Fi turns the password away (it was
  probably changed) and every saved network has been tried, Keepsake shows a card once
  per start-up; **Set up** turns on its setup hotspot and opens the Wi-Fi page. Being out
  of range doesn't trigger it.

### Added

- **Personal touches** (#85). With a name in the provisioning file, Home alternates its
  usual prompts with a greeting ("Good evening, Sam"); on the birthday it reads "Happy
  birthday, Sam!" all day (Feb 29 birthdays on Feb 28 in other years). The welcome
  message from the provisioning file shows once as a card on the first Home (after
  onboarding, if that's showing); a new provisioning file with a new message shows it
  again.

- **Board tests** (#15). A test console in the firmware (on by default, USB serial,
  `kst ...` commands) and `tools/board/run_board_tests.py` on the computer: plug the board
  in and run it for a self-test of every chip (PMIC, RTC, IMU, sensor, SD card, display,
  memory, Wi-Fi), real screenshots of the panel, an SD card round trip and a UI tour with
  simulated button presses; optionally a by-hand button check, a forced crash and a broken
  provisioning file. See `docs/board-tests.md`.

### Added

- **Crash reports on the SD card** (#41). After a crash (panic, watchdog or brownout)
  the next start writes `/.keepsake/crash/crash-NNN.txt` with the reset reason, the
  crashed task and backtrace, and the last log lines from before the crash, plus the core
  dump as `crash-NNN.elf`. The last 10 are kept. Nothing is shown on screen.

### Changed

- **Final flash layout, with room for crash dumps** (#39). The two app slots are 128 KB
  smaller (still almost 8 MB each) and the last 128 KB of flash keeps a core dump when
  the firmware crashes. **Flash this build over USB** (a full `idf.py flash`): OTA can't
  change the partition table.

### Fixed

- **No more double spaces in `.txt` books** (#90). Files saved on Windows (CRLF line
  endings), tabs and runs of spaces now show as single spaces; blank lines still start
  a new paragraph.

### Added

- **Several saved Wi-Fi networks** (#43). Keepsake remembers up to five networks,
  joins the most recently used one, and tries the others in turn when it stops
  answering. Networks from a provisioning file are all saved (the first one is used
  first). Forgetting a network now forgets just that one.

### Added

- **Provisioning file** (#42). Put `.keepsake/provision.json` on the SD card (format:
  `provision.example.json`) and Keepsake imports it at start-up: Wi-Fi network,
  timezone, Gemini key, and the friend's name, birthday, welcome message and weather
  location. It then overwrites and deletes the file and shows "All set up!". A file with
  mistakes imports nothing and gets a `provision-problems.txt` next to it saying what to
  fix.

### Added

- **Continue reading, with the cover** (#27). The top of the Books screen shows the book
  you're reading: its cover (dithered to black and white), title, author and a progress
  bar. Covers are decoded once and kept in `books/.cache/`.

- **`.txt` books in any common encoding** (#27). UTF-16 (Notepad's "Unicode") and old
  Windows/Latin-1 files used to show as garbage; the encoding is now detected when the
  book opens. Accented letters no longer split words in two (`naive`, not `nai ve`).

- **Hold to skip a chapter** (#28). In an EPUB, holding the rocker down jumps to the
  next chapter; holding it up goes back to the start of this chapter, or the one
  before if you're already there.

- **Bookmarks** (#28). The reader menu has **Add bookmark** / **Remove bookmark** and,
  once a book has some, **Bookmarks** to jump back to one. A bookmarked page shows a
  small ribbon in the top right corner. They're kept in a `.bm` file next to the book
  (deleted with it), and they stay put when the text size changes.

- **Go to a point in a book** (#28). The reader menu has **Go to...**: 0%, 10%, ...
  100%, with the nearest one to where you are highlighted. Works for EPUB and `.txt`.
  EPUB progress now weighs chapters by their size, so a short preface no longer
  counts as much of the book as a long chapter.

- **Text size for books** (#28). The reader menu has **Text size**: Small, Medium or
  Large (Literata at 19, 22 or 26 px). Changing it keeps you on the same passage and
  is remembered on the device. `.txt` books now use Literata too, so the size applies
  to them as well.

### Fixed

- **Notes saved while offline no longer get marked "couldn't start"** (#17). When
  Wi-Fi came back, the catch-up transcription could run before Gemini had finished
  signing in, which marked every waiting note as failed instead of leaving it to
  retry. It now waits for Gemini, and the catch-up holds the connection until it's
  done. With Wi-Fi on demand switched on, a new recording asks for the network
  straight away (the toast says "Saved. Transcribing soon") instead of waiting for
  the next sync.

### Changed

- **Works fully without a Gemini key** (#44). With no key set, a recording is a
  playable voice note: its details page offers **Play** and reads "A voice note.
  Press Play to listen." instead of "No transcript available" or a Transcribe
  button; the "waiting to transcribe" badge never appears; topic pages drop
  **Summarize**; and the onboarding skips the "Summaries with Gemini" slide. With a
  key, everything works as before.

### Added

- **Reader menu with Contents** (#28). Pressing the rocker while reading opens a
  menu: **Contents** lists the book's chapters (with the current one highlighted)
  and jumps straight to the one you pick; **Back to books** returns to the shelf.

- **A proper library shelf** (#27). Books shows each book's real title (from the
  EPUB itself, or a tidied-up file name for `.txt`) instead of the file name, with
  how far you've read (`Emma - 42%`, `Done`). The book you're reading is at the top,
  then the rest A to Z. Books in subfolders of `books` are found too. Titles and
  progress are remembered in a small hidden `.library` file in the books folder.

### Changed

- **New fonts** (#19): the interface uses **Nunito**, a rounded, friendly face, and
  books are set in **Literata**, a typeface made for reading on screens, with real
  **bold** and *italic* in book text (italics showed as plain text before). Both are
  open-source (SIL OFL).

### Added

- **EPUB books** (#26, #28). `.epub` files in the `books` folder now show up in
  Books next to `.txt` files and open in the reader: real chapters, headings, bold,
  lists and scene breaks, laid out for the screen. Tilting the rocker turns pages
  across chapters; the bottom line shows the chapter, the page within it and how far
  through the book you are, and your place is remembered. Locked (DRM), damaged and
  other unreadable files show a short, friendly explanation instead. Italics,
  pictures and a proper book font come later.

- **CPU frequency scaling** (power management, #16). The CPU idles at 80 MHz and runs
  at 240 MHz only while something a person is waiting on is happening: drawing the
  screen, handling a button, playing a feedback sound. Saves battery while idle;
  page turns and button response should feel the same (to be confirmed on the
  device). New "Keepsake power management" menuconfig options; automatic light
  sleep is still off.

### Changed

- **Rebranded to Keepsake** (user-facing only). The boot screen shows a
  Keepsake wordmark (a placeholder until the redesign's real logo) and credits
  "Chroninkle and" Followup/ALXV Labs. The About page credits both projects
  and the GPLv3. The onboarding welcome says Keepsake. The Wi-Fi setup hotspot
  is now `Keepsake-XXXXXX` and the setup portal is titled "Keepsake Setup".
  The device registers as `keepsake` on the router (DHCP hostname), and a
  freshly formatted SD card is labelled `KEEPSAKE`. Config symbols and
  internal names keep their Chroninkle names.

## [1.0.0] - 2026-09-24

This is the last planned release before v2, a complete front-end rewrite
(dropping the current text-driven copy for a graphical, icon-based
interface — see `docs/todo.md`). 1.0 marks this as the stable baseline the
v1 UI settles on before that rewrite starts.

### Added

- A new **About** page under Settings, showing the installed firmware
  version and an acknowledgment of the original Followup project (by ALXV
  Labs) this device builds on.

### Changed

- Finished the in-repo Followup → Chroninkle rename: the ESP-IDF project
  name, the `CONFIG_FOLLOWUP_*` → `CONFIG_CHRONINKLE_*` Kconfig namespace
  (including the Wi-Fi setup hotspot's default name, now
  `Chroninkle-XXXXXX`), the `followup_task_config` → `chroninkle_task_config`
  FreeRTOS task table, remaining source comments/log strings/onboarding
  copy, the `webserver/` setup portal, and the internal docs. Only the
  GitHub repository name remains, tracked separately in `docs/todo.md`.
- Screen navigation now uses a quick partial refresh instead of the slow,
  flashy full-screen waveform for almost every transition. Only going to
  the Home screen and locking/shutting down still do a full refresh; the
  existing idle-deferred ghost-clear flush (`EpaperPanel::NeedsGhostingFlush`)
  keeps ghosting in check automatically as partials become far more
  frequent.

### Fixed

- The AXP2101's emergency 6-second PWR hold now explicitly forces
  power-off (`Axp2101Driver::setLongPressPowerOFF()`, register 0x22 bit 0)
  instead of relying on the chip's unverified factory default, which could
  have been "restart" instead of "power off" — silently breaking the
  documented "hardware escape even if firmware is wedged" guarantee.
  Also removed `Axp2101::SetButtonPowerOffRestarts()`, an unused and
  misleadingly-named wrapper (it actually toggled an unrelated PWROK
  pin-pull-low feature on register 0x10, not this one) that had led a
  previous investigation to the wrong fix.

## [0.6.1] - 2026-09-18

### Fixed

- Selecting a row in Todos or Notes no longer reboots the device. The
  `input_callbacks` dispatcher task ran every page callback on a 4096-byte
  stack, but page entry reads the recording archive off the SD card, and that
  FATFS/SDMMC call chain peaked at 4088 bytes — leaving no margin, so an
  interrupt arriving at depth overwrote the stack canary. FreeRTOS only checks
  that canary at a context switch, which is why the panic appeared on the next
  button press and looked like Select was at fault. The dispatcher stack is now
  8192 bytes, matching the other real-work tasks in the project.

### Added

- The boot log now reports `esp_reset_reason()`, so an unexpected reset is
  distinguishable from a deliberate restart without attaching a serial monitor
  at the right moment.

### Changed

- The boot splash now leads with the new **Chroninkle** name and mark, in a
  stacked lockup (`assets/logos/chroninkle-logo-stacked.png`,
  `EmbeddedLogoId::kChroninkleLogoStacked`), reflecting the project's rename
  from Followup. A horizontal lockup (`chroninkle-logo.png`) and an
  icon-only mark (`chroninkle-icon.png`) are generated alongside it for
  future use. The previous Folloup logo and the ALXV Labs logo it launched
  under move to a smaller "Based on" attribution line at the bottom of the
  same splash (75% of their old splash size) rather than disappearing
  (`display_service::DrawSplashScreen`). No other Followup branding changes
  yet — this is the first step of the rename.
- The Record button's quick tap is now a dedicated **Back** button instead of
  a second select/confirm button: it exits an entered list/scroll/control
  (`button_service::IsBackGesture`). The rocker-middle FN button is now the
  sole select/confirm control (`button_service::IsSelectButton`). Holding the
  rocker's down-tilt no longer exits an entered control either — that hold is
  freed up for continuous scrolling within one, reserved for later. Recording
  (press-and-hold the Record button) is unaffected. Updated the onboarding
  carousel's text and artwork (`assets/images/slide2.png`,
  `assets/images/slide3.png`) to match.

## [0.4.0] - 2026-09-13

### Added

- On-device `.txt` reader: a new "Books" item on the Home menu browses `.txt`
  files in a configurable SD card folder (`CONFIG_FOLLOWUP_TEXT_READER_FOLDER`,
  default `books`) and opens a paginated reading view. Each book remembers its
  last-read page position in a `<filename>.pos` sidecar next to it on the SD
  card. Page turns (UP/DOWN tilt) use a partial refresh; long-press DOWN
  returns to the book list.

## [0.3.1] - 2026-09-13

### Fixed

- Holding PWR to power the device back on after a manual shutdown sometimes
  didn't register on the first attempt. Shortened the AXP2101's PWRON
  power-on hold requirement from 1s to 512ms
  (`Axp2101::PowerKeyPressOnTime::k512Ms`), reducing how much uninterrupted
  contact a single press needs against physical key bounce. Confirmed
  reliable on-device across repeated shutdown/power-on cycles.

## [0.3.0] - 2026-09-11

### Changed

- Reordered the Home screen's main menu to Follow up, Todos, Notes, Topics
  (was Follow up, Topics, Notes, Todos).

## [0.2.0] - 2026-09-11

### Added

- Semantic versioning for the firmware: `version.txt` as the source of truth,
  a boot-time version log line, this changelog, and `docs/versioning.md`
  describing the workflow.

### Changed

- Baselined the starting version at 0.2.0 rather than 0.1.0, reflecting the
  substantial feature work already merged since this repo forked from
  `alxv2016/folloup-sticky` (topics, summarization, offline-transcription
  retry, settings redesign, and more — see `git log` for the full history).
