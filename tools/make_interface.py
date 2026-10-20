"""Rebuild the transparent controls label (Pillow, Windows fonts)."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1] / "assets"
root.mkdir(exist_ok=True)
fonts = Path("C:/Windows/Fonts")
footer = Image.new("RGBA", (1450, 48))
d = ImageDraw.Draw(footer)
d.line((0, 0, 1430, 0), fill=(120, 133, 136, 55))
d.text((0, 17), "WASD  move    SHIFT  run    RMB  orbit    WHEEL  zoom    1 / 2 / 3  views    G  blocked cells    SPACE  arcane pulse    R  reset    TAB  interface    F12  capture    ESC  exit",
       font=ImageFont.truetype(str(fonts / "segoeui.ttf"), 14), fill=(165, 173, 174, 240))
footer.save(root / "controls.png")
