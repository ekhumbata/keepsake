# 0001: Simulator scope: host-built screen gallery, no interactive sim before the gift

- **Status:** accepted (2026-11-11)
- **Issue:** #13 (the 1a spike); this decides #14 (1b) and #63 (1c)

## Context

Plan Phase 1 offered a timeboxed desktop simulator in three tiers. 1a was a spike to see
whether the firmware's portable code runs on a host (the ESP-IDF `linux` target, or plain
CMake + the FreeRTOS POSIX port + shims). 1b was a headless screen gallery with golden images.
1c was an interactive simulator running `firmware/main` with faked drivers.

## What the spike found

- **The portable code already builds on the host with plain CMake and no shims.** `tests/`
  (#8) compiles every `firmware/components/core/` component unchanged with g++ and clang under
  ASan/UBSan. The only fix needed was one missing `<cstdint>` include. Neither the IDF `linux`
  target nor a FreeRTOS port is needed for anything that draws a screen.
- The page renderers draw into the same 800×480 1-bpp framebuffer layout the panel uses
  (portrait x maps onto the gate line). So a host render is the device's framebuffer, bit for
  bit. What it can't show is the panel itself: ghosting, partial-refresh banding, contrast.
- `firmware/main` and the services are a different story. They use FreeRTOS tasks and queues,
  `esp_log`, NVS, Wi-Fi, SDMMC/FATFS, I2S, `esp_pm`, `esp_timer` and the GPIO/PMIC interrupt
  plumbing. Running them on a host means faking well over five services, which is the plan's
  own quit condition for 1c.

## Decision

1. **1a: done.** No `sim/` IDF project and no FreeRTOS POSIX port.
2. **1b: build the screen gallery on the existing `tests/` host build** (#14). It renders every
   screen and overlay from sample states into PNGs, checks a golden hash per screen in CI, and
   uploads the PNGs as a CI artifact for review. No SDL window: headless images cover design
   review and regression checks, and no SDL dependency is needed.
3. **1c: not before the gift.** Stays in #63 (post-gift, "only if still wanted").
4. Rendering fidelity is checked against the panel with an on-device screenshot (#15) at a
   hardware session. If the two diverge, the gallery is wrong and gets fixed or dropped, as
   the plan says.

## Consequences

- Design review (M3) works from gallery PNGs posted as CI artifacts, from any machine,
  including a phone.
- Flows that span screens (navigate, record, open a book) are still checked on the device.
- New page renderers belong in `firmware/components/core/epaper_ui`, which keeps them
  host-renderable; anything that needs ESP-IDF to draw can't be in the gallery.
