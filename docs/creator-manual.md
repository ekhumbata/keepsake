# Keepsake creator's manual

For whoever builds a Keepsake, sets it up and gives it away: getting firmware onto the board,
preparing the SD card, the setup file, checking a board, shipping updates, and getting out of
trouble.

There are three other documents, each for a different reader:

| Document | For |
|---|---|
| [User manual](user-manual.md) | the person who uses the Keepsake: every screen and button |
| [Guide](guide.md) | the feature list (what works, what's planned) and short how-tos |
| [CLAUDE.md](../CLAUDE.md), [app architecture](app-architecture.md) | working on the code |

## Contents

- [What you need](#what-you-need)
- [Put the firmware on the board](#put-the-firmware-on-the-board)
- [Prepare the SD card](#prepare-the-sd-card)
- [The setup file](#the-setup-file)
- [Photos](#photos)
- [Books](#books)
- [Setting one up as a gift](#setting-one-up-as-a-gift)
- [Check a board](#check-a-board)
- [Ship an update](#ship-an-update)
- [Where things are kept](#where-things-are-kept)
- [Looking after the screen](#looking-after-the-screen)
- [When something goes wrong](#when-something-goes-wrong)

## What you need

- A [Waveshare ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97) board.
- A microSD card. FAT32 works; Keepsake can also format it (Settings → Storage → Format SD).
- A USB-C cable that carries **data**. Many cables only charge; with one of those the board
  never shows up on the computer and nothing below works.
- Optional: a 3.7 V lithium battery with an MX1.25 plug. Without one the board runs from USB.

The board has three controls: a **rocker** (tilt up/down, press in to select), a **BOOT**
button (called **Record** in the user manual: tap = Back, hold = record) and **PWR**.

## Put the firmware on the board

### Get the firmware

- **A release:** [GitHub Releases](https://github.com/ekhumbata/keepsake/releases) has the
  images for every tagged version.
- **Any commit on `main` or a PR:** every CI run of the `firmware` workflow attaches the images
  as an artifact named `firmware-<commit>` (Actions → the run → Artifacts). From the command
  line: `gh run download <run id> -R ekhumbata/keepsake -n firmware-<commit>`.
- **Build it yourself:** ESP-IDF v5.5, then `cd firmware && idf.py build` (see CLAUDE.md).

The download has four images and where each one goes (also listed in `flash_args`):

| Address | File |
|---|---|
| `0x0` | `bootloader/bootloader.bin` |
| `0x8000` | `partition_table/partition-table.bin` |
| `0xf000` | `ota_data_initial.bin` |
| `0x20000` | `chroninkle.bin` (the app; the name is kept from upstream) |

### Flash it

**From a browser (no tools to install):** in Chrome or Edge, open
[esptool-js](https://espressif.github.io/esptool-js/), click **Connect**, pick the board's
port, add the four files at the addresses above, and click **Program**. Unplug and plug the
board back in when it's done.

**From the command line** (any OS, `pip install esptool`):

```bash
python -m esptool --chip esp32s3 -p <PORT> -b 460800 write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 bootloader/bootloader.bin 0x8000 partition_table/partition-table.bin \
  0xf000 ota_data_initial.bin 0x20000 chroninkle.bin
```

`<PORT>` is `COM3` (or similar) on Windows, `/dev/ttyACM0` on Linux, `/dev/cu.usbmodem…` on a
Mac. **From WSL** the board's USB isn't visible; run Windows Python against the COM port
(`python.exe -m esptool … -p COM3 …`) or pass the USB through with `usbipd-win`.

**From the repo with ESP-IDF:** `cd firmware && idf.py -p <PORT> -b 460800 flash monitor`.

### Download mode

A board running Keepsake goes into download mode by itself when you flash it. A board running
something else (the Waveshare factory demo, with its Chinese menus, or a crashed build) may
not: **hold BOOT, plug in the USB cable, then let go.** The screen doesn't change (e-paper
keeps its last image), but a new port appears (Windows: Device Manager → Ports → "USB
JTAG/serial debug unit").

Flashing leaves the SD card alone. It also keeps what's stored on the board itself (Wi-Fi,
settings, the setup file's values); to wipe those too, run `esptool … erase_flash` before
flashing.

## Prepare the SD card

Put these on the card from a computer (card reader, or USB drive mode: see the user manual's
[Copying books and photos from a computer](user-manual.md#copying-books-and-photos-from-a-computer)):

| Path | What |
|---|---|
| `books/` | `.epub` and `.txt` books, in folders up to 4 deep if you like |
| `photos/` | `.jpg` / `.png` photos, two folder levels |
| `.keepsake/provision.json` | the [setup file](#the-setup-file) (optional) |

Folders and files whose names start with a dot are hidden on some computers; that's normal.

Keepsake makes the rest itself: see [Where things are kept](#where-things-are-kept).

The card is read when Keepsake starts. A card put in while it's on is picked up the next time
Books, Photos or Settings → Storage opens (the board has no card-detect line).

## The setup file

`.keepsake/provision.json` sets a Keepsake up without touching it: Wi-Fi, time zone, names and
dates, notes. Keepsake reads it the next time it starts, before Wi-Fi comes up, then
**overwrites and deletes it** so no passwords stay on the card, and shows **All set up!**.

If anything in it is wrong, **nothing** is imported: Keepsake shows **The setup file has a
problem** and writes `.keepsake/provision-problems.txt` next to it, one line per problem
(e.g. `wifi[1].password: must be 8 to 63 characters (or empty for an open network)`). Fix
the file and restart. An unknown field (a typo) is reported but doesn't stop the rest.

Start from [`provision.example.json`](../provision.example.json). **Copy it somewhere outside
the repo** before filling it in: the repo is public, and `provision.json` must never be
committed. Every field is optional; delete the ones you don't need.

| Field | What | Rules |
|---|---|---|
| `version` | file format | `1` (or leave it out) |
| `wifi` | networks to join, most preferred first | a list of up to 5 `{ "ssid": ..., "password": ... }`; names up to 32 characters, passwords 8-63 (or `""` for an open network); 2.4 GHz only |
| `timezone` | the clock's time zone | a name like `"America/Edmonton"` or `"Europe/London"` |
| `weather_location` | where the Home weather is for | a town (`"Calgary"`) or `"lat, lon"`; up to 64 characters |
| `friend_name` | the name Home greets | up to 32 characters |
| `birthday` | a birthday greeting and card | `"MM-DD"` or `"YYYY-MM-DD"` |
| `anniversary`, `first_date` | anniversary greetings and cards | `"MM-DD"`, or `"YYYY-MM-DD"` to count the years |
| `holidays` | which holidays to celebrate | a list from `christmas`, `new_year`, `valentines`, `halloween`; `[]` for none; leave it out for all four |
| `special_days` | your own days | up to 12 `{ "date": "MM-DD", "title": ..., "message": ... }`; title up to 48 characters, message up to 160 |
| `welcome_message` | a welcome card shown once on Home | up to 400 characters; a new one shows again |
| `about_message` | a note on Settings → About | up to 400 characters |
| `show_intro` | keep the first-run introduction for the next start | `true` or `false` (no quotes): see [gifting](#setting-one-up-as-a-gift) |
| `gemini_key` | turns on transcription and summaries | a Gemini API key; leave it out to keep notes as audio |
| `_comment` | ignored | for your own notes |

The values go into the board's own storage, so they survive a reformatted card. A new setup
file replaces the fields it has and leaves the others as they were.

## Photos

Two ways to add them, both fine:

- **Straight from a phone or camera:** copy the files into `photos/`. Full-size phone photos
  work (they're decoded at reduced size); the first time each one is shown takes a few
  seconds while Keepsake prepares it, and it's cached after that.
- **Prepared on a computer:** `python3 tools/photos/prepare_photos.py <your folder> <card>/photos`
  (needs `pip install -r tools/requirements.txt`; add `pillow-heif` for iPhone HEIC). It turns
  them upright, makes 960 px plain JPEGs and names them `2019-05-20 First date.jpg`. Smaller
  files, quicker first view, and HEIC becomes something Keepsake can read. It refuses to write
  inside the repo.

Photos show **oldest first**. The date comes from the file name if it starts with one, else
from the photo's EXIF date; the rest of the name is the caption (`2019-05-20 First date.jpg`
→ "First date, May 20, 2019"). Camera names like `IMG_1234` or `PXL_…` give a date-only
caption. Rename a file to change either.

HEIC and WebP can't be shown directly: Keepsake says so (save them as JPEG, or use the tool).

With **Settings → Display & sound → Lock screen shows: A photo**, the lock screen is a random
photo from the same folder.

## Books

- **EPUB** (without DRM) and **`.txt`** (UTF-8, UTF-16 or Latin-1). Free classics: Project
  Gutenberg, Standard Ebooks.
- Books bought from most stores are locked to their app (DRM); Keepsake says so instead of
  opening them. PDF isn't supported.
- Each book remembers its place (`<book>.pos`) and bookmarks (`<book>.bm`) next to it. Deleting
  a book from Books removes those too.

## Setting one up as a gift

1. Flash the firmware you want to give (ideally a release, so it updates itself afterwards).
2. Fill in a setup file with Wi-Fi, time zone, names, dates, `welcome_message`,
   `about_message`, and `"show_intro": true`. Put it on the card as `.keepsake/provision.json`,
   with the books and photos.
3. Put the card in and switch Keepsake on. It imports the file, shows **All set up!** and goes
   straight to **Home** (not the introduction, because of `show_intro`).
4. Check what you like: Books, Photos, the About note, the lock screen, Wi-Fi in the status bar.
5. **Shut it down** (hold PWR about a second → Shut down). The screen clears to white.
6. The next time it's switched on, it shows the first-run introduction and then the welcome
   note, as if new.

If it gets started again before it's given, switch it off during the introduction: it only
counts as seen once someone finishes it. **Settings → About → Replay the introduction** shows
it any time without changing that.

The recipient's Wi-Fi must be **2.4 GHz** and can't need a sign-in page. If you don't know
it, leave `wifi` out: without saved Wi-Fi the introduction offers the phone setup (Keepsake
opens a `Keepsake-XXXXXX` hotspot with a setup page).

## Check a board

The firmware has a **test console** on its USB serial port (on by default,
`CONFIG_KEEPSAKE_TEST_CONSOLE`). With the board plugged in and on a normal screen:

```bash
pip install -r tools/board/requirements.txt          # pyserial
python3 tools/board/run_board_tests.py               # self-test, screenshots, a UI tour
python3 tools/board/keepsake_board.py screenshot now.png
python3 tools/board/keepsake_board.py raw "press down click"
```

Add `--port COM3` on Windows (from WSL, run them with Windows Python). The runner writes a
report with real screenshots to `build/board-tests/<time>/`. The self-test covers memory,
NVS, the power chip and battery, the clock, the motion and climate sensors, the SD card, the
display, Wi-Fi and the update slots. Everything else, including every console command, is in
[board-tests.md](board-tests.md).

The console can't press PWR, unplug the cable or move the SD card: lock/unlock, shut-down,
charging and card swaps need hands.

## Ship an update

Keepsake checks GitHub Releases once a day (or on Settings → About → Check for updates),
downloads a newer version in the background, and starts it the next time it sleeps. If the
new version fails its health check it goes back to the old one and doesn't offer that
version again.

To release:

1. Bump `firmware/version.txt` and the CHANGELOG.
2. Write one friendly line in `firmware/whats_new.txt`: it's the "Keepsake updated" card.
3. Merge to `main`, then `git tag vX.Y.Z && git push origin vX.Y.Z`. The `release` workflow
   checks the tag matches `version.txt`, builds, and publishes the images and `latest.json`.
4. A tag like `vX.Y.Z-rc1` makes a prerelease that devices ignore: use it to try a build.

`python3 tools/board/run_board_tests.py --ota` rehearses an update and a rollback on a board.

## Where things are kept

**On the SD card** (Keepsake makes these; they're safe to leave alone):

| Path | What |
|---|---|
| `books/.library` | the shelf: titles, authors, progress, recently read (rebuilt from the books if it goes missing) |
| `books/<book>.pos`, `.bm` | where you are in a book, and its bookmarks |
| `books/.cache/` | cover thumbnails |
| `.keepsake/photo-cache/` | photos prepared for the screen |
| `.keepsake/crash/` | crash reports (`crash-NNN.txt` + `.elf`), the last 10 |
| `.keepsake/provision-problems.txt` | why a setup file was refused |
| `recordings/`, `todos/`, `summaries/`, `topics.json`, `trash/` | voice notes and what was made from them |

**On the board** (kept across card swaps and reflashing; wiped by `erase_flash`): Wi-Fi
networks, time zone, the setup file's names, dates and notes, whether the introduction and
welcome note were seen, reader text size, lock screen choice, button sounds.

## Looking after the screen

E-paper holds its image with no power and doesn't burn in like an LCD. A picture left on it
for weeks, especially warm or in the sun, can leave a faint ghost that a few refreshes clear.
That's why Keepsake clears the screen to white when it shuts down. For long storage, shut it
down (white screen) rather than leaving it on a page.

## When something goes wrong

| Problem | Try |
|---|---|
| The board doesn't show up on the computer | Another cable (charge-only cables are common); then [download mode](#download-mode) |
| The screen shows Chinese menus | That's the Waveshare factory demo: flash Keepsake |
| Flashing works but nothing changes on screen | Unplug and plug back in (the reset after flashing doesn't always take over USB) |
| Frozen, buttons do nothing | Hold PWR 6 seconds (cuts power in hardware), then press PWR |
| The card isn't seen | It's read at start: restart, or open Books / Photos / Storage. If USB drive mode was on, **Disable OTG mode** |
| "The setup file has a problem" | Read `.keepsake/provision-problems.txt`, fix the file, restart |
| A photo says "can't open" | HEIC/WebP: save as JPEG or use the photo tool. "Too big": a smaller copy |
| A book says it's locked or damaged | DRM or a broken file; a DRM-free copy works |
| Board tests reboot the board on every command | An old `keepsake_board.py` (before #142): update the repo |
| It crashed | The next start writes `.keepsake/crash/crash-NNN.txt`; decode the `.elf` with `idf.py coredump-info -c crash-NNN.elf` against the same build |
