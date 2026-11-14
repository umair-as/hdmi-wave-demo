/* Scene 3: Fourier epicycles. Each shape is resampled to its run's N points
 * by arc length, transformed with an FFT, and redrawn by the rotating
 * vectors c_k e^{i f_k theta}, largest first, adding vectors one by one.
 * Shapes run one after another. Each SVG file gets its own band (the second
 * below the first) and its shapes share that band's frame, so the i-dot
 * lands on its stroke.
 *
 * Default (no --svg): the project assets, mark on top, signature below:
 *   #logo-mark  closed path,                          N = 256
 *   #signature  open stroke traced out and back,      N = 1024
 *   #idot       circle, a short second pass,          N = 64 */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "app.h"
#include "draw.h"
#include "fft.h"
#include "rlgl.h"
#include "scene.h"
#include "svgpath.h"

#define NMAX 1024                 /* largest samples per run (power of two) */
#define N_CLOSED 256              /* --svg defaults by shape kind */
#define N_OPEN 1024
#define N_CIRCLE 64
#define MAX_RUNS 8
#define REV_S 6.0f                /* seconds per pen revolution at N = 256 */
#define TRAIL 0.85f               /* fraction of a revolution kept as trail */
#define MIN_CIRCLE_PX 0.5f        /* circles smaller than this are not drawn */

/* The default runs: file in the asset dir, element id, samples per run. */
static const struct { const char *file, *id; int n; } default_runs[] = {
    { "logo-mark.svg", "logo-mark", 256 },
    { "signature.svg", "signature", 1024 },
    { "signature.svg", "idot", 64 },
};

typedef struct Term { double complex c; int f; double r; } Term;

typedef struct Run {
    char label[64];
    int group;                    /* band; runs of one file share a band and a frame */
    int n;                        /* samples (power of two <= NMAX): the per-run knob */
    Pt2 shape[NMAX];              /* resampled, in the group's normalised frame */
    Term term[NMAX];              /* sorted by radius, largest first */
    double complex w[NMAX];       /* e^{2 pi i j / n} */
    double complex curve[NMAX];   /* partial sum of the first `have` terms at theta_j */
    int have;
    float start, dur, rev;        /* scene-time window, pen revolution period */
} Run;

typedef struct Group { double ex, ey; } Group;   /* half extents after normalisation */

static Run runs[MAX_RUNS];
static Group groups[MAX_RUNS];
static int nruns, ngroups;
static float t;

static int cmp_term(const void *a, const void *b)
{
    double ra = ((const Term *)a)->r, rb = ((const Term *)b)->r;
    return (ra < rb) - (ra > rb);
}

/* Fallback when no shape could be loaded. */
static void heart(Pt2 *dense, int m)
{
    for (int i = 0; i < m; i++) {
        double a = 2 * M_PI * i / (m - 1), s = sin(a);
        dense[i] = (Pt2){ 16 * s * s * s,
                          -(13 * cos(a) - 5 * cos(2 * a) - 2 * cos(3 * a) - cos(4 * a)) };
    }
}

static void add_run(const Pt2 *dense, int m, int n, int group, const char *label)
{
    if (nruns == MAX_RUNS) return;
    Run *r = &runs[nruns++];
    memset(r, 0, sizeof *r);
    r->n = n;
    r->group = group;
    snprintf(r->label, sizeof r->label, "%.63s", label);
    svg_resample(dense, m, r->shape, n);
}

/* Normalise each group into a box whose larger side is 2, run the FFTs,
 * and lay the runs out in time. */
static void finish_runs(void)
{
    static double complex z[NMAX];
    for (int g = 0; g < ngroups; g++) {
        double minx = 1e30, maxx = -1e30, miny = 1e30, maxy = -1e30;
        for (int i = 0; i < nruns; i++) {
            if (runs[i].group != g) continue;
            for (int j = 0; j < runs[i].n; j++) {
                minx = fmin(minx, runs[i].shape[j].x); maxx = fmax(maxx, runs[i].shape[j].x);
                miny = fmin(miny, runs[i].shape[j].y); maxy = fmax(maxy, runs[i].shape[j].y);
            }
        }
        double cx = (minx + maxx) / 2, cy = (miny + maxy) / 2;
        double s = 2.0 / fmax(fmax(maxx - minx, maxy - miny), 1e-9);
        groups[g] = (Group){ (maxx - minx) * s / 2, (maxy - miny) * s / 2 };
        for (int i = 0; i < nruns; i++) {
            Run *r = &runs[i];
            if (r->group != g) continue;
            for (int j = 0; j < r->n; j++) {
                r->shape[j].x = (r->shape[j].x - cx) * s;
                r->shape[j].y = (r->shape[j].y - cy) * s;
                z[j] = r->shape[j].x + I * r->shape[j].y;
                r->w[j] = cexp(2 * M_PI * I * j / r->n);
            }
            fft_inplace(z, r->n);
            for (int k = 0; k < r->n; k++) {
                int f = k < r->n / 2 ? k : k - r->n;     /* signed frequency */
                r->term[k] = (Term){ z[k] / r->n, f, cabs(z[k]) / r->n };
            }
            qsort(r->term, (size_t)r->n, sizeof r->term[0], cmp_term);
        }
    }
    /* time per run grows with sqrt(N): 256/1024/64 -> 12.9/25.7/6.4 s of 45 s */
    double total = 0;
    for (int i = 0; i < nruns; i++) total += sqrt(runs[i].n);
    float at = 0;
    for (int i = 0; i < nruns; i++) {
        runs[i].start = at;
        runs[i].dur = (float)(g_app.cycle * sqrt(runs[i].n) / total);
        runs[i].rev = REV_S * sqrtf(runs[i].n / 256.0f);
        at += runs[i].dur;
    }
}

