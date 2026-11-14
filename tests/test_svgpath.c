/* Asset checks for the scene-3 loader (run with the two asset files):
 *   #logo-mark  one <path> in the file, closed (Z), 256 arc-length samples,
 *               no step (incl. last -> first) > 1.5x the mean step
 *   #signature  open stroke traced out and back: same no-jump criterion on
 *               256 samples AND on the 1024 samples scene 3 traces, and the
 *               turnaround vertex is the stroke's end
 *   #idot       <circle> parsed as r = 1.1 at (68.4, 4.6) before the group
 *               transform; its 64-point loop passes the same criterion
 * Also checks that the group transform scale(1,-1) was applied. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "../src/svgpath.h"

#define JUMP 1.5

static int fails;

static void expect(int ok, const char *what)
{
    if (!ok) { printf("  FAIL: %s\n", what); fails++; }
}

/* resample to n and report max step / mean step, including last -> first */
static double jump_ratio(const SvgShape *s, int n, double *len)
{
    static Pt2 out[1024];
    *len = svg_resample(s->pts, s->n, out, n);
    double sum = 0, mx = 0;
    for (int k = 0; k < n; k++) {
        double d = hypot(out[(k + 1) % n].x - out[k].x, out[(k + 1) % n].y - out[k].y);
        sum += d;
        if (d > mx) mx = d;
    }
    return mx / (sum / n);
}

static int load(const char *file, const char *id, SvgShape *s)
{
    char err[128];
    if (svg_load_shapes(file, id, s, 1, err, sizeof err) != 1) {
        printf("%s#%s: FAIL load: %s\n", file, id, err);
        fails++;
        return -1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: test_svgpath logo-mark.svg signature.svg\n"); return 2; }
    const char *logo = argv[1], *sig = argv[2];
    SvgShape s;
    double len, r;

    char *text = svg_read_file(logo);
    int npaths = text ? svg_count_elements(text, "path") : -1;
    free(text);
    if (load(logo, "logo-mark", &s) == 0) {
        r = jump_ratio(&s, 256, &len);
        printf("#logo-mark: paths_in_file=%d subpaths=%d closed=%d segments=%d length=%.2f n=256 "
               "max/mean step=%.3f\n", npaths, s.nsub, s.closed, s.nseg, len, r);
        expect(npaths == 1, "exactly one <path> in the file");
        expect(s.nsub == 1 && s.closed && !s.out_and_back, "one closed subpath");
        expect(fabs(s.pts[0].x - s.pts[s.n - 1].x) < 1e-9 && fabs(s.pts[0].y - s.pts[s.n - 1].y) < 1e-9,
               "polyline closes on its start");
        expect(r <= JUMP, "max step <= 1.5x mean");
        /* scale(1,-1): raw start (5, 10) must land at (5, -10) */
        expect(fabs(s.pts[0].x - 5.0) < 1e-9 && fabs(s.pts[0].y + 10.0) < 1e-9, "y flipped by group transform");
        svg_shape_free(&s);
    }

    if (load(sig, "signature", &s) == 0) {
        r = jump_ratio(&s, 256, &len);
        double r1024 = jump_ratio(&s, 1024, &len);
        Pt2 t = s.pts[s.turn];
        printf("#signature: subpaths=%d closed(Z)=%d out_and_back=%d segments=%d raw_end=(%.2f, %.2f) "
               "turnaround=(%.2f, %.2f) length=%.2f max/mean step n=256: %.3f, n=1024: %.3f\n",
               s.nsub, s.closed, s.out_and_back, s.nseg, s.raw_end.x, s.raw_end.y, t.x, t.y, len, r, r1024);
        expect(s.nsub == 1 && !s.closed && s.out_and_back, "one open stroke, traced out and back");
        expect(fabs(t.x - s.end.x) < 1e-9 && fabs(t.y - s.end.y) < 1e-9, "turnaround == stroke end");
        expect(fabs(s.end.x - s.raw_end.x) < 1e-9 && fabs(s.end.y + s.raw_end.y) < 1e-9,
               "stroke end transformed by scale(1,-1)");
        expect(fabs(s.pts[0].x - s.pts[s.n - 1].x) < 1e-9 && fabs(s.pts[0].y - s.pts[s.n - 1].y) < 1e-9,
               "out-and-back returns to the start");
        expect(r <= JUMP, "max step <= 1.5x mean (n = 256)");
        expect(r1024 <= JUMP, "max step <= 1.5x mean (n = 1024, the scene-3 trace)");
        svg_shape_free(&s);
    }

    if (load(sig, "idot", &s) == 0) {
        r = jump_ratio(&s, 64, &len);
        printf("#idot: circle raw cx=%.2f cy=%.2f r=%.2f, first point after transform=(%.2f, %.2f) "
               "n=64 max/mean step=%.3f\n", s.cx, s.cy, s.r, s.pts[0].x, s.pts[0].y, r);
        expect(s.kind == SVG_CIRCLE, "is a circle");
        expect(fabs(s.cx - 68.4) < 1e-9 && fabs(s.cy - 4.6) < 1e-9 && fabs(s.r - 1.1) < 1e-9,
               "r = 1.1 at (68.4, 4.6) before the transform");
        expect(fabs(s.pts[0].x - 69.5) < 1e-9 && fabs(s.pts[0].y + 4.6) < 1e-9, "centre flipped to y = -4.6");
        expect(r <= JUMP, "max step <= 1.5x mean");
        svg_shape_free(&s);
    }

    /* whole-file load (what --svg FILE does): signature.svg -> stroke, then dot */
    SvgShape all[4];
    char err[128];
    int n = svg_load_shapes(sig, NULL, all, 4, err, sizeof err);
    printf("signature.svg whole file: %d shapes:", n);
    for (int i = 0; i < n; i++) printf(" #%s", all[i].id);
    printf("\n");
    expect(n == 2 && all[0].kind == SVG_PATH && all[1].kind == SVG_CIRCLE, "file order: path, circle");
    for (int i = 0; i < n; i++) svg_shape_free(&all[i]);

    printf("%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
