#!/usr/bin/env python3

from __future__ import annotations

import math
import sys
from dataclasses import dataclass
from pathlib import Path

try:
    import freetype
except ImportError as exc:  # pragma: no cover - environment check
    raise SystemExit(
        "font generation needs freetype-py: pip install -r tools/requirements.txt"
    ) from exc


# Rasterizes TTF/OTF glyphs into the packed 1-bpp ASCII bitmap fonts in
# firmware/components/core/epaper_ui/generated_epaper_fonts.cpp, using FreeType (any OS).
#
# It reproduces the semantics of the original macOS CoreText generator so existing fonts keep
# their metrics:
# - `size` is the em size in pixels (CoreText points at 72 dpi)
# - ascent/descent/leading come from the font's horizontal header, rounded up
# - each glyph's box is its exact, unhinted outline bounding box, rounded outward to pixels;
#   the bitmap is a non-antialiased render of the unhinted outline (a pixel is black when its
#   centre is inside the outline)
# - the advance is the unhinted advance, rounded up
# Output is not byte-identical to CoreText's (the two rasterizers differ on edge pixels);
# see docs/asset-generation.md for the measured difference.


@dataclass(frozen=True)
class FontSpec:
    path: str
    symbol: str
    size: float


@dataclass(frozen=True)
class GlyphRecord:
    width: int
    height: int
    bearing_x: int
    bearing_y: int
    advance: int
    bitmap: list[int]


class FontGenError(RuntimeError):
    pass


kAsciiFirst = 32
kAsciiLast = 126
k26Dot6 = 64.0


def parse_args(argv: list[str]) -> tuple[Path, list[FontSpec]]:
    if len(argv) < 4 or argv[1] != "--output":
        raise FontGenError(
            "usage: generate_epaper_fonts.py --output <file> <font-path:symbol:size>..."
        )

    output_path = Path(argv[2])
    specs: list[FontSpec] = []
    for raw in argv[3:]:
        parts = raw.rsplit(":", 2)
        if len(parts) != 3:
            raise FontGenError(f"invalid font spec: {raw}")
        try:
            size = float(parts[2])
        except ValueError as exc:
            raise FontGenError(f"invalid font size in spec: {raw}") from exc
        specs.append(FontSpec(path=parts[0], symbol=parts[1], size=size))

    return output_path, specs


def load_font(spec: FontSpec) -> "freetype.Face":
    try:
        face = freetype.Face(spec.path)
    except freetype.FT_Exception as exc:
        raise FontGenError(f"unable to load font: {spec.path}") from exc
    # 72 dpi makes one point one pixel, matching CoreText's CTFontCreateWithGraphicsFont(size).
    face.set_char_size(int(round(spec.size * k26Dot6)), 0, 72, 72)
    return face


def font_metrics(face: "freetype.Face", size: float) -> tuple[int, int, int]:
    """ascent, descent, leading in pixels (each rounded up), like CTFontGetAscent & co."""
    scale = size / face.units_per_EM
    hhea_ascent = face.ascender
    hhea_descent = -face.descender
    hhea_gap = max(0, face.height - (face.ascender - face.descender))
    ascent = max(1, math.ceil(hhea_ascent * scale - 1e-9))
    descent = max(0, math.ceil(hhea_descent * scale - 1e-9))
    leading = max(0, math.ceil(hhea_gap * scale - 1e-9))
    return ascent, descent, leading


def resolve_glyph(face: "freetype.Face", character: int) -> int:
    index = face.get_char_index(character)
    if index:
        return index
    if character != ord("?"):
        return resolve_glyph(face, ord("?"))
    raise FontGenError("font is missing fallback glyph '?'")


