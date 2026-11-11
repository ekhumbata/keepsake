# Changelog

All notable changes to Keepsake (forked from Chroninkle at 1.0.0; Chroninkle
was renamed from Followup at 0.6.0) are documented here. Format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/); versioning follows
[Semantic Versioning](docs/versioning.md). History prior to 0.2.0 is available
via `git log`, not backfilled here.

## [Unreleased]

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
