#!/usr/bin/env python3
"""Fail unless every screenshot PNG has real content.

Per PNG: the background is the most common colour. Pass requires
  * luminance standard deviation >= --min-std over the whole frame, and
  * >= --min-colours distinct non-background colours (quantised to 4 bits per
    channel so antialiasing does not count), each covering >= --min-pixels,
both over the whole frame AND over the central region (overlay text alone,
which sits at the edges, cannot pass).
"""
import argparse, glob, os, sys
from collections import Counter
from PIL import Image, ImageStat

def pixels(img):
    b = img.tobytes()                      # RGB, 3 bytes per pixel
    return zip(b[0::3], b[1::3], b[2::3])

def colours(img, bg, min_pixels):
    q = Counter(((r >> 4, g >> 4, b >> 4) for r, g, b in pixels(img)))
    bgq = (bg[0] >> 4, bg[1] >> 4, bg[2] >> 4)
    return [c for c, n in q.items() if c != bgq and n >= min_pixels]

def check(path, a):
    img = Image.open(path).convert('RGB')
    small = img.resize((img.width // 2, img.height // 2), Image.NEAREST)
    bg = Counter(pixels(small)).most_common(1)[0][0]
    std = ImageStat.Stat(img.convert('L')).stddev[0]
    full = colours(small, bg, a.min_pixels)
    w, h = small.size
    centre = small.crop((w // 6, h // 5, w * 5 // 6, h * 4 // 5))
    mid = colours(centre, bg, a.min_pixels)
    ok = std >= a.min_std and len(full) >= a.min_colours and len(mid) >= a.min_colours
    print('%s %s: %dx%d bg=%s luma_std=%.1f colours(full)=%d colours(centre)=%d'
          % ('PASS' if ok else 'FAIL', path, img.width, img.height, bg, std, len(full), len(mid)))
    return ok

ap = argparse.ArgumentParser()
ap.add_argument('dir')
ap.add_argument('--expect', type=int, default=6, help='number of PNGs required')
ap.add_argument('--min-std', type=float, default=4.0)
ap.add_argument('--min-colours', type=int, default=2)
ap.add_argument('--min-pixels', type=int, default=200)
a = ap.parse_args()
files = sorted(glob.glob(os.path.join(a.dir, '*.png')))
if len(files) < a.expect:
    sys.exit('FAIL: %d PNGs in %s, expected %d' % (len(files), a.dir, a.expect))
results = [check(f, a) for f in files]
sys.exit(0 if all(results) else 1)
