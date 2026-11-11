# Keepsake

![Keepsake](assets/logos/keepsake-logo.png)

Keepsake is cozy firmware for the [Waveshare ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97):
a calm e-paper **book reader** first, an **ambient companion** (clock, calendar, weather, indoor climate) second, with
reminders, timers and voice notes alongside. It's designed as a gift for someone who reads a lot and doesn't want to
think about the tech: setup and recovery need no AI or API knowledge, battery life matters, and it should feel warm
rather than gadgety.

Keepsake is a fork of [Chroninkle](https://github.com/mach1na/chroninkle) (a voice-first thought-capture app), which
grew out of ALXV Labs' Followup. Everything Chroninkle does still works; Keepsake builds the reading and ambient
features on top.

**Status:** early. The plan of record is [`docs/plan.md`](docs/plan.md), and progress is tracked as
[milestones](https://github.com/ekhumbata/keepsake/milestones) and
[issues](https://github.com/ekhumbata/keepsake/issues).

## What works today (inherited from Chroninkle)

- **Voice notes:** hold Record to capture, tag it Idea / To-do / Note / Topic, review playback before saving. Stored on
  the SD card as audio.
- **Optional Gemini transcription and summaries,** when a key is set.
- **Todos and follow-ups,** with pending todos on the lock screen.
- **Plain `.txt` books** from the SD card, with saved reading position.
- **USB drive mode** (Settings → Storage) to copy files onto the SD card from a computer.
- **Wi-Fi setup** through the device's own `Keepsake-XXXXXX` hotspot.

The [guide](docs/guide.md) has the feature list (with status), controls and how-tos; the full walkthrough of every
screen is in the [user manual](docs/user-manual.md).

## Coming next

EPUB reading with a proper reader font, a books-first Home screen, a library with covers, adding books from a phone
over Wi-Fi, free public-domain books (Project Gutenberg), an ambient lock screen, a cute original visual identity, and
over-the-air updates. See the [plan](docs/plan.md) for the full list and order.

## Brief Specifications

Keepsake runs on the [Waveshare ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97).

| Item | Information |
| --- | --- |
| Product Name | Keepsake (on ESP32-S3-ePaper-3.97) |
| Product Type | E-paper reader + ambient companion, with voice notes |
| MCU | ESP32-S3R8, dual-core Xtensa LX7 up to 240MHz |
| Memory | 8MB PSRAM, 16MB flash |
| Screen | 3.97-inch black-and-white ePaper, 800 x 480, SSD1677 controller |
| Interaction | Buttons only — this board has no touchscreen (see [Controls](#controls)) |
| Connectivity | 2.4GHz Wi-Fi (802.11 b/g/n), Bluetooth 5 (LE) |
| Audio | ES8311 codec, onboard microphone, NS4150B amplifier, speaker header |
| Sensors | QMI8658 6-axis IMU, PCF85063 real-time clock |
| Power | AXP2101 PMIC, 3.7V lithium battery (MX1.25 connector), USB-C charging |
| Storage | microSD card (recordings, transcripts, summaries, `.txt` books) |
| AI (optional) | Gemini (cloud) transcription and summarization, over Wi-Fi; everything else works without it |

The board also carries an SHTC3 temperature/humidity sensor on the shared I2C bus; the ambient screen will use it ([#36](https://github.com/ekhumbata/keepsake/issues/36)).

## Controls

Keepsake is driven entirely by three physical controls: a rocker, the Record button, and the PWR button. There is no touchscreen.

| Control | Action |
| --- | --- |
| Rocker, tilt up / down | Move the selection; hold a tilt to keep moving |
| Rocker, press in ("Select") | Select / confirm |
| Record button, tap ("Back") | Back out of a list, scroll view, or switch you're currently inside |
| Record button, press and hold | Record — starts on the hold, stops when you let go |
| PWR, tap | Lock the screen, or unlock it |
| PWR, hold ~1s | Open the shutdown confirmation |
| PWR, hold 6s | Hardware power-off, straight from the PMIC, no matter what's on screen |

Recording is exclusive to the Record button, so no other control can start or stop a capture by accident. The 6-second PWR hold bypasses the firmware entirely and always cuts power.

See the [user manual](docs/user-manual.md) for how these controls apply on every screen.

## License and Attribution

Keepsake is licensed under the [GNU GPLv3](LICENSE), like the projects it's built on:
Copyright (C) 2026 Alex Vong ([ALXV](https://github.com/alxv2016)), Copyright (C) 2026 Craig Richards
([mach1na](https://github.com/mach1na)), and the Keepsake contributors.

- **[Chroninkle](https://github.com/mach1na/chroninkle)** by mach1na is the direct upstream; Keepsake forked it at
  v1.0.0. Chroninkle was itself renamed from **Followup** (its name through version 0.5.0).
- **Followup** was a fork of [ALXV's folloup-sticky](https://github.com/alxv2016/folloup-sticky), whose `main` branch
  targets SeeedStudio's reTerminal Sticky and whose `folloup-waveshare` branch ports it to this Waveshare board.
  The boot screen credits Chroninkle and shows Followup's original logo on its "Based on" line.

Thank you to ALXV and mach1na for the design and implementation Keepsake grows from.

Internal identifiers (`CONFIG_CHRONINKLE_*`, the `chroninkle` CMake project, source symbols) deliberately keep their
upstream names so upstream fixes can still be cherry-picked.

The on-device UI's bitmap fonts are generated from [Inter](https://github.com/rsms/inter), © The Inter Project Authors,
licensed under the [SIL Open Font License 1.1](fonts/OFL.txt).
