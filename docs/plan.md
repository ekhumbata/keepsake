# Plan: **Keepsake**, a personal fork of Chroninkle for the Waveshare ESP32-S3-ePaper-3.97 (gift device)

## Context

Eric wants his own customizable firmware for the Waveshare ESP32-S3-ePaper-3.97 (already purchased), based on
[mach1na/chroninkle](https://github.com/mach1na/chroninkle), which is a fork of ALXV's
[folloup-sticky](https://github.com/alxv2016/folloup-sticky) (`folloup-waveshare` branch), the project behind the
MakerWorld "Folloup" enclosure. **The device is a gift for a non-technical friend who reads a lot (currently Libby
on a phone)**, so a good e-book reader is a must-have, the experience should feel cute and cozy, battery life
matters, and setup/recovery must work without any AI or API knowledge.

**Order of work:**
1. Phase 0: repo + toolchain + baseline
2. Power foundations
3. Phase 1: desktop simulator (optional, timeboxed)
4. Phase 1.5: visual identity
5. Phase 2: books & library
6. Phase 3: ambient dashboard
7. Phase 5: gift readiness
8. Gift day
9. Phase 4: reminders & timers, as the first OTA update, then the rest of the post-gift work
10. Phase 6: later features, kept as GitHub issues

**Deadline: about 1 month to gift**, so see "Schedule & scope" for what's in the MVP and what ships over the air
afterwards.

### Decisions already made
- **Name:** Keepsake, repo `ekhumbata/keepsake`, a GitHub fork of `mach1na/chroninkle` @ v1.0.0 (GPLv3). Public,
  since GitHub forks of public repos are public. An `upstream` remote is kept.
- **Simulator:** an SDL2 desktop sim running the real UI code. It's **optional and timeboxed**, with go/no-go checks;
  the device is always the source of truth. No chip-level emulation (Wokwi/QEMU).
- **Features in scope** (the MVP vs post-gift split is in "Schedule & scope"):
  - books (EPUB, TXT, MOBI/AZW3, later FB2/HTML/CBZ; USB drag-and-drop; phone Library app; Gutenberg search)
  - ambient dashboard
  - reminders & timers (time picker first; voice/AI parsing last)
  - a cute visual identity
  - battery-conscious design throughout
- **Deferred (GitHub issues):** PDF, notes/todos sync, alternative AI providers, reader extras.
- **Books, not supported:**
  - **Libby/OverDrive** has no public API, and its loans are DRM-protected, so a DIY device can't legally open them.
    Libby stays the friend's library app on their phone.
  - **Anna's Archive** and other shadow libraries are out: they distribute copyrighted books without permission.
- **Design:** kawaii-inspired but entirely original. **Bookworm** mascot for book pages, **Mochi cloud** for ambient
  screens; no Sanrio characters or art.
- **Storage:** 32 GB microSD. Audio is about 115 MB/h (≈250 h fits); EPUBs are 0.3–5 MB each.
- **Priorities:** books ≫ ambient ≫ reminders/timers ≫ **voice notes & AI (lowest)**. The inherited voice-notes +
  Gemini features stay working as they are, but get no new investment before gifting, and **the device must be fully
  usable with no Gemini key at all** (notes are then saved as playable audio).
- **Gemini (optional):** free tier from Google AI Studio (no card), default model `gemini-3.5-flash-lite`. Limits are
  per-project; free-tier requests may be used by Google to improve its products. Set up only if time allows: a
  dedicated account/project, no billing, the key restricted to the Generative Language API.

### Schedule & scope: gift in about 1 month, then over-the-air updates

The gift ships as an **MVP**, and the rest arrives as **OTA updates after gifting**. That makes reliable OTA +
rollback an MVP must-have: it's how everything in the "Post-gift" column reaches the friend. The phase sections
below hold the full design for both columns.

| Area | Gift MVP (≈ 4 weeks) | Post-gift (OTA updates, in priority order) |
|---|---|---|
| Setup | factory backup, toolchain, baseline, restructure, rebrand, CLAUDE.md files, guide, unit-test CI, milestones + issues | — |
| Power | `power_log` + baseline; frequency scaling; **Wi-Fi on demand (`net_jobs`)**; low-battery nudges + charging screen; display rules | automatic light sleep (needs careful latency testing), audio power gating, IMU interrupt, Battery saver + profiles UI |
| Simulator | 1a spike + 1b gallery **only if** each fits its timebox (max 3 days total); otherwise device screenshots | 1c interactive sim (only if still wanted) |
| Design | rounded UI font, tokens, both mascots (final or polished placeholder), new icons, **books-first Home screen**, restyle of Home / lock / books / toasts / onboarding, copy, personal touches | restyle of the remaining screens (notes/todos/topics last), sound set, style-guide polish |
| Books | **EPUB + TXT**, Literata reader (sizes, TOC, progress, cache), library with covers, **USB drag-and-drop UX**, DRM/PDF/corrupt-file messages, **phone Library web app** (Wi-Fi upload + Gutenberg search, so no computer is needed), on-device Gutenberg search by keyboard. *Stretch:* MOBI/AZW3 | MOBI/AZW3 (if not done), FB2, HTML, CBZ, Standard Ebooks, OPDS, on-device voice search (AI) |
| Ambient | ambient lock screen: clock, date/calendar, weather (Open-Meteo), indoor temp/humidity (SHTC3), night mode, fixed 5-min cadence | cadence tied to battery profiles, current book / next reminder on ambient |
| Reminders & timers | — | **first post-gift update:** reminders via the **time picker** + firing + list, then timers; voice/AI time parsing comes last |
| Voice notes & AI | keep working as inherited; works without a key (audio-only notes); hidden from onboarding when there's no key | lowest priority: notes restyle, transcription retry/badge, voice search, voice reminders, AI provider choice (issue #12) |
| Gift readiness | coredump partition + final serial flash, OTA + health check + rollback, crash logs to SD, **provisioning file + multiple saved networks**, offline behavior, onboarding, 2.4 GHz check, starter shelf, quick-start card, case print, burn-in. *Optional:* Gemini key | — |

**Week-by-week (adjust as reality hits; the go/no-go rules apply):**
- **Week 1:**
  - Phase 0 (backup, toolchain, fork, baseline + `power_log`, restructure, rebrand, CLAUDE.md files, guide, CI,
    milestones/issues)
  - sim spike/gallery (≤ 3 days, may be skipped)
  - start the mascot art and font choice
- **Week 2:**
  - power MVP (frequency scaling, `net_jobs`)
  - design system + restyle of the core screens
  - EPUB engine port (3-day timebox → plan B)
- **Week 3:**
  - library + reader UI, USB drag-and-drop UX, phone Library web app + Gutenberg search (MOBI/AZW3 if time allows)
  - the ambient screen (SHTC3, weather, night mode)
- **Week 4:**
  - gift readiness (partition, OTA + rollback, crash logs, provisioning + saved networks, onboarding, case;
    optional Gemini key)
  - **burn-in on Eric's own use** through the week; fixes only, no new features after mid-week
- **After gifting:** the first OTA brings reminders & timers, then the rest of the Post-gift column in order.

### Where work runs: cloud vs. Eric's machine

Most code work runs in **Claude Code cloud sessions** (claude.ai/code, the phone app, or `/schedule`) against
`ekhumbata/keepsake`:
- each issue → branch → PR
- GitHub Actions builds the firmware and runs the tests; the sim gallery posts headless screenshots to the PR
- Eric reviews PRs, including from his phone

**Bootstrap on Eric's machine before going cloud-first (≈10 min):**
1. Fork to `ekhumbata/keepsake` and enable Issues.
2. Install the Claude GitHub app on the repo.
3. Commit `docs/plan.md`.
4. Create the milestones and issues.

| Cloud-OK (no hardware) | Needs Eric's computer + the board |
|---|---|
| restructure, rebrand, CLAUDE.md files, guide, CI, issues | toolchain install, **factory backup**, baseline flash + checklist |
| font-generator port, unit tests, fuzzers, sim 1a/1b (headless PNGs) | interactive sim window (SDL), CrossMux benchmark |
| design tokens, restyle, placeholder original mascot sprites | final pixel art review on the real panel (ghosting/banding) |
| EPUB/MOBI engine, library, reader, phone Library web app, Gutendex client | page-turn feel, ghosting over 100 turns, USB drag-and-drop on Windows/macOS |
| `power_log`, `net_jobs`, PM config, SHTC3 driver (from datasheet), weather, ambient UI | power measurements, SHTC3 accuracy, sleep/wake latency |
| OTA + rollback, release workflow, provisioning, saved networks, crash logs, partition change | OTA/rollback rehearsal on the device, final serial flash, filling in the real provisioning file, burn-in, case print |
| schematic reading (Phase 3 spike questions) | anything needing the friend's private data |

**Rules:**
- **Private data never goes to cloud sessions.** The friend's Wi-Fi, name, birthday and keys exist only in Eric's
  local provisioning file; cloud sessions use `provision.example.json`.
- **Batch hardware checks:** cloud sessions land features during the week, and Eric holds a hardware session every
  2–3 days to flash `main`, run that milestone's on-device checks, and file bugs as issues.
- A PR that changes on-device behavior is labelled `needs-device-test` until Eric has checked it on the board.
  Merging is fine; the label is cleared at the next hardware session.
- The first hardware session (toolchain, backup, baseline) happens early in week 1, since everything is checked
  against that baseline.

### Progress tracking

**GitHub milestones and issues are the source of truth for what's done.**
- Every plan step and scope-table row is an issue, grouped into milestones `M0 Setup` … `M6 Gift` (gift MVP),
  `M7`/`M8` (post-gift) and `Later`.
- **Milestone progress:** https://github.com/ekhumbata/keepsake/milestones (a % bar per phase).
- **Closing:**
  - a code issue closes when its PR (`Closes #N`) merges
  - a hardware-only issue (label `hardware`) is closed by Eric after the hardware session, with a short result
    comment
- **Session handoff:** every Claude session, cloud or local, ends by commenting on its issue with what's done and
  what's left, so the next session can pick it up without context.
- **Terminal view:**
  - milestone progress:
    `gh api "repos/ekhumbata/keepsake/milestones?state=all" --jq '.[] | "\(.title): \(.closed_issues)/\(.open_issues+.closed_issues) done"'`
  - one milestone's issues: `gh issue list --repo ekhumbata/keepsake --milestone "M4 Books" --state all`
  - what's next for the board: `gh issue list --repo ekhumbata/keepsake --label hardware --state open`
- `docs/guide.md`'s feature list shows the user-facing status (✅/🚧/📋/💤), linked to issues.
- Optional: a GitHub Project board over the same issues (needs `gh auth refresh -s project` once).

### What we're starting from (key facts)
- ESP-IDF C++17 app, ~30 components. `main/` = composition (`app_shell.cpp` orchestration + per-page
  `{runtime, coordinator, interactions}` trios). `docs/app-architecture.md` is the authoritative design spec.
- **Hardware map:** `components/board/include/waveshare_board_config.h` and
  `docs/waveshare-epaper-hardware-spec.md`.
  - SSD1677 800×480 on SPI3
  - shared I2C (GPIO41/42) with AXP2101 `0x34`, PCF85063 `0x51` (INT **GPIO45**), QMI8658 `0x6B` (INT2 GPIO40),
    ES8311 `0x30`, **SHTC3 `0x70` (no driver yet)**
  - buttons ACTION/BOOT GPIO0, UP 4, FN 5, DOWN 6; PMIC IRQ GPIO38
  - SD 4-bit SDMMC; 16 MB flash / 8 MB PSRAM
- **Partitions:** `partitions.csv` has two 8 MB OTA app slots, and `CONFIG_BOOTLOADER_APP_ROLLBACK_ENABLE=y` is
  already set. **No coredump partition.**
- **Simulator-friendly seam:** `epaper_ui`, `design_tokens`, `project_assets`, `page_navigation` have **no ESP-IDF
  REQUIRES**. They're pure C++17 drawing into a 1-bpp framebuffer (`Draw*Page(uint8_t* fb, …, State...)`). Hardware
  sits behind `epaper_panel`, `board`, `button_service`, `audio_hal`, `sd_card`, `axp2101`, `pcf85063`, `qmi8658`.
- **USB drag-and-drop already exists:** `storage_service/usb_storage_backend.cpp` (TinyUSB MSC) exposes the SD card
  from Settings → Storage → "Enable OTG". FATFS is unmounted while it's active, and it has to be disabled
  explicitly.
- **The existing book reader is TXT-only:**
  - `components/text_reader_service` (lists `.txt` in the `books` folder; `.pos` sidecar progress)
  - `epaper_ui/book_list_page` and `book_reader_page` (`PaginateBookText`: greedy word-wrap)
  - `main/book_*_page_*`
- **Reusable EPUB engine:** CrossPoint Reader `lib/Epub/` (**MIT**, PlatformIO/Arduino-based). Its fork **CrossMux**
  (MIT) already has a **Waveshare ePaper 3.97 nightly** build. MIT code can go into GPLv3 if the notice is kept.
- **Other reusable pieces:**
  - `pcf85063` (**no alarm-set API yet**), `timezone_service`, `gemini_service` (`supports_structured_output`),
    `transcription_service`
  - `epaper_ui` widgets (`keyboard`, `time_input`, `card_modal`, `toast`, `progress_bar`, `select_list`)
  - `system_sound_service`, `device_sleep_service` + `main/device_sleep_runtime.cpp`, `lock_screen`
  - `webserver/` portal (TS/Vite, has `FileUpload`; served only in AP setup mode today)
- **Power today:**
  - CPU fixed at 240 MHz, with no `CONFIG_PM_ENABLE`
  - codec and amp always on
  - IMU polled every 200 ms
  - display sleep after 180 s, light sleep after 1800 s
  - waking pays a Wi-Fi reassociation + SD remount
- **The font generator is macOS-only** (`scripts/generate_epaper_fonts.py` uses CoreText).
- No unit tests.
- **Local env (WSL2):** gcc/g++, Node 26, Python 3.14 present; **no ESP-IDF, cmake, SDL2 or USB passthrough yet**.

---

## Repository layout

The emulator and the real firmware live side by side. **The sim never forks app code**: it compiles the same
`firmware/main` and `firmware/components/{core,services}` sources and only swaps the drivers. (Component paths
elsewhere in this plan use Chroninkle's current names. After the restructure they live under
`firmware/components/{core,drivers,services}/`, and `webserver/` becomes `web/`.)

```
keepsake/
├── CLAUDE.md                 # project map, golden rules, commands, issue conventions (loaded every session)
├── AGENTS.md                 # one-liner pointing to CLAUDE.md (for non-Claude agents)
├── README.md                 # what Keepsake is + attribution (Chroninkle, Followup, CrossPoint MIT, fonts OFL)
├── firmware/                 # ESP-IDF project: the real device build (`idf.py -C firmware build`)
│   ├── CLAUDE.md             # refresh/overlay rules, input precedence, recording phases, power, tasks, NVS/Kconfig, releases/OTA
│   ├── CMakeLists.txt  sdkconfig.defaults  partitions.csv  version.txt
│   ├── main/                 # app composition (shared with sim)
│   │   └── CLAUDE.md         # app_shell = orchestration only; page trio; "add a page" checklist
│   └── components/
│       ├── CLAUDE.md         # layering + dependency direction; "add a component" checklist
│       ├── core/             # PORTABLE (no ESP-IDF includes), unit-tested on host, used by sim:
│       │                     #   epaper_ui, design_tokens, project_assets, page_navigation, epub_engine,
│       │                     #   book_formats, reminder_rules
│       ├── drivers/          # hardware only: board, epaper_panel, axp2101, pcf85063, qmi8658, shtc3,
│       │                     #   sd_card, audio_hal, i2c_device
│       └── services/         # *_service components
├── tests/                    # host unit tests + fuzzers for core/ (doctest + libFuzzer), run in CI; independent of sim
├── sim/                      # desktop emulator (optional): host build, never flashed
│   ├── CLAUDE.md             # sim architecture, key map, adding a fake, golden-image workflow, known limits
│   ├── hal/  mocks/  app/  gallery/  fixtures/  sdcard/  tests/golden/
├── web/                      # (was webserver/) TS/Vite: setup portal + Library web app
│   └── CLAUDE.md             # build/copy steps, flash vs SD serving, API contract with firmware
├── tools/                    # asset/font generators (FreeType port), build/flash wrappers, grab_screen.py
├── assets/  fonts/           # design sources (mascots, icons, logos, fonts)
├── docs/                     # guide.md, architecture.md, hardware.md, power.md, simulator.md, plan.md, decisions/
└── .github/workflows/        # firmware.yml, tests.yml, sim.yml (if kept), web.yml
```

**CLAUDE.md vs AGENTS.md:** use **CLAUDE.md** files (Claude Code loads the root one every session and nested ones
in that subtree), plus a root `AGENTS.md` pointer. Content is ported from Chroninkle's 19 KB CLAUDE.md and split by
scope. The root file holds:
- **Repo map**, plus Phase 0's "path note" for the moved folders.
- **Golden rules:**
  - `core/` never includes ESP-IDF headers.
  - The sim never copies app code.
  - Generated files are never hand-edited.
  - One branch + PR per issue/feature; the PR closes its issue.
  - Behavior changes bump `version.txt` + `CHANGELOG.md`.
  - User-visible changes update `docs/guide.md` in the same PR.
  - Later work goes into GitHub issues, not TODO files.
  - **No personal data or secrets in the repo** (it's public): the friend's name, birthday, welcome text and API
    keys live in NVS or the SD config.
  - Build and flash only when asked.
  - The power rules (below).
- **Commands** for firmware, tests, sim and web.
- **Pointers** to `docs/plan.md`, `docs/architecture.md`, `docs/hardware.md`, `docs/power.md`.

Chroninkle's `.agents/skills/esp32-firmware-engineer` moves to `.claude/skills/` (its build/flash wrappers go to
`tools/`) or gets dropped if unused.

**Upstream policy:** after the restructure and redesign, merge upstream only for fixes (cherry-pick driver/power/
storage fixes). Chroninkle's own planned v2 UI rewrite will conflict with our redesign, so don't merge it. Check
upstream monthly.

---

## Power & performance (applies to every phase)

**Principle: save power in the background, never on the interaction.** A button press must show feedback quickly
(sound or partial refresh within about 0.6 s), and page turns stay instant. Savings come from the idle time: radios
off, CPU asleep, fewer refreshes. Users get one setting, **Battery: Always fresh / Balanced / Max battery**.

### Measure first (Phase 0)
- **`power_log` service:** a CSV row on SD every 15 min and on state changes, with:
  - battery % and voltage (AXP2101)
  - awake/display-sleep/light-sleep seconds
  - Wi-Fi on-seconds
  - refresh counts (full/fast/partial)
  - audio-on seconds
  - wake and reset reasons

  Settings → About shows "≈ N days left".
- **Optional lab gear:** an inline current meter (Nordic PPK2, or an INA219 breakout logged by a spare
  microcontroller).
- **Baseline per state** (awake idle, display sleep, light sleep, Wi-Fi idle, recording, a full/partial refresh)
  goes into `docs/power.md`. Battery targets are set from it (reference: Waveshare's claim of 15+ days in clock mode
  on 1500 mAh).

### Power foundations (after Phase 0, before features)

**Gift MVP includes** items 2 (Wi-Fi on demand), 5 (display), frequency scaling from item 1, and the charging screen
+ low-battery nudges from item 7. **Post-gift:** automatic light sleep (item 1), audio gating (3), IMU interrupt (4),
SHTC3 sleep/SD batching (6), Battery saver + profiles (7).

1. **ESP-IDF power management:**
   - enable `CONFIG_PM_ENABLE`, frequency scaling (80 MHz idle → 240 MHz under load), tickless idle and automatic
     light sleep
   - drivers hold PM locks during SPI/I2S/SDMMC/TinyUSB work
   - buttons, PMIC IRQ, RTC INT (GPIO45, light sleep only) and IMU INT2 are wake sources
   - **a `KEEPSAKE_DEV_NO_AUTO_SLEEP` Kconfig switch** for dev builds, because automatic light sleep drops the
     USB-Serial/JTAG console
   - verify page-turn and button latency don't change
2. **Wi-Fi on demand:**
   - the radio is off by default; a `net_jobs` queue batches transcription, weather, catalog search/download, OTA
     check and daily time sync
   - Wi-Fi comes up, drains the queue, then turns off after a ~30 s linger
   - cached BSSID/channel for fast reconnect; `WIFI_PS_MAX_MODEM` while connected
   - user-triggered flows start the radio immediately, so nothing feels delayed
3. **Audio power gating:**
   - the codec and amp power down after ~5 s of silence, managed by a refcounted `audio_power` with a hold-off timer
     (so overlapping cues don't clip, the reason Chroninkle kept them on), plus fade in/out against pops
   - measure the saving to confirm it's worth it
4. **IMU by interrupt:** use the existing `qmi8658` INT2 motion detection instead of 200 ms polling.
5. **Display:**
   - keep the latest-wins refresh coalescing; the panel is in deep sleep between refreshes
   - never show seconds
   - partial refresh by default, full refresh only for the ghosting flush
6. **Sensors and SD:**
   - SHTC3 sleeps between reads
   - SD writes are batched (power log; reading progress saved after a debounce or on exit)
   - check in the schematic whether the SD rail can be gated (otherwise that goes to issue #15)
7. **Battery-aware UX:**
   - a charging screen with Mochi
   - nudges at 20/10/5%
   - automatic **Battery saver** at 15%: ambient cadence → 15 min, background jobs wait for charging, quieter
     sounds
   - big jobs (OTA, large downloads, a transcription backlog) wait for USB power when the battery is low
   - battery % smoothed

### Performance
- **Books:**
  - parse + paginate once and cache under `/Books/.cache/`
  - pre-render the next page into PSRAM while idle, so a page turn only refreshes the panel
  - cache dithered covers/images
  - glyph cache
- **Boot:** the e-paper already shows the last screen, so Wi-Fi, library rescans and transcription retries run off
  the critical path.
- **Network:** keep-alive within a batch; stream downloads straight to SD.
- **Flash/RAM:** big extras (extra fonts, web assets) go on SD where possible; watch `SPIRAM_MALLOC_RESERVE_INTERNAL`
  and app size against the 8 MB OTA slot.

### Rules (root CLAUDE.md)
- Every feature PR includes a **Power impact** note: wake sources, radio use, refreshes per hour, polling.
- No polling where an interrupt exists; no idle timers under 1 s.
- Background network work only goes through `net_jobs`.
- Compare `power_log` before and after any feature that runs while idle.

---

## Phase 0 — Repo, toolchain, baseline

Order matters: get a working baseline of the **unmodified** firmware before changing anything.

1. **Toolchain:**
   - ESP-IDF **v5.5.x** in `~/esp/esp-idf` (`install.sh esp32s3`, which includes esptool)
   - `apt install cmake ninja-build libsdl2-dev libcurl4-openssl-dev libfreetype-dev`
   - USB: **usbipd-win** on Windows → `usbipd attach --wsl` → `/dev/ttyACM0` (fallback: esptool on Windows)
2. **Factory backup, before the first flash:** `esptool.py read_flash 0 0x1000000 waveshare-factory.bin` (kept
   outside the repo), so the board can always be restored.
3. **Fork:** `gh repo fork mach1na/chroninkle --fork-name keepsake --clone` into `~/code/keepsake`.
4. **Baseline:**
   - build and flash the stock firmware; walk Chroninkle's user manual (record → tag → playback, lock, shutdown,
     `.txt` book; transcription only if a Gemini key is handy). Needs a FAT32 card.
   - also check how the stock firmware behaves with **no key**, since that's Keepsake's default
   - add `power_log` and record the power baseline (see above)
5. **Reading benchmark (30 min):** flash the CrossMux Waveshare 3.97 nightly, read an EPUB, note the UX bar, then
   reflash.
6. **Restructure into the layout in one dedicated commit** (pure `git mv`, so rename detection keeps upstream
   cherry-picks working). The next commit only fixes paths
   (`EXTRA_COMPONENT_DIRS components/core components/drivers components/services`; tools/web/CI/docs).
   Verify the firmware still builds and behaves like the baseline.
7. **Rebrand to Keepsake (user-facing only):**
   - README, boot splash + Keepsake logo, About page
   - hotspot default `Keepsake-XXXXXX`, mDNS host `keepsake`, portal title
   - attribution "Based on Chroninkle (mach1na) and Followup (ALXV Labs)" with GPLv3 notices kept
   - `CONFIG_CHRONINKLE_*` and symbol names stay as they are
8. **Agent docs:**
   - the root `CLAUDE.md` + `AGENTS.md`, and scoped CLAUDE.md files for `firmware/`, `firmware/main/`,
     `firmware/components/`, `sim/` and `web/`
   - drop Craig-specific process (mach1na tracker, `docs/specs/v2/`, `v2-dev`)
   - copy this plan to `docs/plan.md`
9. **`docs/guide.md`, a barebones guide for Eric** (replaces the 20 KB `docs/user-manual.md`). Skimmable bullets.
   - **Feature list** table: feature · status (✅ works / 🚧 in progress / 📋 planned / 💤 later + issue link) ·
     one-line description. Seeded with the inherited features (voice notes, Gemini transcription/summaries, todos,
     follow-ups, topics, TXT books, lock screen, USB drive mode) and everything in this plan.
   - **Controls cheat sheet:** rocker up/down/press, Record tap/hold, PWR tap/hold/6 s.
   - **How to:**
     - set up Wi-Fi (provisioning file or hotspot)
     - set a reminder
     - record a voice note (optional AI transcription)
     - add books over USB and over Wi-Fi
     - find a free book
     - read
     - set a timer
     - switch the lock screen to ambient
     - battery profiles
     - charge and power off
     - fix common problems
   - **For Eric (dev):** build, flash, monitor, tests, sim (if kept), regenerate assets/fonts, cut a release/OTA,
     read crash logs.
10. **Port the font generator** from CoreText to FreeType (Pillow); verify it reproduces the committed fonts.
11. **Host unit tests** (`tests/`, doctest via plain CMake) for `core/`, plus CI:
    - `firmware.yml`: `espressif/idf:v5.5` container build with `--strict-warnings`
    - `tests.yml`: unit tests + a short fuzz run
    - `web.yml`: lint/build
12. **Issue tracker (all work, not just later features):**
    - forks ship with Issues off: `gh repo edit ekhumbata/keepsake --enable-issues`
    - **labels:** `mvp`, `post-gift`, `later`, `enhancement`, `bug`, `upstream-backlog`,
      `area:{setup,power,sim,ui,books,ambient,reminders,gift,ai,sync}`
    - **milestones:** `M0 Setup`, `M1 Power`, `M2 Sim`, `M3 Design`, `M4 Books`, `M5 Ambient`, `M6 Gift`
      (gift-MVP, with due dates from the weekly schedule), `M7 Post-gift: Reminders & timers`,
      `M8 Post-gift: Polish` and `Later`
    - **one issue per plan step / scope-table row** (`gh issue create --milestone … --label …`). Each body has
      **Why / Approach / Depends on / Done when / Power impact**, with a link to the matching section of
      `docs/plan.md`.
    - Phase 6 rows go to `Later`; the inherited `docs/todo.md` items become `upstream-backlog` issues, and
      `docs/todo.md` becomes a pointer to the tracker
    - **workflow:** each issue gets its own branch `NN-short-name`, and the PR says `Closes #NN`; the root CLAUDE.md
      documents this
    - `docs/guide.md`'s feature list links each feature to its issue/milestone

## Phase 1 — Desktop simulator (`sim/`): optional, timeboxed

The device is the source of truth; every feature is verified on hardware regardless. If a check fails, stop at the
last working tier and record why in `docs/decisions/`.

| Tier | Timebox | Continue if… | Quit if… |
|---|---|---|---|
| 1a spike | 1 day | portable code compiles on host with ≤ a handful of shims | host FreeRTOS/IDF issues still unresolved → 1b only |
| 1b gallery | 2 days | screens render identically to the device (compare an on-device screenshot) | rendering diverges → drop the sim |
| 1c interactive | 1 week | main flows (navigate, record → mock transcribe, open a book) run | needs heavy fakes for more than ~5 services, or keeps diverging |
| upkeep | ongoing | sim fixes take < ~20% of feature time | costs more than flashing and testing → freeze or delete |

**Fallback:** a `screenshot` serial console command dumps the framebuffer as PNG/PBM (`tools/grab_screen.py`), so
design reviews use real-device images. The unit tests in `tests/` keep running either way.

- **1a. Host runtime spike:**
  - try ESP-IDF's `linux` target (`idf.py --preview set-target linux`) as a separate `sim/` IDF project whose
    `COMPONENT_DIRS` lists `firmware/components/{core,services}` + `sim/` overrides
  - fallback: plain CMake + the FreeRTOS-Kernel POSIX port + `sim/shim/`
- **1b. Screen gallery:** fixture JSON → page `State` → `Draw*Page()` → a 480×800 framebuffer in SDL. A headless
  `--screenshot` mode produces golden-image tests.
- **1c. Interactive sim:** run `main/` unchanged with drivers swapped:

| Real component | Sim replacement |
|---|---|
| `epaper_panel` | `SimPanel`, same API; realistic refresh delays (partial 0.6 s, fast 2.8 s, full 3.5 s), optional flash/ghosting, partial counter |
| `board`, `axp2101` | fake PMIC: battery %, USB toggle, PWR presses |
| buttons | ↑/↓ rocker, Enter = rocker press, Space tap/hold = Record, P / Shift+P = PWR tap/hold |
| `sd_card` | host dir `sim/sdcard/` |
| `pcf85063`, `qmi8658`, SHTC3 | sim clock with fast-forward, fake motion, fake temp/humidity |
| `audio_hal` | null sink or SDL audio; canned WAV or SDL mic |
| Wi-Fi, Gemini, catalogs, weather | mocks (canned JSON) + optional real-HTTP mode via libcurl |

Debug keys: T clock, B battery, U USB, L Wi-Fi, F force alarm. Not simulated: power draw, SPI timing, I2S, real
Wi-Fi.

## Phase 1.5 — Visual identity: cute, kawaii-inspired design system

**Direction:** soft, cozy and playful in the spirit of kawaii, but **entirely original**. Built before the feature
phases so every new screen uses it, and reviewed as images (sim gallery or device screenshots) before hardware.

**Constraints:** 1-bit B/W (4-gray later, issue #3), 480×800 portrait, slow refresh, so no animation loops and no
color. Cuteness comes from shape, line, pattern, character and tone.

1. **Two original mascots,** 1-bit pixel-art sprite sets (48/96/160 px), drawn in Aseprite/Piskel → PNG →
   `assets/epaper_assets.json` → `tools/generate_epaper_project_assets.py`.
   - **Bookworm** (round, smiley, big glasses, peeking out of books) on **book pages**:
     - library, reader (corner while a page loads, chapter-end cheer)
     - Find-a-book (thinking), downloads, the USB drive screen
     - empty shelf, and the DRM/unsupported-file message (oops)
   - **Mochi cloud** (squishy cloud with tiny arms) on **ambient screens**:
     - the ambient/lock screen and weather moods (sun, rain, snow, wind, fog)
     - sleep/shutdown/night mode (sleepy), charging, low battery
     - reminders (holding a bell)
   - **Defaults for the rest** (adjustable at design review): voice notes use Mochi; onboarding introduces both.
     Names are chosen by Eric or by the friend during onboarding.
2. **Typography:** a rounded UI font (M PLUS Rounded 1c / Nunito / Quicksand, OFL) replaces Inter; Literata (OFL)
   for book text.
3. **Shapes** (new `design_tokens`):
   - large radii, 3 px rounded strokes, pill buttons and tags, speech-bubble toasts
   - hearts/stars/sparkles/bows, scalloped dividers
   - sparse patterns (polka dot, gingham, tiny stars), each tested for partial-refresh banding
4. **Icons:** a chunky, rounded hand-drawn set (home, notes, todos, books, reminders, timer, weather, settings,
   battery, Wi-Fi).
5. **Voice & copy:** short and warm, with ♡ ✿ ☆ in the font charset; all copy in one strings file.
6. **Personal touches:** greeting by the friend's name, a birthday screen + jingle, and a one-time "Hello from Eric"
   card. **Stored in NVS/SD config, set via the portal, never in the repo.**
7. **Sounds:** soft original or CC0 chimes (startup, saved, reminder, low battery), 16 kHz mono MP3.
8. **Books-first Home screen:**
   - **Continue reading** (cover + %), **Library**, **Add books**, **Reminders** (post-gift) and **Settings**
   - Notes, Todos, Follow-ups and Topics fold into one **Notes** entry lower down, keeping the reader simple
   - the Record button still captures a note from anywhere
9. **Restyle the existing screens**, in priority order:
   - MVP: Home, lock, library/reader, toasts, modals, footer, keyboard, onboarding, settings
   - post-gift: notes/todos/follow-ups/topics, details
10. **Style guide:** tokens, fonts, icons, mascot moods, patterns and every screen. A design review before Phase 2.
11. **Web portal + Library web app** in the same style (they can use pastel color on the phone).

Eric's input needed: mascot names, final UI font, the friend's name and birthday, pixel art (draw or commission).
Placeholder sprites until then.

## Phase 2 — Books & library (priority feature)

**Goal:** the friend gets their **existing DRM-free books** onto the device by drag-and-drop from a computer, finds
free books without a computer, and reads comfortably. **PDF is deferred** (issue #1).

| Format | Handling | Notes |
|---|---|---|
| EPUB 2/3 | native, reflowable (`epub_engine`) | DRM = `rights.xml` / Adobe `encryption.xml` → friendly message; IDPF font obfuscation alone is fine |
| TXT, MD | native (existing reader) | encoding detection: UTF-8 / UTF-16 BOM / Latin-1 |
| HTML (single file) | native, EPUB XHTML block parser | |
| FB2 | native, XML → block model | low effort |
| MOBI / AZW3 (DRM-free) | native: PalmDB + PalmDOC LZ77 / KF8 HTML → block model | Kindle Store files are DRM'd → friendly message |
| CBZ | native: JPEG/PNG decode (esp_jpeg/tjpgd, pngle), scale, dither | CBR → issue #10 |
| PDF | "PDFs aren't supported yet" message | issue #1 |

**2a. EPUB engine.**
- **Timebox: 3 days** to port CrossPoint `lib/Epub` (MIT, keep its license) from Arduino types to stdio, as the
  portable `core/epub_engine`.
- **Plan B** if the port stalls: a minimal own parser (miniz/`tinfl` + `expat` from the IDF registry) covering spine,
  TOC and basic XHTML.
- **Scope:**
  - `container.xml` → OPF metadata/spine
  - TOC from NCX or EPUB3 nav
  - XHTML → blocks (paragraphs, headings, bold/italic, breaks, lists); images become placeholders (issue #4)
  - cache sections + pagination under `/Books/.cache/` (keyed by book hash + font size)
- **Robustness:**
  - parse in a dedicated worker task with a generous PSRAM stack (Chroninkle's SD call chain has already overflowed
    a 4 KB stack once)
  - size and nesting limits; corrupt files give a friendly error, never a reboot
  - parsers fuzzed on the host (`tests/`)

**2b. Library service.** `text_reader_service` → `library_service`:
- scans `/Books/` **recursively** (FAT is case-insensitive, so Chroninkle's `books/` is the same folder)
- `OpenBook(path) → BookReader` per format, sharing one block/page model (PDF slots in later)
- metadata: title, author, dithered cover thumbnail
- progress (spine + offset, %); migrate the `.pos` sidecars
- last-opened sort, delete

**2c. Reader UI.**
- Literata regular/italic/bold at 3 sizes; Latin-1 + typographic punctuation.
- **Controls:**
  - rocker down/up = next/prev page (hold = skip chapter)
  - rocker press = menu (contents, font size, bookmark, go to %, back)
  - Record tap = back to the library
- **Footer:** chapter · page in chapter · %.
- Partial refresh per page turn; full refresh every N pages (default 10) via `NeedsGhostingFlush()`.
- **Library list:** cover, title, author, progress; "Continue reading" on top.
- Lock/ambient screen option: "Reading: *Title* · 42%".

**2d. Book sources (legal only).** `book_catalog_service` with `Search` / `Download` (streamed to SD, progress, HTTPS
with the cert bundle, through `net_jobs`):
1. **Project Gutenberg via Gutendex.** User-initiated single downloads only (Gutenberg disallows bulk scraping);
   cache search results.
2. **Standard Ebooks.** Direct EPUB downloads (check whether its OPDS feed now needs a Patrons Circle membership).
3. **Generic OPDS.** User-added catalogs (e.g. Eric's Calibre/calibre-web/Kavita).
4. **Send from phone/computer.** Upload over Wi-Fi.

**2e. USB drag-and-drop** (builds on the existing OTG mode; holds a PM lock while active):
- **When plugged in:** VBUS appears → card "Connected to a computer? Open as a USB drive" → Yes / Just charging
  (auto-dismiss after 15 s; remembers "Just charging" for the session).
- **While active:** a full-screen "Keepsake is a USB drive: drag books into **Books**, then eject", with Bookworm.
- **On Done or unplug:** exit, remount, rescan, toast "3 new books added". Document "eject first".
- **Setup:** pre-create `Books/` with a `README.txt` listing the formats; files at the card root are found and
  offered a move.
- **Dev note:** TinyUSB takes the USB PHY from the serial console while this mode is on.

**2f. Wi-Fi search & add-books.**
- **Phone/computer (primary, MVP):** a "Library" web app at `http://keepsake.local`, served **only while the
  on-device "Add books" screen is open**. Tabs: Search (Gutendex) → "Send to device"; Upload (drag-drop from a phone
  or computer, so the friend doesn't need a computer or USB); Manage. The screen shows the URL + a QR code (scan →
  open). Built in `web/`, reusing the portal's HTTP server in STA mode + mDNS.
- **On device, typed (MVP):** Books → "Find a book" with the existing `keyboard` widget → results → download → open.
- **On device, voice (post-gift, AI):** hold Record and say a title → transcription → the same search. Needs a
  Gemini key.

**2g. Tests:**
- a test shelf covering every format: EPUB2/3, TXT UTF-8 + Latin-1, FB2, MOBI/AZW3, CBZ, a fake DRM EPUB, a PDF, and
  one deliberately corrupt EPUB
- host unit tests for the parsers + pagination
- golden images: library, reader at each font size, menu, DRM message, search results

## Phase 3 — Ambient dashboard

1. **Hardware spike (schematic first):** SHTC3 wiring/rail; whether PCF85063 INT (GPIO45) can power the PMIC on from
   full-off; whether the SD rail can be gated.
   - GPIO45 is not an RTC-domain GPIO on the ESP32-S3 (only 0–21), so it wakes light sleep only; deep sleep would
     use timer wake.
   - Schematic: files.waveshare.com/wiki/ESP32-S3-ePaper-3.97/ESP32-S3_e-Paper-3.97-schematic.pdf
   - Reference: Waveshare's `08_ESP32-S3_e-Paper-3.97` demo.
2. **Climate:** `drivers/shtc3` (board-agnostic) + `climate_service`.
3. **Weather:** `weather_service` with Open-Meteo (no key), fetched **hourly via `net_jobs`**; location from
   `timezone_service` or the portal; NVS cache with a "stale" marker.
4. **UI:** the ambient lock-screen variant with Mochi:
   - big clock, date + mini month calendar
   - weather + hi/lo, indoor °C/%RH
   - top todos, next reminder, current book

   Settings: lock screen Todos/Ambient, °C/°F, 12/24 h.
5. **Cadence:** fixed at 5 min in the MVP; post-gift it follows the battery profile (1 / 5 / 15 min). Partial
   refresh, light sleep with timer wake in between.
   **Night mode** (default 23:00–07:00): no updates; a sleeping Mochi + tomorrow's first todo/reminder, then a fresh
   full refresh in the morning.
6. **Portal:** location, units, night hours.
7. **Measure** drain per cadence on hardware; pick the default.

## Phase 4 — Reminders & timers

1. **`pcf85063` driver:** add `SetAlarm`, `EnableAlarmInterrupt` and the countdown timer; GPIO45 interrupt in
   `power_service` (or a new `alarm_service`).
2. **`reminder_service`:**
   - stores `/sdcard/reminders.json` (id, due_at local ISO-8601, text, source recording, state)
   - always arms the RTC + timer wake for the next due reminder; re-arms on boot/time/TZ change
   - the scheduling maths lives in the portable `core/reminder_rules` and is unit-tested (DST, midnight, TZ change)
3. **Picker entry (primary, no AI):** Reminders → New → pick a quick preset ("In 1 h", "Tonight 8 pm", "Tomorrow
   9 am") or a date/time with `time_input`, then choose a label from a short preset list (Take medicine, Call
   someone, Appointment, Water plants…) or record a short voice memo (saved as audio, played when it fires). Edit
   reuses the same screens.
4. **Voice + AI entry (lowest priority, needs a Gemini key):**
   - a **Reminder** tag in the post-recording menu
   - Gemini structured output `{due_at, text, confidence}` given the local time + TZ, validated (not in the past,
     parseable)
   - confirm card "Tomorrow 15:00: call the dentist ♡" → Save / Edit time / Discard
5. **Firing:** wake → modal with Mochi + bell, looping `alarm.mp3`; Done / Snooze 10 min; auto-stop after N min, then
   the reminder stays flagged on the lock screen.
6. **Timers:** Focus/Pomodoro page (5/15/25/50 min), refreshed once per minute; the end uses the same firing path.
7. **Reminders list** (upcoming/past, delete); the next reminder shows on the lock/ambient screen.
8. **Power interaction:** if the RTC can't power the board on from full-off, shutdown warns "Reminders won't ring
   while off", and auto-shutdown (issue #16) must not shut down while reminders are pending.

## Phase 5 — Gift readiness

- **Before handing it over (needs USB; can't be fixed by OTA later):**
  - add a **coredump partition** (64 KB, shrinking the OTA slots slightly) and finalize `partitions.csv`
  - make a full serial flash of the release build
  - confirm the factory backup is safe
- **OTA updates:**
  - pull-based from GitHub Releases (on Wi-Fi connect, at most daily via `net_jobs`, or Settings → "Check for
    updates"), over HTTPS with the cert bundle and a version check
  - the new image is marked valid only after a health check (boot reaches Home, SD mounts, display works);
    otherwise the existing rollback reverts it
- **Crash diagnostics:**
  - coredump + reset reason + the last log lines are written to SD `/.keepsake/crash/` on the next boot
  - Settings → About shows "Something went wrong N times"
  - Eric collects them in USB drive mode
- **No-key mode (default):**
  - without a Gemini key, recordings save as playable audio notes
  - no "waiting to transcribe" badge, no AI wording in onboarding or settings
  - nothing looks broken
- **Gemini key (optional):** if added, a dedicated Google account/project, free tier, **no billing**, the key
  **restricted to the Generative Language API**, loaded via the provisioning file into NVS (never committed). Tell
  the friend, in plain words, about free-tier data use.
- **Provisioning file (pre-loads the friend's setup; nothing personal in the repo):**
  - the repo ships `provision.example.json`; Eric fills in a private copy outside the repo and puts it on the SD
    card as `/.keepsake/provision.json`
  - **fields:** `wifi` (a list of `{ssid, password}`), `timezone`, `weather_location`, `friend_name`, `birthday`,
    `welcome_message`, `gemini_key`
  - on boot a `provisioning_service` validates it, imports it into NVS (Wi-Fi, timezone, Gemini key and personal
    settings), then **overwrites and deletes the file**, so no plaintext secrets stay on the card. A toast confirms
    "All set up ♡".
  - the same mechanism re-provisions later (drop in a new file over USB)
- **Multiple saved networks:**
  - extend `wifi_service` from a single `ssid`/`password` to a list of up to 5 networks (friend's home, Eric's home
    for testing/burn-in, a phone hotspot)
  - connect to the strongest known network in the scan; migrate the existing single entry
  - Settings → Wi-Fi lists and removes saved networks; the portal adds them
- **Wi-Fi reality check:** the ESP32-S3 is **2.4 GHz only**, so confirm the friend's router has a 2.4 GHz network
  (WPA2/WPA3-personal; enterprise/captive-portal networks won't work). Test the setup portal on an iPhone and an
  Android phone.
- **Out-of-box flow:**
  - **with a provisioning file** (the plan): first boot imports it, connects to their Wi-Fi when in range, and shows
    the welcome card; no setup steps for the friend
  - **without it, or when the saved password fails** (e.g. a changed password): an automatic, friendly "Let's
    reconnect" screen with Mochi + the `Keepsake-XXXXXX` hotspot setup flow
  - API keys never appear unless the key fails
- **Offline behavior (everything core works without internet):**
  - **works offline:** reading, library, USB book transfer, voice recording + playback, clock/calendar (RTC),
    indoor climate, night mode, existing todos, reminders/timers once set
  - **waits for Wi-Fi via `net_jobs`:** transcription and voice-reminder parsing (only when a Gemini key is set; the
    time picker works offline), Gutenberg search/download (friendly "needs Wi-Fi" message), weather
    (shows "updated X h ago", hidden after ~6 h), OTA, NTP (the RTC holds time between syncs)
  - **no known network in range:** `net_jobs` backs off (rescans at most every 30–60 min, or at once when the
    friend triggers something that needs Wi-Fi), so the battery isn't drained by constant scanning
  - the UI never blocks on the network
- **Plain-language errors** for downloads, weather and OTA; transient failures (5xx/timeouts) stay queued in
  `net_jobs`. (The "N notes waiting to transcribe" badge is post-gift and only appears when a key is set.)
- **Starter content:** a preloaded starter shelf of public-domain favorites, a printed quick-start card, and a
  friend-facing version of `docs/guide.md`.
- **Enclosure:** the MakerWorld Folloup case; check the fit of the speaker, battery and SD access.
- **Burn-in:** a week of daily use on battery; review `power_log` and crash logs before gifting.

## Phase 6 — Later features (GitHub issues, milestone "Later")

| # | Issue | Labels | Design notes for the body |
|---|---|---|---|
| 1 | PDF support via the Keepsake converter | `area:books` | Static TS app (pdf.js, Apache-2.0) served from SD at keepsake.local + GitHub Pages. "Text" mode → generated EPUB (JSZip); "Pages" mode → device-resolution 1-bit page-pack (evaluate CrossPoint XTC/XTCH vs `.kspages`), auto-crop, halves, landscape. Raw PDFs over USB → "Needs preparing" + QR. Another `BookReader`. |
| 2 | Experimental on-device PDF text extraction | `area:books` | xref + inflate + Tj/TJ + ToUnicode → reflow. Depends on #1. |
| 3 | 4-level grayscale antialiased text | `area:books`,`area:ui` | 2-bpp framebuffer path + 4-gray waveform; measure speed/ghosting/power. |
| 4 | Inline images in EPUB | `area:books` | Reuse the CBZ decode/dither; PSRAM budget. |
| 5 | Justified text + hyphenation | `area:books` | Liang patterns (en) on SD. |
| 6 | Dictionary lookup | `area:books` | StarDict dict on SD; rocker word selection. |
| 7 | KOReader progress sync | `area:books`,`area:sync` | KOReader sync protocol. |
| 8 | Calibre wireless device connection | `area:books` | Calibre smart-device protocol (as in CrossPoint/CrossMux). |
| 9 | Book cover as sleep screen | `area:books`,`area:ui` | Dithered cover on lock/shutdown. |
| 10 | CBR (RAR) comics | `area:books` | Only if a small unrar fits. |
| 11 | Notes/todos off-device sync | `area:sync` | Pluggable `sync_service`: webhook → Todoist / Notion / Git-markdown. |
| 12 | Alternative AI providers | `area:ai` | Provider interface for transcription/summary: Claude, OpenAI/Whisper, self-hosted Whisper. |
| 13 | Calendar (ICS) events on ambient | `area:ambient` | A private ICS URL; today's next events. |
| 14 | Recurring reminders | `area:reminders` | Rules in `core/reminder_rules`; AI parses "every Monday". |
| 15 | Power: SD rail gating + deep-sleep ambient | `area:power` | Use the schematic findings; deep sleep with timer wake for ambient. |
| 16 | Auto-shutdown instead of light sleep | `upstream-backlog`,`area:power` | Battery only; USB keeps light sleep; blocked while reminders are pending. |
| 17 | Deduplicate `FitLabelText` | `upstream-backlog`,`area:ui` | From Chroninkle `docs/todo.md`. |
| 18 | Derive TimelineEntry icon/checked state at render time | `upstream-backlog`,`area:ui` | From Chroninkle `docs/todo.md`. |

## Verification

- **Phase 0:**
  - factory backup exists
  - stock firmware builds and flashes from WSL; the manual checklist passes
  - after the restructure, the build behaves like the baseline
  - the font port reproduces the committed fonts
  - CI green (firmware, unit tests, web)
- **Unit tests** (every PR): parsers, pagination, reminder rules, MOBI decompression; a short fuzz run in CI with
  no crashes.
- **Power:**
  - baseline in `docs/power.md`
  - after the foundations: lower idle drain, with page-turn and button latency unchanged
  - Wi-Fi on-time matches the jobs run
  - a 48 h battery `power_log` run at the end of each feature phase, with days per profile recorded
- **Sim (if kept):** golden images pass; the main flows run from the keyboard; go/no-go recorded per tier.
- **Books:**
  - USB drag-and-drop on Windows + macOS (eject and plain unplug): the toast reports the new books, all formats
    open, and DRM, PDF and corrupt files show friendly messages
  - TOC jumps, progress survives a reboot, reopening is < 1 s from cache
  - Gutenberg search (phone Library app and on-device keyboard) downloads a known title
  - with **no Gemini key**, recording a note saves playable audio, and no AI wording or errors appear
  - the phone Library app works on the LAN and stops when the screen closes
  - ghosting stays acceptable over 100 page turns
- **Ambient:** SHTC3 matches a reference thermometer; weather fetch + stale handling; night mode; overnight drain.
- **Reminders (post-gift OTA):**
  - created with the picker (presets, custom time, label or voice memo); voice + AI only once that's built and a key
    is set
  - fire from the lock screen and from light sleep
  - survive a reboot and DST/TZ changes
  - full-off behavior matches the spike and the warning
- **Gift readiness:**
  - an OTA update installs from GitHub Releases, and a deliberately broken OTA rolls back
  - **rehearse the first post-gift update** (a dummy version bump) end to end before handing it over
  - a forced crash leaves a report on SD
  - fresh-device setup completed on an iPhone and an Android phone with a 2.4 GHz network
  - the provisioning file imports correctly and is gone from the SD card afterwards; a bad Wi-Fi password leads to
    the "Let's reconnect" flow
  - **offline day test:** a full day with Wi-Fi off. Reading, recording, the clock and the ambient screen all work;
    weather shows "updated X h ago" and recovers when Wi-Fi returns; the drain shows no scanning spikes in
    `power_log`
- **Every feature PR:** `--strict-warnings` build, Power impact note, `version.txt` + `CHANGELOG.md`, and
  `docs/guide.md` updated.
