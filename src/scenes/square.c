/* Scene 2: a square wave built from its odd harmonics, one added every
 * cycle/8 seconds, with the matching tone on the audio output. */
#include <math.h>
#include <stdio.h>

#include "app.h"
#include "audio.h"
#include "draw.h"
#include "scene.h"

#define NTERMS AUDIO_MAX_TERMS     /* harmonics 1, 3, ..., 15 */
#define NPTS 720
#define FADE_IN 0.6f               /* visual fade of a new harmonic, s */

static float t;

static int terms_at(float tt)
{
    int n = 1 + (int)(tt / (g_app.cycle / NTERMS));
    return n > NTERMS ? NTERMS : n;
}

/* visual weight of term k (0-based) at time tt: 0..1 */
static float weight(int k, float tt)
{
    float born = k * (g_app.cycle / NTERMS);
    float a = (tt - born) / FADE_IN;
    return a <= 0 ? 0 : a >= 1 ? 1 : a;
}

static void init(void)
{
    t = 0;
    audio_set_fundamental(110.0f);
    audio_set_terms(1);
    audio_set_active(true);
}

static void update(float dt)
{
    t += dt;
    audio_set_terms(terms_at(t));
}

static void draw(int w, int h)
{
    float k = draw_scale(h);
    float x0 = w * 0.06f, x1 = w * 0.94f, cy = h * 0.38f, A = h * 0.17f;
    const float periods = 2.0f, drift = 0.15f * t;   /* slow scroll */
    draw_axes(x0, cy - A * 1.4f, x1, cy + A * 1.4f, cy, COL_AXIS);
    /* ideal square, dim */
    Vector2 sq[NPTS], sum[NPTS], part[NPTS];
    for (int i = 0; i < NPTS; i++) {
        float u = (float)i / (NPTS - 1), ph = 2 * PI * (periods * u + drift);
        float x = x0 + (x1 - x0) * u;
        sq[i] = (Vector2){ x, cy - A * (sinf(ph) >= 0 ? 1.0f : -1.0f) };
        float s = 0;
        for (int j = 0; j < NTERMS; j++) {
            float wj = weight(j, t);
            if (wj > 0) s += wj * sinf((2 * j + 1) * ph) / (2 * j + 1);
        }
        sum[i] = (Vector2){ x, cy - A * (4 / PI) * s };
    }
    draw_polyline(sq, NPTS, 1.0f, COL_GRID);
    /* the newest harmonic on its own */
    int n = terms_at(t), last = n - 1, hk = 2 * last + 1;
    for (int i = 0; i < NPTS; i++) {
        float u = (float)i / (NPTS - 1), ph = 2 * PI * (periods * u + drift);
        part[i] = (Vector2){ sum[i].x, cy - A * (4 / PI) * weight(last, t) * sinf(hk * ph) / hk };
    }
    if (last > 0) draw_polyline(part, NPTS, 2 * k, Fade(COL_PINK, 0.7f));
    draw_polyline(sum, NPTS, 4 * k, COL_TEAL);

    /* spectrum: bars at k = 1..15, height 1/k, odd ones active */
    float bx0 = w * 0.10f, bx1 = w * 0.90f, base = h * 0.90f, H = h * 0.25f;
    DrawLineEx((Vector2){ bx0, base }, (Vector2){ bx1, base }, 2 * k, COL_AXIS);
    float slot = (bx1 - bx0) / 15.0f, bw = slot * 0.55f;
    for (int hh = 1; hh <= 15; hh++) {
        float x = bx0 + slot * (hh - 0.5f);
        char lab[8];
        snprintf(lab, sizeof lab, "%d", hh);
        draw_text(lab, x - draw_text_width(lab, 22 * k) / 2, base + 10 * k, 22 * k, COL_DIM);
        if (!(hh & 1)) continue;
        float bh = H / hh, wj = weight(hh / 2, t);
        Rectangle r = { x - bw / 2, base - bh, bw, bh };
        DrawRectangleLinesEx(r, 2 * k, COL_GRID);
        if (wj > 0) {
            Rectangle f = { r.x, base - bh * wj, bw, bh * wj };
            DrawRectangleRec(f, hh == hk ? COL_PINK : COL_TEAL);
        }
    }
    char buf[96];
    snprintf(buf, sizeof buf, "terms: %d   highest harmonic: %d (%d Hz)   f0 = 110 Hz%s",
             n, hk, 110 * hk, g_app.mute ? "   [muted]" : "");
    draw_text(buf, bx0, base - H - 50 * k, 26 * k, COL_DIM);
}

static void unload(void) { audio_set_active(false); }

const Scene scene_square = {
    .name = "Square wave from odd harmonics",
    .formula = "f(t) = (4/pi) * sum_{k=1,3,5..15} sin(k w t) / k",
    .init = init, .update = update, .draw = draw, .unload = unload,
};