def render_glyph(face: "freetype.Face", character: int) -> GlyphRecord:
    glyph_index = resolve_glyph(face, character)
    face.load_glyph(glyph_index, freetype.FT_LOAD_NO_HINTING | freetype.FT_LOAD_NO_BITMAP)
    slot = face.glyph

    # linearHoriAdvance is the unhinted advance in 16.16 pixels.
    advance = max(1, math.ceil(slot.linearHoriAdvance / 65536.0 - 1e-9))

    outline = slot.outline
    if outline.n_points == 0 or outline.n_contours == 0:
        return GlyphRecord(0, 0, 0, 0, advance, [])

    bbox = outline.get_bbox()  # exact bounding box, 26.6 pixels, y up
    min_x = math.floor(bbox.xMin / k26Dot6)
    max_x = math.ceil(bbox.xMax / k26Dot6)
    min_y = math.floor(bbox.yMin / k26Dot6)
    max_y = math.ceil(bbox.yMax / k26Dot6)
    width = max(0, max_x - min_x)
    height = max(0, max_y - min_y)
    if width == 0 or height == 0:
        return GlyphRecord(0, 0, min_x, max_y, advance, [])

    slot.render(freetype.FT_RENDER_MODE_MONO)
    bitmap = slot.bitmap
    # The rendered bitmap covers the outline's control box rounded out to whole pixels, which
    # contains the exact box above; copy the overlap into a canvas of exactly that box.
    left = slot.bitmap_left
    top = slot.bitmap_top
    pitch = abs(bitmap.pitch)
    buffer = bitmap.buffer

    packed: list[int] = []
    current_byte = 0
    bit_index = 0
    for row in range(height):
        src_row = row + (top - max_y)
        for col in range(width):
            src_col = col + (min_x - left)
            black = False
            if 0 <= src_row < bitmap.rows and 0 <= src_col < bitmap.width:
                byte = buffer[src_row * pitch + (src_col >> 3)]
                black = (byte & (0x80 >> (src_col & 7))) != 0
            if black:
                current_byte |= 0x80 >> bit_index
            bit_index += 1
            if bit_index == 8:
                packed.append(current_byte)
                current_byte = 0
                bit_index = 0
    if bit_index:
        packed.append(current_byte)

    return GlyphRecord(
        width=width,
        height=height,
        bearing_x=min_x,
        bearing_y=max_y,
        advance=advance,
        bitmap=packed,
    )


def emit_byte_array(values: list[int], indent: str) -> str:
    if not values:
        return ""

    lines: list[str] = []
    line = indent
    for index, value in enumerate(values):
        entry = f"0x{value:02X}"
        if index == 0:
            line += entry
        elif len(line) + len(entry) + 2 > 100:
            lines.append(line + ",")
            line = indent + entry
        else:
            line += ", " + entry
    lines.append(line)
    return "\n".join(lines)


def generate_source(specs: list[FontSpec]) -> str:
    out = [
        '#include "generated_epaper_fonts.h"',
        "",
        "namespace epaper_fonts {",
        "",
    ]

    for spec in specs:
        font = load_font(spec)
        ascent, descent, leading = font_metrics(font, spec.size)
        line_height = max(1, ascent + descent + leading)

        all_bitmap_bytes: list[int] = []
        glyph_lines: list[str] = []
        for code in range(kAsciiFirst, kAsciiLast + 1):
            glyph = render_glyph(font, code)
            offset = len(all_bitmap_bytes)
            all_bitmap_bytes.extend(glyph.bitmap)
            glyph_lines.append(
                "    { %5u, %4u, %3u, %3u, %4d, %4d, %3u },"
                % (
                    offset,
                    len(glyph.bitmap),
                    glyph.width,
                    glyph.height,
                    glyph.bearing_x,
                    glyph.bearing_y,
                    glyph.advance,
                )
            )

        out.extend(
            [
                "namespace {",
                f"const epaper_font::GlyphBitmap {spec.symbol}_glyphs[] = {{",
                *glyph_lines,
                "};",
                "",
                f"const uint8_t {spec.symbol}_bitmaps[] = {{",
            ]
        )
        if all_bitmap_bytes:
            out.append(emit_byte_array(all_bitmap_bytes, indent="    "))
        out.extend(
            [
                "};",
                "}  // namespace",
                "",
                f"const epaper_font::BitmapFont {spec.symbol} = {{",
                f'    "{spec.symbol}",',
                f"    {kAsciiFirst},",
                f"    {kAsciiLast},",
                f"    {line_height},",
                f"    {ascent},",
                f"    {spec.symbol}_glyphs,",
                f"    {spec.symbol}_bitmaps,",
                "};",
                "",
            ]
        )

    out.append("}  // namespace epaper_fonts")
    out.append("")
    return "\n".join(out)


def main(argv: list[str]) -> int:
    try:
        output_path, specs = parse_args(argv)
        source = generate_source(specs)
        output_path.write_text(source, encoding="utf-8")
        return 0
    except FontGenError as exc:
        sys.stderr.write(f"font generation failed: {exc}\n")
        return 1


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
