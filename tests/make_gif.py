"""Frames (PNG) -> an optimised looping GIF with Pillow: make_gif.py <frames dir> <out.gif> [fps] [width]."""
import glob
import os
import sys

from PIL import Image


def main(src, out, fps=12, width=800):
    frames = []
    for path in sorted(glob.glob(os.path.join(src, "*.png"))):
        im = Image.open(path).convert("RGB")
        im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
        frames.append(im.quantize(colors=128, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG))
    frames[0].save(out, save_all=True, append_images=frames[1:], duration=round(1000 / fps), loop=0, optimize=True)


if __name__ == "__main__":
    a = sys.argv[1:]
    main(a[0], a[1], int(a[2]) if len(a) > 2 else 12, int(a[3]) if len(a) > 3 else 800)
