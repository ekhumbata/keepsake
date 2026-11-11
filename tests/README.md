# Host tests

Unit tests and fuzzers for the portable `firmware/components/core/` components (`epaper_ui`,
`design_tokens`, `project_assets`, `page_navigation`, and later `epub_engine`,
`book_formats`, `reminder_rules`). `core/` never includes ESP-IDF headers, so it builds with
the host compiler. `tests/CMakeLists.txt` compiles those same sources into a static library;
it never copies them.

## Run

Needs CMake ≥ 3.16, Ninja (or Make), a C++20 compiler and network access on the first
configure (doctest is fetched from GitHub, pinned to `v2.4.11`).

```bash
cmake -S tests -B build/tests -G Ninja
cmake --build build/tests
ctest --test-dir build/tests --output-on-failure   # or: build/tests/unit_tests
```

Unit tests build with AddressSanitizer + UndefinedBehaviorSanitizer by default
(`-DKEEPSAKE_SANITIZE=OFF` to turn them off).

## Fuzz

Needs clang with its sanitizer runtimes (Ubuntu: `clang-18 libclang-rt-18-dev`).

```bash
cmake -S tests -B build/fuzz -G Ninja -DKEEPSAKE_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build/fuzz
mkdir -p build/fuzz/corpus
build/fuzz/fuzz_paginate_book_text -max_total_time=60 build/fuzz/corpus
```

CI runs every `fuzz/fuzz_*.cpp` target for 60 s and uploads any crashing input as an
artifact. To reproduce one locally, pass the file to the fuzzer: `build/fuzz/<target> <crash-file>`.

## Layout

| Path | What |
|---|---|
| `unit/main.cpp` | doctest's `main()` |
| `unit/test_*.cpp` | test cases; any new file is picked up automatically |
| `fuzz/fuzz_*.cpp` | one libFuzzer target per file (`LLVMFuzzerTestOneInput`) |

## Screen gallery

`screen_gallery` (`gallery/screen_gallery.cpp`) renders every `epaper_ui` screen and overlay
from sample states into the panel's own 800×480 framebuffer and writes portrait PNGs.
`gallery/golden.txt` holds a CRC-32 per screen; the `screen_gallery` ctest fails when a screen
changes. CI uploads the PNGs as the `screen-gallery-<sha>` artifact on every run, so a design
change can be reviewed from the Actions page, even on a phone. Why it's headless and
host-built: `docs/adr/0001-simulator-scope.md`.

```bash
build/tests/screen_gallery --out build/tests/gallery                         # just render
build/tests/screen_gallery --out build/tests/gallery --golden tests/gallery/golden.txt --update
```

After an **intended** UI change, look at the PNGs, rerun with `--update`, and commit
`golden.txt` with the change. A new screen gets a sample state in `Screens()` in the same PR.

## Writing tests

- Test behavior the device relies on (invariants, edge cases, bugs that happened), not
  implementation details.
- Render code: draw into a real 800×480 1-bpp framebuffer (see `unit/test_render_smoke.cpp`)
  and let the sanitizers catch out-of-bounds writes. Pixel-exact golden images belong to the sim
  gallery (#14).
- Test text must be original (never paste excerpts from real books).
- A new parser (EPUB, MOBI, FB2…) gets a fuzz target in the same PR.
