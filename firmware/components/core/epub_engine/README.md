# epub_engine

Portable EPUB reading for Keepsake (#26): no ESP-IDF includes, so it builds on the host,
where it is unit-tested (`tests/unit/test_epub_*.cpp`) and fuzzed
(`tests/fuzz/fuzz_epub_*.cpp`).

## Why our own (plan B)

The plan was to port CrossPoint Reader's `lib/Epub` (MIT). After a look at it, that would
have been a rewrite rather than a port:
- its XHTML parser lays text out as it goes, so it is tied to CrossPoint's `GfxRenderer`,
  fonts and page model
- it depends on `HalStorage`, `Memory`, `Serialization`, `Logging` and Arduino `Print`
- most of its 42k lines are hyphenation tables

This engine is the plan's "plan B": a small, dependency-free implementation of what a
reflowing 1-bit reader needs.

## Pieces

| File | What |
|---|---|
| `byte_source` | random-access bytes: `FileByteSource` (stdio, works on FATFS) or `MemoryByteSource` |
| `inflate` | raw DEFLATE decoder + CRC-32; bounded output, every read range-checked |
| `zip_archive` | central directory + single-entry extraction (stored/deflate), CRC-checked; no ZIP64 |
| `xml_tokenizer` | forgiving pull tokenizer: HTML named entities without a DTD, stray `<`, CDATA, DOCTYPE subsets |
| `href` | resolves hrefs inside the zip (percent-decoding, `..`, fragments, external links) |
| `epub_book` | container → OPF (metadata, cover, spine), DRM detection, TOC from EPUB 3 nav or EPUB 2 NCX |
| `xhtml_parser` | one content document → `Chapter` of `Block`s (see `content.h`) |

## Block model (`content.h`)

Paragraphs, headings (1–6), list items (with depth), blockquotes, preformatted text, images
(a placeholder for now, #69) and rules. Text is UTF-8 in runs with bold/italic flags; a run of
`"\n"` is a `<br>`. `Chapter::anchors` maps element ids to block indices, so TOC entries
with a `#fragment` land on the right block. TXT, FB2 and MOBI can produce the same model later.

## Errors the UI can explain (#29)

`EpubBook::Open` returns an `OpenError`:
- `kDrm`: Adobe `rights.xml`, Apple `sinf.xml`, or an `encryption.xml` that encrypts more than fonts
- `kNotEpub`: not a zip, or no package document; also a truncated download, since the zip index is at the end
- `kDamaged`: a broken zip or package
- `kNoContent`: an empty reading order
- `kIoError`

A chapter that fails to read (bad CRC, too large) fails on its own without closing the book.

## Limits

Package files (OPF/NCX/nav) are capped at 2 MB and a chapter's XHTML at 8 MB. A chapter also
stops at 20,000 blocks or 4 MB of text, with `Chapter::truncated` set. Other caps: 20,000 zip
entries, 4,000 TOC entries, an element-nesting depth of 256 (deeper elements are treated as
inline).

## Not here yet (for #27/#28)

- Caching parsed chapters/pagination under `/Books/.cache/`
- Running the parse on a worker task with a large PSRAM stack
- Pagination of blocks into pages
- CSS (class-based italics/bold, alignment)
- Inline images
