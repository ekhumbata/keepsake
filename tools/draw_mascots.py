#!/usr/bin/env python3
"""Draws Keepsake's two original mascots as 1-bit pixel art (#20).

Bookworm (round, smiley, big glasses, peeking over an open book) and Mochi (a squishy cloud with
tiny arms), plus festive versions for special days (#85). Placeholders, built from simple shapes
on a small pixel grid so they read as pixel art and stay easy to tweak; hand-drawn art can replace
any PNG later with the same name.

    python3 tools/draw_mascots.py          # writes assets/mascots/*.png (3x scale)
    python3 tools/draw_mascots.py --preview out.png   # one sheet with every sprite, for review

Then regenerate the C++ assets: python3 tools/generate_epaper_project_assets.py
"""
from __future__ import annotations

import argparse
import math
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "assets" / "mascots"
SCALE = 3


class Grid:
    def __init__(self, width: int, height: int):
        self.w, self.h = width, height
        self.ink = [[0] * width for _ in range(height)]

    def set(self, x: int, y: int, value: int = 1) -> None:
        if 0 <= x < self.w and 0 <= y < self.h:
            self.ink[y][x] = value

    def get(self, x: int, y: int) -> int:
        return self.ink[y][x] if 0 <= x < self.w and 0 <= y < self.h else 0

    def stamp(self, other: "Grid", ox: int, oy: int, clear: "Grid | None" = None) -> None:
        """Draws `other` at (ox, oy); `clear` (a mask) first wipes what it covers."""
        if clear is not None:
            for y in range(clear.h):
                for x in range(clear.w):
                    if clear.ink[y][x]:
                        self.set(ox + x, oy + y, 0)
        for y in range(other.h):
            for x in range(other.w):
                if other.ink[y][x]:
                    self.set(ox + x, oy + y, 1)


