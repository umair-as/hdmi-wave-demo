#!/usr/bin/env python3
"""Check that an SVG path can be traced by one run of epicycles without a jump.

Selects the path by --id (default: the only <path>). Exactly one M is allowed.
A closed path (ends with Z) is traced as is; an open stroke (no Z) is traced
forward then back, which closes it without a jump. After resampling to N points
evenly by arc length, no step (including last -> first) may exceed JUMP x the
mean step. Uniform transforms (e.g. a y flip) do not change these ratios, so the
path data is checked in its own frame.
Optionally renders previews (PIL): --png OUT.png [--size PX] [--points].
"""
import argparse, math, re, sys

def parse(svg, pid=None):
    if pid:
        paths = re.findall(r'<path[^>]*\sid="%s"[^>]*\sd="([^"]+)"' % re.escape(pid), svg)
    else:
        paths = re.findall(r'<path[^>]*\sd="([^"]+)"', svg)
    if len(paths) != 1:
        sys.exit('FAIL: expected exactly one matching <path>, found %d' % len(paths))
    toks = re.findall(r'[MLCZmlczHhVvQqSsTtAa]|-?\d*\.?\d+(?:e-?\d+)?', paths[0])
    segs, i, cur, start, nM, closed = [], 0, None, None, 0, False
    while i < len(toks):
        c = toks[i]; i += 1
        if c == 'M':
            nM += 1; cur = start = (float(toks[i]), float(toks[i + 1])); i += 2
        elif c == 'L':
            p = (float(toks[i]), float(toks[i + 1])); i += 2
            segs.append(('L', cur, p)); cur = p
        elif c == 'C':
            v = [float(t) for t in toks[i:i + 6]]; i += 6
            p = (v[4], v[5]); segs.append(('C', cur, (v[0], v[1]), (v[2], v[3]), p)); cur = p
        elif c == 'Z':
            closed = True
            if i != len(toks): sys.exit('FAIL: content after Z (separate pieces)')
        else:
            sys.exit('FAIL: unsupported command %r (only absolute M/L/C/Z)' % c)
    if nM != 1: sys.exit('FAIL: %d M commands (subpaths); need exactly 1' % nM)
    gap = math.dist(cur, start)
    if closed and gap > 1e-6:
        segs.append(('L', cur, start))      # Z closes with a straight line
    return segs, start, gap, closed

def flatten(segs, n=64):
    pts = [segs[0][1]]
    for s in segs:
        if s[0] == 'L':
            pts.append(s[2])
        else:
            _, p0, p1, p2, p3 = s
            for k in range(1, n + 1):
                t = k / n; u = 1 - t
                pts.append((u**3*p0[0] + 3*u*u*t*p1[0] + 3*u*t*t*p2[0] + t**3*p3[0],
                            u**3*p0[1] + 3*u*u*t*p1[1] + 3*u*t*t*p2[1] + t**3*p3[1]))
    return pts

def resample(pts, n):
    cum = [0.0]
    for a, b in zip(pts, pts[1:]): cum.append(cum[-1] + math.dist(a, b))
    total, out, j = cum[-1], [], 0
    for k in range(n):
        s = total * k / n
        while cum[j + 1] < s: j += 1
        seg = cum[j + 1] - cum[j]
        t = 0 if seg == 0 else (s - cum[j]) / seg
        out.append((pts[j][0] + t * (pts[j + 1][0] - pts[j][0]), pts[j][1] + t * (pts[j + 1][1] - pts[j][1])))
    return out, total

ap = argparse.ArgumentParser()
ap.add_argument('svg'); ap.add_argument('--id'); ap.add_argument('-n', type=int, default=256)
ap.add_argument('--jump', type=float, default=1.5)
ap.add_argument('--png'); ap.add_argument('--size', type=int, default=512)
ap.add_argument('--fill', action='store_true'); ap.add_argument('--points', action='store_true')
a = ap.parse_args()
svg = open(a.svg).read()
segs, start, gap, closed = parse(svg, a.id)
dense = flatten(segs)
if not closed:                      # open stroke: out and back, closes without a jump
    dense = dense + dense[-2::-1]
pts, total = resample(dense, a.n)
steps = [math.dist(pts[k], pts[(k + 1) % a.n]) for k in range(a.n)]
mean, mx = sum(steps) / a.n, max(steps)
worst = steps.index(mx)
print('%s#%s: %s segments=%d traced_length=%.2f n=%d mean_step=%.4f max_step=%.4f (%.3fx, at %d->%d) end_to_start=%.3f'
      % (a.svg, a.id or '', 'closed' if closed else 'open, traced out-and-back', len(segs), total, a.n,
         mean, mx, mx / mean, worst, (worst + 1) % a.n, gap))
ok = mx <= a.jump * mean
print('PASS' if ok else 'FAIL: step %.2fx mean exceeds %.2fx' % (mx / mean, a.jump))

if a.png:
    from PIL import Image, ImageDraw
    vb = [float(v) for v in re.search(r'viewBox="([^"]+)"', svg).group(1).split()]
    ss = 4; W = a.size * ss; sc = W / max(vb[2], vb[3]); H = int(vb[3] * sc)
    img = Image.new('RGB', (W, H), (12, 16, 24)); dr = ImageDraw.Draw(img)
    P = [((x - vb[0]) * sc, (y - vb[1]) * sc) for x, y in dense]
    if a.fill:
        dr.polygon(P, fill=(47, 211, 168), outline=(26, 143, 113))
    else:
        dr.line(P + [P[0]], fill=(47, 211, 168), width=max(1, int(3 * sc / 2)), joint='curve')
    if a.points:
        for x, y in pts:
            X, Y = (x - vb[0]) * sc, (y - vb[1]) * sc; r = 2 * ss
            dr.ellipse((X - r, Y - r, X + r, Y + r), fill=(255, 120, 200))
    img.resize((a.size, int(H / ss)), Image.LANCZOS).save(a.png)
sys.exit(0 if ok else 1)
