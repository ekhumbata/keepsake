# Keepsake guide

The short version: what Keepsake does, how to use it, and how to work on it. For every screen in detail, see the
[user manual](user-manual.md) (inherited from Chroninkle; this guide replaces it over time). The plan of record is
[plan.md](plan.md).

**Status key:** ✅ works · 🚧 in progress · 📋 planned for the gift · 🎁 planned for after the gift (over-the-air
update) · 💤 later

## Features

### Books

| Feature | Status | What it is |
|---|---|---|
| Plain-text books (`.txt`) | ✅ | Reads `.txt` files from the SD card's `books` folder (UTF-8, UTF-16 or Latin-1, detected automatically) and remembers your page |
| EPUB books | 🚧 [#26](https://github.com/ekhumbata/keepsake/issues/26), [#28](https://github.com/ekhumbata/keepsake/issues/28) | Real e-books with chapters, headings, bold, lists; pages across chapters with progress (to be checked on the device). Pictures still to come |
| Reader with a book font | 🚧 [#28](https://github.com/ekhumbata/keepsake/issues/28) | Literata at three text sizes, chapter + % footer and a menu with Contents, bookmarks and Go to %, and hold-to-skip-chapter are in |
| Library with covers | 🚧 [#27](https://github.com/ekhumbata/keepsake/issues/27) | Titles, progress and "reading now" first are in, subfolders too, with a Continue reading card showing the cover; covers on every book still to come |
| Friendly file messages | 🚧 [#29](https://github.com/ekhumbata/keepsake/issues/29) | A kind message for DRM-locked, PDF and damaged files, never a crash (shown as text in the reader today; the mascot card comes with the redesign) |
| Add books over USB | ✅ / 📋 [#30](https://github.com/ekhumbata/keepsake/issues/30) | USB drive mode works today from Settings → Storage; the plug-in prompt and "3 new books" toast are planned |
| Add books from a phone | 📋 [#32](https://github.com/ekhumbata/keepsake/issues/32) | Open "Add books", go to `keepsake.local` on the phone, upload or search |
| Free books (Project Gutenberg) | 📋 [#31](https://github.com/ekhumbata/keepsake/issues/31), [#33](https://github.com/ekhumbata/keepsake/issues/33) | Search and download public-domain books, from the phone or on the device |
| MOBI / AZW3 (DRM-free) | 📋 stretch [#34](https://github.com/ekhumbata/keepsake/issues/34) | Kindle-format files that aren't DRM-locked |
| FB2, HTML, comics (CBZ) | 🎁 [#59](https://github.com/ekhumbata/keepsake/issues/59) | More formats |
| Standard Ebooks, OPDS catalogs | 🎁 [#60](https://github.com/ekhumbata/keepsake/issues/60) | More free-book sources, and your own Calibre library |
| PDF | 💤 [#66](https://github.com/ekhumbata/keepsake/issues/66) | Via a converter on the phone/computer |

### Home, ambient and design

| Feature | Status | What it is |
|---|---|---|
| Lock screen with todos | ✅ | Date + your top pending todos while locked |
| Books-first Home screen | 📋 [#23](https://github.com/ekhumbata/keepsake/issues/23) | Continue reading, Library, Add books, Reminders, Settings; notes fold into one entry |
| Weather | 🚧 [#37](https://github.com/ekhumbata/keepsake/issues/37) | From Open-Meteo (no key), hourly, for the provisioning file's `weather_location` (a town, or "lat, lon"). Shows under Home's greeting ("8C, light rain"); offline it says "updated 2 h ago" and hides after 6 h (to be checked on the device) |
| Ambient lock screen | 📋 [#38](https://github.com/ekhumbata/keepsake/issues/38) | Big clock, date + month, weather, indoor temperature/humidity, night mode |
| Indoor climate | 📋 [#36](https://github.com/ekhumbata/keepsake/issues/36) | The board's SHTC3 temperature/humidity sensor |
| Weather | 📋 [#37](https://github.com/ekhumbata/keepsake/issues/37) | Open-Meteo (no account), hourly, "updated X h ago" when offline |
| Cute visual identity | 🚧 [#19](https://github.com/ekhumbata/keepsake/issues/19)–[#25](https://github.com/ekhumbata/keepsake/issues/25) | Rounded font (Nunito) and book font (Literata) are in; placeholder pixel Bookworm and Mochi mascots are in (`tools/draw_mascots.py`, final art to come); chunky icons, warm copy to come |
| Choose your own fonts | 🎁 [#104](https://github.com/ekhumbata/keepsake/issues/104) | Settings → Fonts: pick the interface and book fonts separately, from fonts copied onto the SD card (Quicksand included) |
| First-run introduction | 🚧 [#86](https://github.com/ekhumbata/keepsake/issues/86) | A hello by name, then Bookworm, Mochi, the buttons, power and Wi-Fi. Without Wi-Fi it offers the phone setup ("Let's get online"); the welcome note follows with both mascots (to be checked on the device and on an iPhone and an Android phone) |
| Personal touches | 🚧 [#85](https://github.com/ekhumbata/keepsake/issues/85) | Home greets by name; on special days (birthday, anniversary, first date, your own days, Christmas, New Year, Valentine's, Halloween) it says so and shows a card with a mascot once that day; the welcome message shows once. Values come from the provisioning file, never the repo. A birthday jingle is still to come |
| Keepsake branding | 🚧 [#9](https://github.com/ekhumbata/keepsake/issues/9) | Boot screen, About page, `Keepsake-XXXXXX` hotspot |

### Reminders, timers and notes

| Feature | Status | What it is |
|---|---|---|
| Voice notes | ✅ | Hold Record, speak, tag it; saved on the SD card as audio you can play back |
| Todos, follow-ups, topics | ✅ | Organize notes; pending todos show on the lock screen |
| Transcription + summaries (Gemini) | ✅ optional | Only with a Gemini key; without one, notes stay as playable audio |
| No-key polish | 🚧 [#44](https://github.com/ekhumbata/keepsake/issues/44) | Without a key: voice notes with Play, no AI wording, no "waiting" badges (to be checked on the device) |
| Reminders (time picker) | 🎁 [#50](https://github.com/ekhumbata/keepsake/issues/50)–[#52](https://github.com/ekhumbata/keepsake/issues/52) | Presets or a time, a label or a short voice memo, rings even from sleep |
| Focus timers | 🎁 [#53](https://github.com/ekhumbata/keepsake/issues/53) | 5/15/25/50 minutes |
| Voice reminders (AI) | 🎁 [#54](https://github.com/ekhumbata/keepsake/issues/54) | "Remind me tomorrow at 3 to call the dentist" (needs a key) |
| Recurring reminders | 💤 [#79](https://github.com/ekhumbata/keepsake/issues/79) | "Every Monday" |

### Power, setup and updates

| Feature | Status | What it is |
|---|---|---|
| Auto-sleep | ✅ | Screen rests after ~3 min, deep sleep after ~30 min |
| Power management | 🚧 [#16](https://github.com/ekhumbata/keepsake/issues/16) | Slower CPU while idle (frequency scaling is in; to be measured on the device) |
| Wi-Fi only when needed | 🚧 [#17](https://github.com/ekhumbata/keepsake/issues/17) | Radio off by default; jobs batched; a new recording brings it up to transcribe straight away. Built, behind a menuconfig switch until it's checked on the board |
| Charging screen + low-battery nudges | 🚧 [#84](https://github.com/ekhumbata/keepsake/issues/84) | A Charging card with Mochi on plug-in (closes by itself); a gentle note at 20% and a sleepy-Mochi card at 10% and 5%; a smoothed battery percentage (to be checked on the device) |
| Battery saver + profiles | 🎁 [#58](https://github.com/ekhumbata/keepsake/issues/58) | Always fresh / Balanced / Max battery |
| Wi-Fi setup hotspot | ✅ | `Keepsake-XXXXXX` hotspot + setup page |
| Provisioning file | 🚧 [#42](https://github.com/ekhumbata/keepsake/issues/42) | Drop a file on the SD card and it sets everything up, then deletes itself (to be checked on the device) |
| Several saved networks | 🚧 [#43](https://github.com/ekhumbata/keepsake/issues/43) | Up to 5, most recent first, moving on when one stops answering (to be checked on the device); a "Let's reconnect" card offers the setup hotspot when the password is rejected; Settings → Saved Wi-Fi lists them and forgets one |
| Over-the-air updates | 🚧 [#40](https://github.com/ekhumbata/keepsake/issues/40) | Checks the latest GitHub release once a day (or Settings → About → Check for updates) and downloads a newer one in the background. It starts the next time the device sleeps. If it fails its health check it rolls back and isn't offered again. A "Keepsake updated" card says what's new (to be checked on the device) |
| Crash reports | 🚧 [#39](https://github.com/ekhumbata/keepsake/issues/39), [#41](https://github.com/ekhumbata/keepsake/issues/41) | A crash leaves a report and core dump on the SD card for Eric (to be checked on the device) |
| Works offline | 📋 [#45](https://github.com/ekhumbata/keepsake/issues/45) | Everything but downloads, weather and AI works without Wi-Fi |

## Controls

There's no touchscreen: a rocker on the side, the **Record** button and the **PWR** button.

| Control | What it does |
|---|---|
| Rocker up / down | Move the selection (hold to keep moving); in the reader, previous / next page |
| Rocker press | Select / confirm |
| Record, tap | Back |
| Record, hold | Record a voice note (stops when you let go) |
| PWR, tap | Lock / unlock |
| PWR, hold ~1 s | Shut down (asks first) |
| PWR, hold 6 s | Hard power-off, whatever is on screen |

## How to

### Set up Wi-Fi
- **Today:** with no saved Wi-Fi, Keepsake opens a hotspot named `Keepsake-XXXXXX` (no password). Join it from a phone;
  the setup page opens by itself (or go to `http://192.168.4.1`). Pick your network and type the password. The time
  zone and an optional Gemini key can be set there too.
- Keepsake only works with **2.4 GHz** Wi-Fi (most routers have one). Work or hotel networks with a sign-in page won't
  work.
- **Or with a provisioning file** ([#42](https://github.com/ekhumbata/keepsake/issues/42)), which is how the gift is set up:
  see [Pre-load the setup](#pre-load-the-setup-provisioning-file).

### Pre-load the setup (provisioning file)
1. Copy [`provision.example.json`](../provision.example.json) **somewhere outside the repo** and fill it in: Wi-Fi
   networks, `timezone` (e.g. `Europe/London`), and optionally the friend's name, birthday (`MM-DD`), anniversary and
   first date (`MM-DD` or `YYYY-MM-DD`; with the year it counts them), which `holidays` to celebrate, your own
   `special_days` (date, title, message), a welcome message, a weather location and a Gemini key. Delete the lines
   you don't need.
2. Put it on the SD card as `.keepsake/provision.json` (the `.keepsake` folder is hidden on some computers).
3. Start Keepsake. It imports the file before Wi-Fi starts, shows **All set up!**, then **overwrites and deletes the
   file** so no passwords stay on the card.
4. If something in it is wrong, nothing is imported: Keepsake shows **The setup file has a problem** and writes
   `.keepsake/provision-problems.txt` next to it, listing each problem. Fix the file and restart.
- Never commit a filled-in copy (`provision.json` is in `.gitignore`).

### Add books over USB
1. Plug Keepsake into a computer with a USB-C data cable.
2. On Keepsake: **Settings → Storage → Enable OTG**. The SD card appears on the computer as a drive.
3. Copy `.epub` or `.txt` books into the `books` folder.
4. **Eject the drive on the computer first**, then on Keepsake choose **Disable OTG mode**.

### Add books from a phone
📋 Planned ([#32](https://github.com/ekhumbata/keepsake/issues/32)): open **Add books** on Keepsake, scan the QR code
(or go to `keepsake.local`), then upload a file or search for a free book.

### Find a free book
📋 Planned ([#31](https://github.com/ekhumbata/keepsake/issues/31), [#33](https://github.com/ekhumbata/keepsake/issues/33)):
**Find a book** on the device or the **Search** tab on the phone, then pick a result to download it.

### Read
- **Home → Books**, pick a book, then **Continue reading** or **Start from the beginning**.
- Rocker down = next page, up = previous page. Your page is remembered.
- Press the rocker in for the menu: **Contents** (jump to a chapter) or **Back to books**. Tap Record also goes back.

### Record a voice note
1. Hold **Record**, wait for the start sound, speak (up to 10 s), let go.
2. Listen to the playback, then tag it **Note**, **To-do**, **Idea** or **Topic** (or **Discard**).
3. With a Gemini key and Wi-Fi it gets transcribed; without one it stays as audio you can play from its details page.

### Set a reminder or timer
🎁 After the gift ([#52](https://github.com/ekhumbata/keepsake/issues/52),
[#53](https://github.com/ekhumbata/keepsake/issues/53)).

### Switch the lock screen to ambient
📋 Planned ([#38](https://github.com/ekhumbata/keepsake/issues/38)): Settings → lock screen → Ambient.

### Battery
- Short PWR press locks and puts the screen to sleep right away; that's the easiest way to save battery.
- 📋 Battery profiles come after the gift ([#58](https://github.com/ekhumbata/keepsake/issues/58)).

### Charge and power off
- Charge with USB-C: a Charging card with Mochi appears, and the status bar shows the battery and a charging mark.
- Keepsake nudges at 20%, 10% and 5% battery, once each.
- Hold PWR ~1 s → **Shut down**. On USB power it stays on until unplugged.

### Fix common problems
| Problem | Try this |
|---|---|
| Frozen or not responding | Hold PWR for 6 s, then press PWR to start it again |
| The SD card is missing on Keepsake | Settings → Storage → **Disable OTG mode** (USB drive mode keeps the card for the computer) |
| Wi-Fi won't connect | Make sure the network is 2.4 GHz. In Settings → Network, pick it again and re-enter the password, or turn on **Access Point** and use the setup page |
| A book doesn't show up | It must be an `.epub` or `.txt` file inside `books` (subfolders up to 4 deep are fine), and its name mustn't start with a dot |
| Notes aren't transcribed | Expected without a Gemini key; they're saved as audio |

## For Eric (development)

Full commands and conventions live in [`CLAUDE.md`](../CLAUDE.md); this is the cheat sheet.

| Task | Command / where |
|---|---|
| Build | `cd firmware && idf.py build` (ESP-IDF v5.5; release builds: see CLAUDE.md "Local test builds vs. release builds") |
| Flash + monitor | `cd firmware && idf.py -p <PORT> -b 460800 flash monitor` |
| Host unit tests | `cmake -S tests -B build/tests -G Ninja && cmake --build build/tests && ctest --test-dir build/tests` ([tests/README.md](../tests/README.md)) |
| CI | every PR builds the firmware (fails on warnings), runs tests + fuzzers and checks the web portal; firmware images are attached to the run |
| Regenerate images/logos/icons | `pip install -r tools/requirements.txt && python3 tools/generate_epaper_project_assets.py` ([asset-generation.md](asset-generation.md)) |
| Regenerate fonts | `python3 tools/generate_epaper_fonts.py --output … <font.ttf:symbol:size>…` (command in [asset-generation.md](asset-generation.md)) |
| Setup portal (web) | `cd web && npm ci && npm run build`, then copy `dist/index.*` to `firmware/components/services/wifi_service/portal/` |
| Cut a release / OTA | Bump `firmware/version.txt` (and CHANGELOG), write one friendly line in `firmware/whats_new.txt` (the "Keepsake updated" card), merge, then `git tag vX.Y.Z && git push origin vX.Y.Z`. The `release` workflow builds and publishes the images and `latest.json`, and devices pick it up within a day. `vX.Y.Z-rc1` makes a prerelease that devices ignore. Rehearse on a board with `run_board_tests.py --ota` ([#40](https://github.com/ekhumbata/keepsake/issues/40)) |
| Preload photos (#136) | `python3 tools/photos/prepare_photos.py <your photos folder> <SD card>/photos` (needs Pillow; `pillow-heif` for iPhone HEIC). It turns them upright, makes them 960 px baseline JPEGs, and names them "2019-05-20 First date.jpg" (date from the name or the photo, title from the name). Keepsake shows them oldest first with that caption; rename to change either. **Never commit them** (the repo is public; the tool refuses to write inside it) |
| Read crash logs | After a crash, the next boot writes `/.keepsake/crash/crash-NNN.txt` on the SD card (reset reason, crashed task, backtrace, the last ~8 KB of log) and `crash-NNN.elf` (the core dump). Copy them off over USB; decode with `idf.py coredump-info -c crash-NNN.elf` against the matching build. The last 10 are kept ([#41](https://github.com/ekhumbata/keepsake/issues/41)) |
| Test a plugged-in board | `python3 tools/board/run_board_tests.py` (self-test, screenshots, UI tour; `--flash`, `--interactive`, `--destructive`): [board-tests.md](board-tests.md) |
| What's next on the board | issues labelled [`hardware`](https://github.com/ekhumbata/keepsake/issues?q=is%3Aopen+label%3Ahardware) and PRs labelled `needs-device-test` |
