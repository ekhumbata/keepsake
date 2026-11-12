#!/usr/bin/env python3
"""Writes the release manifest (latest.json) the device reads before an update (#40).

    python3 tools/release/make_manifest.py --image keepsake-1.22.0.bin --version 1.22.0 \\
        --url https://github.com/<owner>/<repo>/releases/download/v1.22.0/keepsake-1.22.0.bin \\
        [--notes-file firmware/whats_new.txt] --out latest.json

The format is parsed by firmware/components/core/firmware_update (keep them in step):
version, url (https), size, sha256 (lowercase hex) and optional notes (one line, at most 240
bytes, shown on the device's "Keepsake updated" card).
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from pathlib import Path

MAX_NOTES_BYTES = 240
VERSION = re.compile(r"^(0|[1-9]\d{0,3})\.(0|[1-9]\d{0,3})\.(0|[1-9]\d{0,3})(-[0-9A-Za-z.-]+)?$")


def read_notes(path: Path | None) -> str:
    """The first non-comment paragraph of the notes file, joined into one line."""
    if path is None or not path.exists():
        return ""
    lines = []
    for line in path.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("#"):
            continue
        if not line:
            if lines:
                break
            continue
        lines.append(line)
    notes = " ".join(lines)
    if len(notes.encode("utf-8")) > MAX_NOTES_BYTES:
        raise SystemExit(f"{path}: notes are {len(notes.encode('utf-8'))} bytes; keep them under {MAX_NOTES_BYTES}")
    return notes


def make_manifest(image: Path, version: str, url: str, notes: str = "") -> dict:
    version = version.removeprefix("v")
    if not VERSION.match(version):
        raise SystemExit(f"not a version: {version!r}")
    if not url.startswith("https://") or " " in url:
        raise SystemExit(f"the image URL must be https: {url!r}")
    data = image.read_bytes()
    if not data:
        raise SystemExit(f"{image} is empty")
    manifest = {
        "version": version,
        "url": url,
        "size": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
    }
    if notes:
        manifest["notes"] = notes
    return manifest


def main(argv: list) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--image", type=Path, required=True)
    parser.add_argument("--version", required=True)
    parser.add_argument("--url", required=True)
    parser.add_argument("--notes-file", type=Path)
    parser.add_argument("--out", type=Path, required=True)
    options = parser.parse_args(argv)
    manifest = make_manifest(options.image, options.version, options.url, read_notes(options.notes_file))
    options.out.write_text(json.dumps(manifest, indent=2) + "\n")
    print(json.dumps(manifest, indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
