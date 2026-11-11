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
| Plain-text books (`.txt`) | ✅ | Reads `.txt` files from the SD card's `books` folder and remembers your page |
| EPUB books | 📋 [#26](https://github.com/ekhumbata/keepsake/issues/26) | Real e-books with chapters, bold/italic and a contents page |
| Reader with a book font | 📋 [#28](https://github.com/ekhumbata/keepsake/issues/28) | Literata in three sizes, chapter + % footer, menu (contents, size, bookmark, go to %) |
| Library with covers | 📋 [#27](https://github.com/ekhumbata/keepsake/issues/27) | Every book in `books` (including subfolders), cover, author, progress, "Continue reading" on top |
| Friendly file messages | 📋 [#29](https://github.com/ekhumbata/keepsake/issues/29) | A kind message for DRM-locked, PDF and damaged files, never a crash |
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
| Ambient lock screen | 📋 [#38](https://github.com/ekhumbata/keepsake/issues/38) | Big clock, date + month, weather, indoor temperature/humidity, night mode |
| Indoor climate | 📋 [#36](https://github.com/ekhumbata/keepsake/issues/36) | The board's SHTC3 temperature/humidity sensor |
| Weather | 📋 [#37](https://github.com/ekhumbata/keepsake/issues/37) | Open-Meteo (no account), hourly, "updated X h ago" when offline |
| Cute visual identity | 📋 [#19](https://github.com/ekhumbata/keepsake/issues/19)–[#25](https://github.com/ekhumbata/keepsake/issues/25) | Rounded font, Bookworm and Mochi mascots, chunky icons, warm copy |
| Personal touches | 📋 [#85](https://github.com/ekhumbata/keepsake/issues/85) | Greeting by name, birthday screen, a welcome card (kept off the repo) |
| Keepsake branding | 🚧 [#9](https://github.com/ekhumbata/keepsake/issues/9) | Boot screen, About page, `Keepsake-XXXXXX` hotspot |

### Reminders, timers and notes

| Feature | Status | What it is |
|---|---|---|
| Voice notes | ✅ | Hold Record, speak, tag it; saved on the SD card as audio you can play back |
| Todos, follow-ups, topics | ✅ | Organize notes; pending todos show on the lock screen |
| Transcription + summaries (Gemini) | ✅ optional | Only with a Gemini key; without one, notes stay as playable audio |
| No-key polish | 📋 [#44](https://github.com/ekhumbata/keepsake/issues/44) | No AI wording or "waiting" badges when there's no key |
| Reminders (time picker) | 🎁 [#50](https://github.com/ekhumbata/keepsake/issues/50)–[#52](https://github.com/ekhumbata/keepsake/issues/52) | Presets or a time, a label or a short voice memo, rings even from sleep |
| Focus timers | 🎁 [#53](https://github.com/ekhumbata/keepsake/issues/53) | 5/15/25/50 minutes |
| Voice reminders (AI) | 🎁 [#54](https://github.com/ekhumbata/keepsake/issues/54) | "Remind me tomorrow at 3 to call the dentist" (needs a key) |
| Recurring reminders | 💤 [#79](https://github.com/ekhumbata/keepsake/issues/79) | "Every Monday" |

### Power, setup and updates

| Feature | Status | What it is |
|---|---|---|
| Auto-sleep | ✅ | Screen rests after ~3 min, deep sleep after ~30 min |
| Power management | 📋 [#16](https://github.com/ekhumbata/keepsake/issues/16) | Slower CPU while idle |
| Wi-Fi only when needed | 📋 [#17](https://github.com/ekhumbata/keepsake/issues/17) | Radio off by default; jobs batched |
| Charging screen + low-battery nudges | 📋 [#84](https://github.com/ekhumbata/keepsake/issues/84) | Mochi while charging; gentle warnings at 20/10/5% |
| Battery saver + profiles | 🎁 [#58](https://github.com/ekhumbata/keepsake/issues/58) | Always fresh / Balanced / Max battery |
| Wi-Fi setup hotspot | ✅ | `Keepsake-XXXXXX` hotspot + setup page |
| Provisioning file | 📋 [#42](https://github.com/ekhumbata/keepsake/issues/42) | Drop a file on the SD card and it sets everything up, then deletes itself |
| Several saved networks | 📋 [#43](https://github.com/ekhumbata/keepsake/issues/43) | Up to 5; friendly "Let's reconnect" when a password changes |
| Over-the-air updates | 📋 [#40](https://github.com/ekhumbata/keepsake/issues/40) | Updates from GitHub Releases, with automatic rollback |
| Crash reports | 📋 [#41](https://github.com/ekhumbata/keepsake/issues/41) | Saved to the SD card for Eric |
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
- **Planned:** a provisioning file pre-loads everything ([#42](https://github.com/ekhumbata/keepsake/issues/42)).

### Add books over USB
1. Plug Keepsake into a computer with a USB-C data cable.
2. On Keepsake: **Settings → Storage → Enable OTG**. The SD card appears on the computer as a drive.
3. Copy `.txt` books into the `books` folder (EPUB support is coming, [#26](https://github.com/ekhumbata/keepsake/issues/26)).
4. **Eject the drive on the computer first**, then on Keepsake choose **Disable OTG mode**.

### Add books from a phone
📋 Planned ([#32](https://github.com/ekhumbata/keepsake/issues/32)): open **Add books** on Keepsake, scan the QR code
(or go to `keepsake.local`), then upload a file or search for a free book.

### Find a free book
📋 Planned ([#31](https://github.com/ekhumbata/keepsake/issues/31), [#33](https://github.com/ekhumbata/keepsake/issues/33)):
**Find a book** on the device or the **Search** tab on the phone, then pick a result to download it.

### Read
- **Home → Books**, pick a book, then **Continue reading** or **Start from the beginning**.
- Rocker down = next page, up = previous page. Tap Record to go back to the list. Your page is remembered.

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
- Charge with USB-C; the status bar shows the battery and a charging mark.
- Hold PWR ~1 s → **Shut down**. On USB power it stays on until unplugged.

### Fix common problems
| Problem | Try this |
|---|---|
| Frozen or not responding | Hold PWR for 6 s, then press PWR to start it again |
| The SD card is missing on Keepsake | Settings → Storage → **Disable OTG mode** (USB drive mode keeps the card for the computer) |
| Wi-Fi won't connect | Make sure the network is 2.4 GHz. In Settings → Network, pick it again and re-enter the password, or turn on **Access Point** and use the setup page |
| A book doesn't show up | Today it must be a `.txt` file directly inside `books` (not a subfolder) |
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
| Cut a release / OTA | 📋 [#40](https://github.com/ekhumbata/keepsake/issues/40) |
| Read crash logs | 📋 [#41](https://github.com/ekhumbata/keepsake/issues/41) |
| What's next on the board | issues labelled [`hardware`](https://github.com/ekhumbata/keepsake/issues?q=is%3Aopen+label%3Ahardware) and PRs labelled `needs-device-test` |