/* Asset directory: the source tree when the binary runs from its build tree
 * (developer builds only), otherwise the installed one. */
static const char *asset_dir(void)
{
#if defined(HWD_SOURCE_ASSET_DIR) && defined(HWD_BUILD_DIR)
    char exe[1024];
    ssize_t l = readlink("/proc/self/exe", exe, sizeof exe - 1);
    if (l > 0) {
        exe[l] = 0;
        if (!strncmp(exe, HWD_BUILD_DIR "/", strlen(HWD_BUILD_DIR) + 1)) return HWD_SOURCE_ASSET_DIR;
    }
#endif
    return HWD_ASSET_DIR;
}

static int load_file(const char *file, const char *id, int force_n)
{
    SvgShape sh[8];
    char err[128];
    int n = svg_load_shapes(file, id, sh, 8, err, sizeof err);
    if (n < 0) {
        fprintf(stderr, "hdmi-wave-demo: %s%s%s: %s (skipped)\n", file, id ? "#" : "", id ? id : "", err);
        return -1;
    }
    for (int k = 0; k < n; k++) {
        int samples = force_n ? force_n
                    : sh[k].kind == SVG_CIRCLE ? N_CIRCLE : sh[k].out_and_back ? N_OPEN : N_CLOSED;
        add_run(sh[k].pts, sh[k].n, samples, ngroups, sh[k].id);
        svg_shape_free(&sh[k]);
    }
    return n;
}

static void init(void)
{
    t = 0;
    nruns = ngroups = 0;
    if (g_app.nsvg) {
        /* --svg FILE loads every <path>/<circle> of FILE in order; FILE#ID one element */
        for (int i = 0; i < g_app.nsvg; i++) {
            char file[1024];
            snprintf(file, sizeof file, "%s", g_app.svg[i]);
            char *hash = strrchr(file, '#');
            if (hash) *hash = 0;
            if (load_file(file, hash ? hash + 1 : NULL, 0) > 0) ngroups++;
        }
    } else {
        const char *dir = asset_dir(), *prev = NULL;
        for (size_t i = 0; i < sizeof default_runs / sizeof default_runs[0]; i++) {
            if (prev && strcmp(prev, default_runs[i].file)) ngroups++;   /* new file: new band */
            prev = default_runs[i].file;
            char file[1024];
            snprintf(file, sizeof file, "%s/%s", dir, default_runs[i].file);
            load_file(file, default_runs[i].id, default_runs[i].n);
        }
        if (nruns) ngroups++;
        printf("hdmi-wave-demo: epicycles: %d runs from %s\n", nruns, dir);
        fflush(stdout);
    }
    if (nruns == 0) {
        static Pt2 dense[1025];
        heart(dense, 1025);
        add_run(dense, 1025, N_CLOSED, 0, "built-in heart");
        ngroups = 1;
    }
    finish_runs();
}

/* terms visible at local time lt: grows exponentially 1 -> n over 85% of the run */
static int terms_at(const Run *r, float lt)
{
    float u = lt / (0.85f * r->dur);
    if (u <= 0) return 1;
    if (u >= 1) return r->n;
    int n = (int)lroundf(powf((float)r->n, u));
    return n < 1 ? 1 : n > r->n ? r->n : n;
}

static void grow(Run *r, int want)
{
    for (; r->have < want; r->have++) {
        const Term *tm = &r->term[r->have];
        long f = ((tm->f % r->n) + r->n) % r->n;
        for (int j = 0; j < r->n; j++)             /* e^{i f theta_j}, index reduced exactly */
            r->curve[j] += tm->c * r->w[(f * j) % r->n];
    }
}

static void update(float dt)
{
    t += dt;
    for (int i = 0; i < nruns; i++) {
        float lt = t - runs[i].start;
        if (lt >= 0) grow(&runs[i], terms_at(&runs[i], lt));
    }
}

