#!/usr/bin/env python3
"""Prepares photos for Keepsake's SD card (#136): the right way up, 960 px on the long side,
plain (baseline) JPEG, named so the device shows them in order with a caption.

    pip install -r tools/requirements.txt          # Pillow (+ pillow-heif for iPhone HEIC)
    python3 tools/photos/prepare_photos.py ~/Pictures/us /Volumes/KEEPSAKE/photos

The output name is "<date> <title>.jpg", e.g. "2019-05-20 First date.jpg". The date comes from
the file name if it has one, else from the photo itself (EXIF). The title is the rest of the
name, unless it's a camera name like IMG_1234. Keepsake sorts oldest first and shows the
caption "First date, May 20, 2019". Rename the outputs to change either.

Personal photos must never be committed: the repo is public. This tool refuses to write
anywhere inside the repository.
"""
from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
LONG_SIDE = 960
QUALITY = 85
INPUT_SUFFIXES = {".jpg", ".jpeg", ".png", ".gif", ".heic", ".heif", ".webp", ".tif", ".tiff", ".bmp"}
DATE_IN_NAME = re.compile(r"^(\d{4})[-_.]?(\d{2})[-_.]?(\d{2})(?!\d)[\s_.-]*(.*)$")
CAMERA_NAME = re.compile(r"^(img|dsc|dscn|dscf|pxl|mvimg|pano|vid|screenshot|photo|image|p|dji|gopr|sam|wp)?"
                         r"[\d\s_.()~-]*$", re.IGNORECASE)
UNSAFE = re.compile(r'[<>:"/\\|?*\x00-\x1f]')


def load_heif_support() -> None:
    try:
        import pillow_heif  # type: ignore

        pillow_heif.register_heif_opener()
    except ImportError:
        pass


def inside_repo(path: Path) -> bool:
    try:
        path.resolve().relative_to(REPO)
        return True
    except ValueError:
        return False


def valid_date(y: int, m: int, d: int) -> bool:
    import datetime

    try:
        datetime.date(y, m, d)
        return 1900 <= y <= 2100
    except ValueError:
        return False


def exif_date(image) -> str:
    """'2019-05-20' from DateTimeOriginal (or DateTime), else ''."""
    exif = image.getexif()
    value = exif.get_ifd(0x8769).get(0x9003) or exif.get(0x0132) or ""
    match = re.match(r"(\d{4}):(\d{2}):(\d{2})", str(value))
    if match and valid_date(*map(int, match.groups())):
        return "-".join(match.groups())
    return ""


def output_name(source: Path, taken: str) -> str:
    stem = source.stem
    date = ""
    title = stem
    match = DATE_IN_NAME.match(stem)
    if match and valid_date(int(match[1]), int(match[2]), int(match[3])):
        date = f"{match[1]}-{match[2]}-{match[3]}"
        title = match[4]
    if CAMERA_NAME.match(title):
        title = ""
    title = UNSAFE.sub("", title.replace("_", " ")).strip(" .-")
    title = re.sub(r"\s+", " ", title)[:80]
    date = date or taken
    name = " ".join(part for part in (date, title) if part) or stem
    return name + ".jpg"


def prepare(source: Path, out_dir: Path) -> Path:
    from PIL import Image, ImageOps

    with Image.open(source) as image:
        taken = exif_date(image)
        upright = ImageOps.exif_transpose(image)
        if upright.mode not in ("RGB", "L"):
            background = Image.new("RGB", upright.size, "white")
            rgba = upright.convert("RGBA")
            background.paste(rgba, mask=rgba.split()[-1])
            upright = background
        upright.thumbnail((LONG_SIDE, LONG_SIDE), Image.LANCZOS)
        target = out_dir / output_name(source, taken)
        counter = 2
        while target.exists():
            target = out_dir / f"{target.stem.rsplit(' (', 1)[0]} ({counter}).jpg"
            counter += 1
        # Baseline (not progressive) and no EXIF: small, upright, and readable by the device.
        upright.save(target, "JPEG", quality=QUALITY, optimize=True, progressive=False)
    return target


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", type=Path, help="a folder of photos (searched recursively) or one photo")
    parser.add_argument("out", type=Path, help="where to write them, e.g. the SD card's photos folder")
    options = parser.parse_args(argv)

    if inside_repo(options.out):
        print(f"Refusing to write into the repository ({options.out}): it's public. "
              "Write to the SD card or another folder.", file=sys.stderr)
        return 2
    load_heif_support()
    sources = [options.source] if options.source.is_file() else sorted(
        p for p in options.source.rglob("*") if p.suffix.lower() in INPUT_SUFFIXES and not p.name.startswith("."))
    if not sources:
        print(f"No photos found in {options.source}", file=sys.stderr)
        return 1
    options.out.mkdir(parents=True, exist_ok=True)
    failed = 0
    for source in sources:
        try:
            target = prepare(source, options.out)
            print(f"{source.name} -> {target.name}")
        except Exception as error:  # one bad file shouldn't stop the rest
            failed += 1
            hint = " (install pillow-heif for HEIC)" if source.suffix.lower() in (".heic", ".heif") else ""
            print(f"skipped {source.name}: {error}{hint}", file=sys.stderr)
    print(f"{len(sources) - failed} photo(s) ready in {options.out}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
