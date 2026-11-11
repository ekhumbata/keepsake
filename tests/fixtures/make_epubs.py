#!/usr/bin/env python3
"""Generates the EPUB test shelf for tests/unit/test_epub_*.cpp.

All text is original filler written for these tests. Run by CMake at build time:
    python3 make_epubs.py <output-dir>
"""

from __future__ import annotations

import struct
import sys
import zipfile
import zlib
from pathlib import Path

CONTAINER = """<?xml version="1.0" encoding="UTF-8"?>
<container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">
  <rootfiles>
    <rootfile full-path="{opf}" media-type="application/oebps-package+xml"/>
  </rootfiles>
</container>
"""

XHTML = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html>
<html xmlns="http://www.w3.org/1999/xhtml" xmlns:epub="http://www.idpf.org/2007/ops">
<head><title>{title}</title><style>p {{ margin: 0 }}</style></head>
<body>
{body}
</body>
</html>
"""

CHAPTER_ONE = """
<h1 id="start">Chapter One</h1>
<p>The kettle   began to   sing
just as the rain arrived, and <b>Mira</b> decided the day was
<i>officially</i> a reading day.</p>
<p>She wrote three things on a card:<br/>tea<br/>blanket<br/>book</p>
<ul>
  <li>Find the lamp</li>
  <li>Move the cat <em>gently</em></li>
</ul>
<blockquote><p>A quiet afternoon is a small holiday.</p></blockquote>
<hr/>
<h2 id="part2">A Second Part</h2>
<p>It&#8217;s late&nbsp;now &mdash; the &ldquo;last&rdquo; page &amp; then sleep.</p>
<p><img src="../images/lamp.png" alt="A small lamp"/></p>
<script>var ignored = "this should not appear";</script>
"""

CHAPTER_TWO = """
<h1>Chapter Two</h1>
<p>Morning came with <strong>toast</strong> and a list of plans that nobody intended to keep.</p>
<pre>
line one
  line two
</pre>
<p id="end">The end of the sample.</p>
"""

NOTES = """
<p id="note1">A note that only appears when someone follows a link to it.</p>
"""


def write_epub(path: Path, files: list[tuple[str, str | bytes]], *, compress: bool = True) -> None:
    with zipfile.ZipFile(path, "w") as z:
        # mimetype first and stored, as the spec asks.
        z.writestr(zipfile.ZipInfo("mimetype"), "application/epub+zip",
                   compress_type=zipfile.ZIP_STORED)
        for name, data in files:
            z.writestr(name, data,
                       compress_type=zipfile.ZIP_DEFLATED if compress else zipfile.ZIP_STORED)


# A 1x1 PNG, enough to be a file.
def make_png(width: int, height: int) -> bytes:
    """A small grayscale cover (stdlib only): black top third, a mid-gray square below."""
    rows = []
    for y in range(height):
        row = bytearray([0])  # filter: none
        for x in range(width):
            if y < height // 3:
                row.append(0)
            elif height // 2 <= y < height * 5 // 6 and width // 4 <= x < width * 3 // 4:
                row.append(128)
            else:
                row.append(255)
        rows.append(bytes(row))

    def chunk(kind: bytes, data: bytes) -> bytes:
        return (struct.pack(">I", len(data)) + kind + data +
                struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

    header = struct.pack(">IIBBBBB", width, height, 8, 0, 0, 0, 0)  # 8-bit grayscale
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) +
            chunk(b"IDAT", zlib.compress(b"".join(rows))) + chunk(b"IEND", b""))


PNG = make_png(60, 90)


def epub2_files() -> list[tuple[str, str | bytes]]:
    opf = """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="2.0" unique-identifier="id">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>  A Rainy
      Day Sampler </dc:title>
    <dc:creator>Test Author</dc:creator>
    <dc:creator>Second Author</dc:creator>
    <dc:language>en</dc:language>
    <meta name="cover" content="cover-img"/>
  </metadata>
  <manifest>
    <item id="ncx" href="toc.ncx" media-type="application/x-dtbncx+xml"/>
    <item id="c1" href="text/one.xhtml" media-type="application/xhtml+xml"/>
    <item id="c2" href="text/two.xhtml" media-type="application/xhtml+xml"/>
    <item id="cover-img" href="images/cover.png" media-type="image/png"/>
    <item id="lamp" href="images/lamp.png" media-type="image/png"/>
  </manifest>
  <spine toc="ncx">
    <itemref idref="c1"/>
    <itemref idref="c2"/>
  </spine>
</package>
"""
    ncx = """<?xml version="1.0" encoding="UTF-8"?>
<ncx xmlns="http://www.daisy.org/z3986/2005/ncx/" version="2005-1">
  <navMap>
    <navPoint id="n1" playOrder="1">
      <navLabel><text>Chapter One</text></navLabel>
      <content src="text/one.xhtml#start"/>
      <navPoint id="n1a" playOrder="2">
        <navLabel><text>A Second   Part</text></navLabel>
        <content src="text/one.xhtml#part2"/>
      </navPoint>
    </navPoint>
    <navPoint id="n2" playOrder="3">
      <navLabel><text>Chapter Two</text></navLabel>
      <content src="text/two.xhtml"/>
    </navPoint>
  </navMap>