static Vector2 to_screen(double complex z, Vector2 c, float s)
{
    return (Vector2){ c.x + (float)creal(z) * s, c.y + (float)cimag(z) * s };
}

/* Epicycle chain at angle theta. Circles below MIN_CIRCLE_PX are skipped,
 * segment counts follow the radius, and every circle and thin arm goes into
 * consecutive RL_LINES blocks, which rlgl merges into one draw call per batch. */
static void draw_chain(const Run *r, double theta, Vector2 c, float sc, float k)
{
    static Vector2 joint[NMAX + 1];
    double complex p = 0;
    Color cc = Fade(COL_BLUE, 0.30f), ca = Fade(COL_AMBER, 0.85f);
    joint[0] = to_screen(0, c, sc);
    for (int q = 0; q < r->have; q++) {
        const Term *tm = &r->term[q];
        p += tm->c * cexp(I * tm->f * theta);
        joint[q + 1] = to_screen(p, c, sc);
        Vector2 a = joint[q], b = joint[q + 1];
        float rad = (float)tm->r * sc;
        int seg = 0;
        if (rad >= MIN_CIRCLE_PX && tm->f != 0) {
            seg = (int)(rad * 0.5f);
            seg = seg < 8 ? 8 : seg > 72 ? 72 : seg;
        }
        bool arm = q >= 8 && rad >= 0.3f;
        if (!seg && !arm) continue;
        rlCheckRenderBatchLimit(2 * seg + 2);
        rlBegin(RL_LINES);
        if (seg) {
            rlColor4ub(cc.r, cc.g, cc.b, cc.a);
            float px = a.x + rad, py = a.y;
            for (int s = 1; s <= seg; s++) {
                float an = 2 * PI * s / seg, nx = a.x + rad * cosf(an), ny = a.y + rad * sinf(an);
                rlVertex2f(px, py);
                rlVertex2f(nx, ny);
                px = nx; py = ny;
            }
        }
        if (arm) {
            rlColor4ub(ca.r, ca.g, ca.b, ca.a);
            rlVertex2f(a.x, a.y);
            rlVertex2f(b.x, b.y);
        }
        rlEnd();
    }
    /* the biggest arms thick, on top */
    for (int q = 0; q < r->have && q < 8; q++) DrawLineEx(joint[q], joint[q + 1], 2.5f * k, ca);
    DrawCircleV(joint[r->have], 7 * k, COL_PINK);
}

static void draw(int w, int h)
{
    float k = draw_scale(h);
    float top = h * 0.13f, band = (h * 0.95f - top) / ngroups;
    float half_w = w * 0.32f, cx = w * 0.62f;          /* labels live left of 0.28 w */
    static Vector2 pts[NMAX + 1];
    for (int i = 0; i < nruns; i++) {
        Run *r = &runs[i];
        const Group *g = &groups[r->group];
        float sc = fminf(half_w / (float)fmax(g->ex, 1e-3), 0.36f * band / (float)fmax(g->ey, 1e-3));
        int n = r->n;
        float lt = t - r->start;
        Vector2 c = { cx, top + band * (r->group + 0.5f) };
        for (int j = 0; j <= n; j++) pts[j] = to_screen(r->shape[j % n].x + I * r->shape[j % n].y, c, sc);
        draw_polyline(pts, n + 1, 2 * k, COL_GRID);              /* target, faint */
        if (lt >= 0) {
            bool done = lt >= r->dur && i < nruns - 1;
            double theta = 2 * M_PI * lt / r->rev;
            if (done) {
                for (int j = 0; j <= n; j++) pts[j] = to_screen(r->curve[j % n], c, sc);
                draw_polyline(pts, n + 1, 3 * k, COL_TEAL);
            } else {
                int jnow = (int)floor(theta / (2 * M_PI) * n), len = (int)(TRAIL * n);
                for (int q = 0; q <= len; q++)
                    pts[q] = to_screen(r->curve[((jnow - len + q) % n + n) % n], c, sc);
                draw_polyline(pts, len + 1, 3.5f * k, COL_TEAL);
                draw_chain(r, theta, c, sc, k);
            }
        }
        char buf[128];
        snprintf(buf, sizeof buf, "%.60s: %d / %d circles", r->label, r->have, n);
        int line = 0;
        for (int q = 0; q < i; q++) if (runs[q].group == r->group) line++;
        draw_text(buf, w * 0.03f, c.y - 14 * k + line * 34 * k, 28 * k, lt >= 0 ? COL_DIM : COL_GRID);
    }
}

static void unload(void) {}

const Scene scene_epicycles = {
    .name = "Fourier epicycles",
    .formula = "z(theta) = sum_k c_k e^(i k theta),   c_k = (1/N) sum_n z_n e^(-2 pi i k n / N),  N = 256 / 1024 / 64",
    .init = init, .update = update, .draw = draw, .unload = unload,
};
