#!/usr/bin/env python3

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path


@dataclass(frozen=True)
class AssetSpec:
    path: Path
    symbol: str


@dataclass(frozen=True)
class BitmapAsset:
    width: int
    height: int
    stride_bytes: int
    data: list[int]


class AssetGenError(RuntimeError):
    pass


def parse_asset_spec(raw: str) -> AssetSpec:
    parts = raw.split(":")
    if len(parts) != 2:
        raise AssetGenError(f"invalid asset spec: {raw}")
    path = Path(parts[0])
    if not path.is_file():
        raise AssetGenError(f"asset file not found: {path}")
    return AssetSpec(path=path, symbol=parts[1])


def load_png_pixels(asset_path: Path, *, fixed_size: int | None = None) -> tuple[int, int, list[int]]:
    """Loads a PNG (any OS, via Pillow) as 1 = ink, 0 = paper.

    A pixel is ink when it is at least slightly opaque (alpha >= 16) and darker than near-white
    (luminance < 245), the same threshold the original macOS sips/BMP pipeline used.
    `fixed_size` resizes to a square first, for icon sets drawn at a larger size. Bicubic is
    the Pillow filter closest to sips's output (measured on the 44 -> 36 px icons: 12/58
    identical, 3% of ink pixels differ, all on edges).
    """
    try:
        from PIL import Image
    except ImportError as exc:  # pragma: no cover - environment check
        raise AssetGenError(
            "asset generation needs Pillow: pip install -r tools/requirements.txt"
        ) from exc

    try:
        with Image.open(asset_path) as source:
            image = source.convert("RGBA")
    except OSError as exc:
        raise AssetGenError(f"unable to read {asset_path}: {exc}") from exc

    if fixed_size is not None and image.size != (fixed_size, fixed_size):
        image = image.resize((fixed_size, fixed_size), Image.Resampling.BICUBIC)

    width, height = image.size
    rgba = image.tobytes()
    pixels: list[int] = []
    for index in range(0, len(rgba), 4):
        red, green, blue, alpha = rgba[index], rgba[index + 1], rgba[index + 2], rgba[index + 3]
        luminance = (red * 30 + green * 59 + blue * 11) // 100
        pixels.append(1 if alpha >= 16 and luminance < 245 else 0)
    return width, height, pixels


def pack_bitmap(width: int, height: int, pixels: list[int]) -> BitmapAsset:
    stride_bytes = (width + 7) // 8
    packed: list[int] = []
    for row in range(height):
        row_start = row * width
        for byte_index in range(stride_bytes):
            value = 0
            for bit in range(8):
                col = byte_index * 8 + bit
                value <<= 1
                if col < width and pixels[row_start + col]:
                    value |= 0x01
            packed.append(value)
    return BitmapAsset(width=width, height=height, stride_bytes=stride_bytes, data=packed)


def emit_byte_array(values: list[int], indent: str) -> list[str]:
    if not values:
        return [f"{indent}0x00,"]
    lines: list[str] = []
    chunk_size = 12
    for start in range(0, len(values), chunk_size):
        chunk = values[start : start + chunk_size]
        lines.append(indent + ", ".join(f"0x{value:02X}" for value in chunk) + ",")
    return lines


def write_header(output_path: Path, specs: list[AssetSpec], *, guard: str, namespace: str) -> None:
    lines = [
        f"#ifndef {guard}",
        f"#define {guard}",
        "",
        '#include "asset_types.h"',
        "",
        f"namespace {namespace} {{",
        "",
    ]
    for spec in specs:
        lines.append(f"extern const EmbeddedImageAsset {spec.symbol};")
    lines.extend(
        [
            "",
            f"}}  // namespace {namespace}",
            "",
            f"#endif  // {guard}",
            "",
        ]
    )
    output_path.write_text("\n".join(lines), encoding="utf-8")


def write_source(
    output_path: Path,
    header_name: str,
    specs: list[AssetSpec],
    bitmaps: list[BitmapAsset],
    *,
    namespace: str,
) -> None:
    lines = [
        f'#include "{header_name}"',
        "",
        f"namespace {namespace} {{",
        "",
    ]

    for spec, bitmap in zip(specs, bitmaps):
        lines.append(f"const uint8_t {spec.symbol}_data[] = {{")
        lines.extend(emit_byte_array(bitmap.data, "    "))
        lines.append("};")
        lines.append("")
        lines.append(f"const EmbeddedImageAsset {spec.symbol} = {{")
        lines.append(f"    {spec.symbol}_data,")
        lines.append(f"    {bitmap.width},")
        lines.append(f"    {bitmap.height},")
        lines.append(f"    {bitmap.stride_bytes},")
        lines.append("    ImageFormat::kMono1,")
        lines.append("};")
        lines.append("")

    lines.extend([f"}}  // namespace {namespace}", ""])
    output_path.write_text("\n".join(lines), encoding="utf-8")


def build_bitmaps(specs: list[AssetSpec], *, fixed_size: int | None) -> list[BitmapAsset]:
    bitmaps: list[BitmapAsset] = []
    for spec in specs:
        width, height, pixels = load_png_pixels(spec.path, fixed_size=fixed_size)
        bitmaps.append(pack_bitmap(width, height, pixels))
    return bitmaps
