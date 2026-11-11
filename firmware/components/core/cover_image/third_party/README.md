# third_party

- `stb_image.h`: stb_image v2.30 by Sean Barrett and contributors, from
  https://github.com/nothings/stb at commit `f58f558c120e9b32c217290b80bad1a0729fbb2c`,
  with one patch, marked "Keepsake patch": `stbi__getn` skips its `memcpy` when asked for
  0 bytes (a zero-length PNG chunk passes a null buffer; found by `fuzz_cover_image`). Dual-licensed public domain (Unlicense) / MIT; see the end of the file.
  Only JPEG, PNG and GIF are compiled in (see `../stb_image_impl.cpp`).
