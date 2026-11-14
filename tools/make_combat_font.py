"""Regenerate the HUD glyph atlas (ASCII + Cyrillic); runtime and builds do not need Python.

Pass a local monospace TrueType font (the checked-in atlas uses Windows Consolas).
Only the rendered glyph image is shipped; no font software is redistributed.
The glyph order must match `glyphIndex()` in src/ui.hpp.
"""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont

CELL_W, CELL_H, COLS = 32, 44, 16
EXTRA = "—«»№•…←→↑↓×°·★"

def glyphs():
    out = [chr(c) for c in range(32, 128)]
    out += [chr(c) for c in range(0x410, 0x450)]
    out += ["Ё", "ё"]
    out += list(EXTRA)
    return out

parser = argparse.ArgumentParser()
parser.add_argument("font", type=Path)
args = parser.parse_args()
chars = glyphs()
rows = (len(chars) + COLS - 1) // COLS
atlas = Image.new("RGBA", (COLS * CELL_W, rows * CELL_H))
draw = ImageDraw.Draw(atlas)
font = ImageFont.truetype(str(args.font), 32)
for i, ch in enumerate(chars):
    draw.text(((i % COLS) * CELL_W + 7, (i // COLS) * CELL_H + 4), ch, font=font, fill="white")
atlas.save(Path(__file__).resolve().parents[1] / "assets/combat-font.png")
print(len(chars), "glyphs,", atlas.size)