def shape(width: int, height: int, inside) -> tuple[Grid, Grid]:
    """A filled shape from a predicate -> (outline, fill mask)."""
    fill = Grid(width, height)
    for y in range(height):
        for x in range(width):
            if inside(x + 0.5, y + 0.5):
                fill.set(x, y)
    outline = Grid(width, height)
    for y in range(height):
        for x in range(width):
            if fill.get(x, y) and any(not fill.get(x + dx, y + dy)
                                      for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                outline.set(x, y)
    return outline, fill


def circle(cx: float, cy: float, r: float):
    return lambda x, y: (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def union(*preds):
    return lambda x, y: any(p(x, y) for p in preds)


def dots(grid: Grid, points) -> None:
    for x, y in points:
        grid.set(x, y)


def block(grid: Grid, x: int, y: int, w: int, h: int) -> None:
    for yy in range(y, y + h):
        for xx in range(x, x + w):
            grid.set(xx, yy)


# --- the two mascots ------------------------------------------------------------------------

def mochi(width: int = 40, height: int = 40, ox: int = 0, oy: int = 0) -> tuple[Grid, Grid]:
    """A squishy cloud with a face and tiny arms; body in the lower ~2/3."""
    body = union(circle(13, 24, 8), circle(22, 20, 10), circle(30, 25, 7),
                 lambda x, y: 6 <= x <= 35 and 25 <= y <= 33 and
                 ((x - 6) ** 2 + (y - 29) ** 2 <= 16 or x >= 10) and
                 ((x - 35) ** 2 + (y - 29) ** 2 <= 16 or x <= 31))
    outline, fill = shape(width, height, lambda x, y: body(x - ox, y - oy))
    g = Grid(width, height)
    g.stamp(outline, 0, 0)
    # Face: eyes, a small smile, blush.
    block(g, ox + 15, oy + 25, 2, 2)
    block(g, ox + 25, oy + 25, 2, 2)
    dots(g, [(ox + 19, oy + 28), (ox + 20, oy + 29), (ox + 21, oy + 29), (ox + 22, oy + 29),
             (ox + 23, oy + 28)])
    dots(g, [(ox + 13, oy + 28), (ox + 14, oy + 28), (ox + 28, oy + 28), (ox + 29, oy + 28)])
    # Tiny arms.
    dots(g, [(ox + 4, oy + 27), (ox + 3, oy + 26), (ox + 37, oy + 27), (ox + 38, oy + 26)])
    return g, fill


def bookworm(width: int = 40, height: int = 40, ox: int = 0, oy: int = 0) -> tuple[Grid, Grid]:
    """A round head with big glasses and a smile, peeking over an open book."""
    head_outline, head_fill = shape(width, height, lambda x, y: circle(20, 17, 12)(x - ox, y - oy))
    g = Grid(width, height)
    g.stamp(head_outline, 0, 0)
    # Glasses: two rings and a bridge.
    for cx in (15, 25):
        ring, _ = shape(width, height, lambda x, y, cx=cx: circle(cx, 16, 4.2)(x - ox, y - oy))
        g.stamp(ring, 0, 0)
    dots(g, [(ox + 19, oy + 15), (ox + 20, oy + 15), (ox + 21, oy + 15)])
    dots(g, [(ox + 7, oy + 15), (ox + 8, oy + 15), (ox + 32, oy + 15), (ox + 33, oy + 15)])  # arms of the glasses
    # Eyes (looking down at the book) and a smile.
    block(g, ox + 15, oy + 17, 2, 2)
    block(g, ox + 25, oy + 17, 2, 2)
    dots(g, [(ox + 18, oy + 23), (ox + 19, oy + 24), (ox + 20, oy + 24), (ox + 21, oy + 24),
             (ox + 22, oy + 23)])
    # A little tuft on top.
    dots(g, [(ox + 20, oy + 4), (ox + 19, oy + 3), (ox + 21, oy + 2), (ox + 22, oy + 3)])
    # The open book in front, covering the bottom of the head.
    book = Grid(width, height)
    book_mask = Grid(width, height)
    for x in range(4, 37):
        dip = abs(x - 20) // 6  # pages rise towards the edges
        top = 28 - dip
        for y in range(top, 37):
            book_mask.set(ox + x, oy + y)
        book.set(ox + x, oy + top)
        book.set(ox + x, oy + 36)
    for y in range(22, 37):
        if book_mask.get(ox + 4, oy + y):
            book.set(ox + 4, oy + y)
        if book_mask.get(ox + 36, oy + y):
            book.set(ox + 36, oy + y)
    for y in range(28, 37):
        book.set(ox + 20, oy + y)  # spine
    # A few lines of text on each page.
    for row in (31, 33):
        for x in list(range(8, 17, 2)) + list(range(23, 33, 2)):
            book.set(ox + x, oy + row)
    # Little hands holding the book.
    dots(book, [(ox + 3, oy + 29), (ox + 2, oy + 30), (ox + 37, oy + 29), (ox + 38, oy + 30)])
    g.stamp(book, 0, 0, clear=book_mask)
    full_fill = Grid(width, height)
    full_fill.stamp(head_fill, 0, 0)
    full_fill.stamp(book_mask, 0, 0)
    return g, full_fill


# --- props ------------------------------------------------------------------------------------

def heart(filled: bool = True) -> Grid:
    rows = [
        ".##.##.",
        "#######",
        "#######",
        ".#####.",
        "..###..",
        "...#...",
    ] if filled else [
        ".##.##.",
        "#..#..#",
        "#.....#",
        ".#...#.",
        "..#.#..",
        "...#...",
    ]
    return from_rows(rows)


def sparkle() -> Grid:
    return from_rows(["..#..", "..#..", "##.##", "..#..", "..#.."])


def snowflake() -> Grid:
    return from_rows(["#.#.#", ".###.", "##.##", ".###.", "#.#.#"])


def party_hat() -> Grid:
    return from_rows([
        "....##....",
        "...####...",
        "....##....",
        "....##....",
        "...#..#...",
        "...#.##...",
        "..#.##.#..",
        "..###..#..",
        ".#....##..",
        ".#..##..#.",
        "#.##....##",
        "##########",
    ])


def santa_hat() -> Grid:
    return from_rows([
        "..............###",
        "...........##.###",
        ".........######..",
        ".......#########.",
        ".....############",
        "....#############",
        "...##############",
        "..################",
        ".##################",
        "#.................#",
        "#.#.#.#.#.#.#.#.#.#",
        "###################",
    ])


def flower() -> Grid:
    return from_rows([
        ".###.",
        "#.#.#",
        "##.##",
        "#.#.#",
        ".###.",
        "..#..",
        "..#.#",
        "..##.",
        "..#..",
    ])


def burst() -> Grid:
    return from_rows([
        "#...#...#",
        ".#..#..#.",
        "..#...#..",
        ".........",
        "###...###",
        ".........",
        "..#...#..",
        ".#..#..#.",
        "#...#...#",
    ])


def pumpkin() -> Grid:
    return from_rows([
        "......##.....",
        ".....##......",
        "..#########..",
        ".#...#.#...#.",
        "#...#...#...#",
        "#.##.#.#.##.#",
        "#...#...#...#",
        "#...#####...#",
        ".#...#.#...#.",
        "..#########..",
    ])


def bat() -> Grid:
    return from_rows([
        "#.........#",
        "##..#.#..##",
        "###.###.###",
        ".#########.",
        "..#.###.#..",
        "......#....",
    ])


def from_rows(rows: list[str]) -> Grid:
    g = Grid(max(len(r) for r in rows), len(rows))
    for y, row in enumerate(rows):
        for x, c in enumerate(row):
            if c == "#":
                g.set(x, y)
    return g


# --- the sprite set -----------------------------------------------------------------------

def compose() -> dict[str, Grid]:
    sprites: dict[str, Grid] = {}

    g, _ = mochi()
    g.stamp(sparkle(), 3, 6)
    g.stamp(sparkle(), 33, 3)
    sprites["mochi_happy"] = g

    g, _ = bookworm()
    sprites["bookworm_reading"] = g

    # Birthday: Mochi in a party hat, confetti around.
    g, _ = mochi()
    g.stamp(party_hat(), 17, 0, clear=None)
    dots(g, [(4, 6), (8, 12), (31, 4), (35, 10), (2, 16), (37, 15), (29, 9)])
    sprites["day_birthday"] = g

    # Christmas: Mochi in a Santa hat, snow falling.
    g, fill = mochi()
    hat = santa_hat()
    mask = Grid(hat.w, hat.h)
    for y in range(hat.h):
        for x in range(hat.w):
            mask.set(x, y, 1 if any(hat.ink[y][xx] for xx in range(0, x + 1)) and
                     any(hat.ink[y][xx] for xx in range(x, hat.w)) else 0)
    g.stamp(hat, 12, 3, clear=mask)
    g.stamp(snowflake(), 2, 4)
    g.stamp(snowflake(), 34, 14)
    g.stamp(snowflake(), 4, 16)
    sprites["day_christmas"] = g

    # New Year: Mochi under fireworks.
    g, _ = mochi()
    g.stamp(burst(), 1, 1)
    g.stamp(burst(), 29, 0)
    g.stamp(sparkle(), 17, 2)
    sprites["day_new_year"] = g

    # Valentine's: Mochi with hearts floating up.
    g, _ = mochi()
    g.stamp(heart(True), 30, 3)
    g.stamp(heart(False), 4, 6)
    g.stamp(heart(True), 15, 0)
    sprites["day_valentines"] = g

    # Anniversary: Mochi and Bookworm together, a heart between them.
    g = Grid(80, 40)
    worm, _ = bookworm()
    cloud, _ = mochi()
    g.stamp(worm, 0, 0)
    g.stamp(cloud, 40, 0)
    big = Grid(14, 12)
    for y in range(12):
        for x in range(14):
            if heart(True).get(x // 2, y // 2):
                big.set(x, y)
    g.stamp(big, 33, 1)
    sprites["day_anniversary"] = g

    # Hello (onboarding, the welcome card): the two together, Mochi waving.
    g = Grid(80, 40)
    worm, _ = bookworm()
    cloud, _ = mochi()
    g.stamp(worm, 0, 0)
    g.stamp(cloud, 40, 0)
    # The raised arm and two little motion marks.
    dots(g, [(77, 26), (78, 25), (78, 24), (79, 23), (76, 18), (77, 17), (79, 19), (79, 20)])
    g.stamp(sparkle(), 34, 3)
    g.stamp(sparkle(), 46, 6)
    sprites["hello"] = g

    # First date: Bookworm with a flower, a heart above.
    g, _ = bookworm()
    g.stamp(flower(), 33, 14)
    g.stamp(heart(False), 31, 2)
    sprites["day_first_date"] = g

    # Halloween: Mochi with a little pumpkin.
    g, _ = mochi()
    g.stamp(pumpkin(), 14, 0)
    g.stamp(bat(), 1, 3)
    g.stamp(bat(), 29, 6)
    sprites["day_halloween"] = g

    # Charging (#84): Mochi having a snack of electricity, a bolt above.
    g, _ = mochi()
    g.stamp(from_rows([
        "....##",
        "...##.",
        "..##..",
        ".#####",
        "...##.",
        "..##..",
        ".##...",
        ".#....",
    ]), 18, 0)
    dots(g, [(5, 8), (34, 6), (36, 12)])
    sprites["charging"] = g

    # Sleepy (#84, low battery): Mochi with closed eyes, a "z z" drifting up.
    g, _ = mochi()
    for x, y in ((15, 25), (16, 25), (15, 26), (16, 26), (25, 25), (26, 25), (25, 26), (26, 26)):
        g.set(x, y, 0)
    dots(g, [(14, 26), (15, 26), (16, 26), (17, 26), (24, 26), (25, 26), (26, 26), (27, 26)])
    z_big = from_rows(["#####", "...#.", "..#..", ".#...", "#####"])
    z_small = from_rows(["###", "..#", ".#.", "#..", "###"])
    g.stamp(z_big, 33, 2)
    g.stamp(z_small, 27, 0)
    sprites["sleepy"] = g

    # Any other special day: Mochi with sparkles all around.
    g, _ = mochi()
    for x, y in ((2, 4), (16, 1), (33, 2), (36, 13), (1, 15)):
        g.stamp(sparkle(), x, y)
    sprites["day_celebrate"] = g
    return sprites


def to_image(grid: Grid, scale: int):
    from PIL import Image

    img = Image.new("L", (grid.w * scale, grid.h * scale), 255)
    px = img.load()
    for y in range(grid.h):
        for x in range(grid.w):
            if grid.ink[y][x]:
                for dy in range(scale):
                    for dx in range(scale):
                        px[x * scale + dx, y * scale + dy] = 0
    return img


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--preview", type=Path, help="write one review sheet instead")
    options = parser.parse_args()
    sprites = compose()
    if options.preview:
        from PIL import Image

        gap = 8
        images = [(name, to_image(g, 4)) for name, g in sprites.items()]
        width = sum(i.width for _, i in images) + gap * (len(images) + 1)
        height = max(i.height for _, i in images) + 2 * gap
        sheet = Image.new("L", (width, height), 200)
        x = gap
        for _, image in images:
            sheet.paste(image, (x, gap))
            x += image.width + gap
        sheet.save(options.preview)
        print(f"wrote {options.preview}: " + ", ".join(sprites))
        return 0
    OUT.mkdir(parents=True, exist_ok=True)
    for name, grid in sprites.items():
        to_image(grid, SCALE).save(OUT / f"{name}.png")
    print(f"wrote {len(sprites)} sprites to {OUT}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
