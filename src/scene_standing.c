/* Scene 4: standing waves on a string fixed at both ends, modes n = 1..5.
 * Each mode is drawn with the two counter-propagating travelling waves whose
 * sum it is (faint), and its nodes x = mL/n marked. */
#include <math.h>
#include <stdio.h>

#include "draw.h"
#include "scene.h"

#define MODES 5
#define NPTS 360
#define OMEGA1 2.2f        /* rad/s, fundamental */

static float t;

static void init(void) { t = 0; }
static void update(float dt) { t += dt; }

static void draw(int w, int h)
{
    float k = draw_scale(h);
    float x0 = w * 0.12f, x1 = w * 0.95f, L = x1 - x0;
    float top = h * 0.14f, row = (h * 0.95f - top) / MODES, A = row * 0.22f;
    static const Color mode_col[MODES] = {
        { 47, 211, 168, 255 }, { 90, 160, 255, 255 }, { 255, 190, 80, 255 },
        { 255, 120, 200, 255 }, { 180, 140, 255, 255 },
    };
    for (int n = 1; n <= MODES; n++) {
        float cy = top + row * (n - 0.5f), kn = n * PI / L, wn = n * OMEGA1;
        Vector2 sw[NPTS], r[NPTS], l[NPTS];
        for (int i = 0; i < NPTS; i++) {
            float x = L * i / (NPTS - 1), px = x0 + x;
            sw[i] = (Vector2){ px, cy - 2 * A * sinf(kn * x) * cosf(wn * t) };
            /* 2A sin(kx) cos(wt) = A sin(kx - wt) + A sin(kx + wt) */
            r[i] = (Vector2){ px, cy - A * sinf(kn * x - wn * t) };
            l[i] = (Vector2){ px, cy - A * sinf(kn * x + wn * t) };
        }
        DrawLineEx((Vector2){ x0, cy }, (Vector2){ x1, cy }, 1.0f, COL_GRID);
        /* envelope */
        for (int i = 0; i + 1 < NPTS; i += 6) {
            float x = L * i / (NPTS - 1), e = 2 * A * fabsf(sinf(kn * x));
            DrawLineEx((Vector2){ x0 + x, cy - e }, (Vector2){ x0 + x, cy + e }, 1.0f, Fade(COL_GRID, 0.6f));
        }
        draw_polyline(r, NPTS, 1.5f * k, Fade(COL_DIM, 0.35f));
        draw_polyline(l, NPTS, 1.5f * k, Fade(COL_DIM, 0.35f));
        draw_polyline(sw, NPTS, 4 * k, mode_col[n - 1]);
        for (int m = 0; m <= n; m++) {
            Vector2 p = { x0 + L * m / n, cy };
            DrawCircleV(p, 8 * k, COL_BG);
            DrawRing(p, 5 * k, 8 * k, 0, 360, 24, COL_TEXT);
        }
        DrawRectangle((int)(x0 - 8 * k), (int)(cy - A * 2.3f), (int)(6 * k), (int)(A * 4.6f), COL_AXIS);
        DrawRectangle((int)(x1 + 2 * k), (int)(cy - A * 2.3f), (int)(6 * k), (int)(A * 4.6f), COL_AXIS);
        char buf[48];
        snprintf(buf, sizeof buf, "n = %d", n);
        draw_text(buf, w * 0.03f, cy - 26 * k, 30 * k, mode_col[n - 1]);
        snprintf(buf, sizeof buf, "%d nodes", n + 1);
        draw_text(buf, w * 0.03f, cy + 8 * k, 20 * k, COL_DIM);
    }
}

static void unload(void) {}

const Scene scene_standing = {
    .name = "Standing waves",
    .formula = "y = 2A sin(n pi x / L) cos(w_n t) = A sin(kx - wt) + A sin(kx + wt),  w_n = n w_1",
    .init = init, .update = update, .draw = draw, .unload = unload,
};
