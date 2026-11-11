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

## Writing tests

- Test behavior the device relies on (invariants, edge cases, bugs that happened), not
  implementation details.
- Render code: draw into a real 800×480 1-bpp framebuffer (see `unit/test_render_smoke.cpp`)
  and let the sanitizers catch out-of-bounds writes. Pixel-exact golden images belong to the sim
  gallery (#14).
- Test text must be original (never paste excerpts from real books).
- A new parser (EPUB, MOBI, FB2…) gets a fuzz target in the same PR.
