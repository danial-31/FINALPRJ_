"""Regenerate the ASCII HUD atlas; runtime and builds do not need Python.

Pass a local monospace TrueType font (the checked-in atlas uses Windows Consolas).
Only the rendered glyph image is shipped; no font software is redistributed.
"""
from pathlib import Path
import argparse
from PIL import Image, ImageDraw, ImageFont

parser = argparse.ArgumentParser()
parser.add_argument("font", type=Path)
args = parser.parse_args()
atlas = Image.new("RGBA", (384, 192))
draw = ImageDraw.Draw(atlas)
font = ImageFont.truetype(str(args.font), 24)
for code in range(32, 128):
    i = code - 32
    draw.text(((i % 16) * 24 + 3, (i // 16) * 32 + 1), chr(code), font=font, fill="white")
atlas.save(Path(__file__).resolve().parents[1] / "assets/combat-font.png")