</ncx>
"""
    return [
        ("META-INF/container.xml", CONTAINER.format(opf="OEBPS/content.opf")),
        ("OEBPS/content.opf", opf),
        ("OEBPS/toc.ncx", ncx),
        ("OEBPS/text/one.xhtml", XHTML.format(title="One", body=CHAPTER_ONE)),
        ("OEBPS/text/two.xhtml", XHTML.format(title="Two", body=CHAPTER_TWO)),
        ("OEBPS/images/cover.png", PNG),
        ("OEBPS/images/lamp.png", PNG),
    ]


def epub3_files() -> list[tuple[str, str | bytes]]:
    opf = """<?xml version="1.0" encoding="UTF-8"?>
<package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">
  <metadata xmlns:dc="http://purl.org/dc/elements/1.1/">
    <dc:title>Sampler &amp; Friends</dc:title>
    <dc:creator id="a">Caf&#233; Writer</dc:creator>
    <dc:language>en-GB</dc:language>
  </metadata>
  <manifest>
    <item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>
    <item id="c1" href="chapter%201.xhtml" media-type="application/xhtml+xml"/>
    <item id="notes" href="notes.xhtml" media-type="application/xhtml+xml"/>
    <item id="c2" href="two.xhtml" media-type="application/xhtml+xml"/>
    <item id="cov" href="img/cover.png" media-type="image/png" properties="cover-image"/>
  </manifest>
  <spine>
    <itemref idref="c1"/>
    <itemref idref="notes" linear="no"/>
    <itemref idref="c2"/>
  </spine>
</package>
"""
    nav = XHTML.format(title="Contents", body="""
<nav epub:type="landmarks"><ol><li><a href="two.xhtml">Not the TOC</a></li></ol></nav>
<nav epub:type="toc" id="toc">
  <h1>Contents</h1>
  <ol>
    <li><a href="chapter%201.xhtml">Chapter
        One</a>
      <ol><li><a href="chapter%201.xhtml#part2">A Second Part</a></li></ol>
    </li>
    <li><a href="two.xhtml#end">Chapter Two</a></li>
  </ol>
</nav>
""")
    return [
        ("META-INF/container.xml", CONTAINER.format(opf="book/package.opf")),
        ("book/package.opf", opf),
        ("book/nav.xhtml", nav),
        ("book/chapter 1.xhtml", XHTML.format(title="One", body=CHAPTER_ONE.replace("../images/", "img/"))),
        ("book/notes.xhtml", XHTML.format(title="Notes", body=NOTES)),
        ("book/two.xhtml", XHTML.format(title="Two", body=CHAPTER_TWO)),
        ("book/img/cover.png", PNG),
    ]


ADOBE_ENCRYPTION = """<?xml version="1.0"?>
<encryption xmlns="urn:oasis:names:tc:opendocument:xmlns:container"
            xmlns:enc="http://www.w3.org/2001/04/xmlenc#">
  <enc:EncryptedData>
    <enc:EncryptionMethod Algorithm="http://www.w3.org/2001/04/xmlenc#aes128-cbc"/>
    <enc:CipherData><enc:CipherReference URI="OEBPS/text/one.xhtml"/></enc:CipherData>
  </enc:EncryptedData>
</encryption>
"""

FONT_OBFUSCATION = """<?xml version="1.0"?>
<encryption xmlns="urn:oasis:names:tc:opendocument:xmlns:container"
            xmlns:enc="http://www.w3.org/2001/04/xmlenc#">
  <enc:EncryptedData>
    <enc:EncryptionMethod Algorithm="http://www.idpf.org/2008/embedding"/>
    <enc:CipherData><enc:CipherReference URI="OEBPS/fonts/a.otf"/></enc:CipherData>
  </enc:EncryptedData>
</encryption>
"""


def main(out_dir: str) -> int:
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)

    write_epub(out / "epub2_ncx.epub", epub2_files())
    write_epub(out / "epub3_nav.epub", epub3_files())
    write_epub(out / "epub2_stored.epub", epub2_files(), compress=False)

    write_epub(out / "drm_adobe.epub",
               epub2_files() + [("META-INF/encryption.xml", ADOBE_ENCRYPTION),
                                ("META-INF/rights.xml", "<rights/>")])
    write_epub(out / "drm_encryption_only.epub",
               epub2_files() + [("META-INF/encryption.xml", ADOBE_ENCRYPTION)])
    write_epub(out / "font_obfuscated.epub",
               epub2_files() + [("META-INF/encryption.xml", FONT_OBFUSCATION)])

    write_epub(out / "no_container.epub",
               [f for f in epub2_files() if f[0] != "META-INF/container.xml"])
    write_epub(out / "no_package.epub", [("META-INF/container.xml", CONTAINER.format(opf="x.opf")),
                                         ("readme.txt", "hello")])

    good = (out / "epub2_ncx.epub").read_bytes()
    (out / "truncated.epub").write_bytes(good[: len(good) // 2])
    (out / "not_a_zip.epub").write_bytes(b"%PDF-1.4\n% not really a book\n" * 20)

    # A stored book with one byte of chapter two flipped: the CRC check must catch it.
    stored = bytearray((out / "epub2_stored.epub").read_bytes())
    marker = b"Morning came"
    index = stored.find(marker)
    assert index > 0
    stored[index] ^= 0x20
    (out / "bad_crc.epub").write_bytes(bytes(stored))
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1]))
