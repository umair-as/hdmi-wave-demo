/* Scene 7: Lissajous figures. Two perpendicular oscillators at the
 * frequency ratio a:b trace x = sin(a s + d), y = sin(b s). The ratio steps
 * through five small-integer pairs, one every cycle/5 seconds, and the phase
 * d drifts, so each figure seems to turn in 3D. The pen's two projections
 * ride on rails above and left of the figure, as the phasor does in scene 1.
 * Everything is a closed-form function of time. */
#include <math.h>
#include <stdio.h>

#include "app.h"
#include "draw.h"
#include "scene.h"

#define NPTS 1200                 /* points on the main curve */
#define THUMB_PTS 300
#define PEN_W 1.1f                /* rad/s along the curve parameter s */
#define DRIFT 0.35f               /* rad/s of the phase d */
#define FADE_S 1.0f               /* a new figure fades in over this */

static const struct { int a, b; } ratio[] = { { 1, 2 }, { 3, 2 }, { 3, 4 }, { 5, 4 }, { 5, 6 } };
#define NRATIO ((int)(sizeof ratio / sizeof ratio[0]))

static float t;

static void init(void) { t = 0; }
static void update(float dt) { t += dt; }

static Vector2 at(int a, int b, float s, float d, Vector2 c, float r)
{
    return (Vector2){ c.x + r * sinf(a * s + d), c.y - r * sinf(b * s) };
}

static void draw(int w, int h)
{
    float k = draw_scale(h);
    float seg = g_app.cycle / NRATIO;
    int ci = (int)(t / seg);
    if (ci > NRATIO - 1) ci = NRATIO - 1;
    float fade = fminf((t - ci * seg) / FADE_S, 1.0f);
    int a = ratio[ci].a, b = ratio[ci].b;
    float d = DRIFT * t, s = PEN_W * t;

    /* main figure, its frame and the two projection rails */
    float R = h * 0.32f;
    Vector2 c = { w * 0.60f, h * 0.57f };
    DrawRectangleLinesEx((Rectangle){ c.x - R, c.y - R, 2 * R, 2 * R }, 1.5f * k, COL_GRID);
    float rail_y = c.y - R - 36 * k, rail_x = c.x - R - 36 * k;
    DrawLineEx((Vector2){ c.x - R, rail_y }, (Vector2){ c.x + R, rail_y }, 2 * k, COL_AXIS);
    DrawLineEx((Vector2){ rail_x, c.y - R }, (Vector2){ rail_x, c.y + R }, 2 * k, COL_AXIS);

    static Vector2 pts[NPTS + 1];
    for (int i = 0; i <= NPTS; i++) pts[i] = at(a, b, 2 * PI * i / NPTS, d, c, R);
    draw_polyline(pts, NPTS + 1, 3 * k, Fade(COL_TEAL, 0.25f + 0.65f * fade));

    Vector2 pen = at(a, b, s, d, c, R);
    Vector2 on_x = { pen.x, rail_y }, on_y = { rail_x, pen.y };
    draw_dashed(on_x, pen, 10 * k, 2 * k, Fade(COL_AMBER, 0.6f));
    draw_dashed(on_y, pen, 10 * k, 2 * k, Fade(COL_BLUE, 0.6f));
    DrawCircleV(on_x, 9 * k, COL_AMBER);
    DrawCircleV(on_y, 9 * k, COL_BLUE);
    DrawCircleV(pen, 10 * k, COL_PINK);

    char buf[96];
    snprintf(buf, sizeof buf, "a:b = %d:%d    d = %.2f rad", a, b, fmodf(d, 2 * PI));
    draw_text(buf, c.x - R, c.y + R + 18 * k, 26 * k, COL_DIM);

    /* every ratio as a thumbnail, the current one highlighted */
    float rt = h * 0.055f, tx = w * 0.10f;
    Vector2 th[THUMB_PTS + 1];
    for (int q = 0; q < NRATIO; q++) {
        Vector2 tc = { tx, h * (0.22f + 0.155f * q) };
        bool cur = q == ci;
        for (int i = 0; i <= THUMB_PTS; i++) th[i] = at(ratio[q].a, ratio[q].b, 2 * PI * i / THUMB_PTS, d, tc, rt);
        draw_polyline(th, THUMB_PTS + 1, (cur ? 2.5f : 1.5f) * k, cur ? COL_TEAL : COL_GRID);
        snprintf(buf, sizeof buf, "%d:%d", ratio[q].a, ratio[q].b);
        draw_text(buf, tx + rt + 18 * k, tc.y - 14 * k, 28 * k, cur ? COL_TEXT : COL_DIM);
    }
}

static void unload(void) {}

const Scene scene_lissajous = {
    .name = "Lissajous figures",
    .formula = "x = sin(a s + d),  y = sin(b s);   a:b = 1:2, 3:2, 3:4, 5:4, 5:6, d drifting",
    .init = init, .update = update, .draw = draw, .unload = unload,
};
