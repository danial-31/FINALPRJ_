"""Capture a procedural walk-cycle preview from the actual Release OpenGL renderer."""
from pathlib import Path
import math
import subprocess
from PIL import Image

root = Path(__file__).resolve().parents[1]
folder = root / "captures" / "animation-preview-v2"
folder.mkdir(parents=True, exist_ok=True)
frames = []
for i in range(24):
    path = folder / f"walk-{i:02}.ppm"
    subprocess.run([
        str(root / "build/Release/finalprj.exe"), "--view", "hero", "--motion",
        "--no-ui", "--time", str(i * (0.60 / 0.62 * 1.45 / 1.6) / 24), "--capture", str(path),
    ], check=True, stdout=subprocess.DEVNULL)
    with Image.open(path) as frame:
        frames.append(frame.resize((768, 480), Image.Resampling.LANCZOS).convert("RGB"))
    path.unlink()
frames[0].save(root / "captures/hero-walk.gif", save_all=True,
               append_images=frames[1:], duration=37, loop=0)
print("Saved captures/hero-walk.gif: 24 actual OpenGL frames")
