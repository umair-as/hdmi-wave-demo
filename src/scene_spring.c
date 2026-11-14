/* Scene 1: a mass on a spring and the phasor that generates its motion.
 * Everything is a closed-form function of time, so no history is kept and
 * any dt (live or fixed-step screenshots) draws the same picture. */
#include <math.h>
#include <stdio.h>

#include "draw.h"
#include "scene.h"

#define OMEGA 1.6f          /* rad/s */
#define WINDOW_S 8.0f       /* seconds of history shown on the graph */
#define NPTS 480

static float t;

static void init(void) { t = 0; }
static void update(float dt) { t += dt; }

static void draw(int w, int h)
{
    float k = draw_scale(h);
    float A = h * 0.26f, cy = h * 0.55f;
    Vector2 c = { w * 0.17f, cy };            /* phasor centre */
    float th = OMEGA * t;
    /* phasor angle measured from "up" so the vertical projection is A cos(wt) */
    Vector2 tip = { c.x + A * sinf(th), c.y - A * cosf(th) };
    float x = A * cosf(th);

    /* phasor circle */
    DrawRing(c, A - 1.5f * k, A + 1.5f * k, 0, 360, 96, COL_GRID);
    DrawLineEx((Vector2){ c.x - A, c.y }, (Vector2){ c.x + A, c.y }, 1.5f * k, COL_GRID);
    DrawLineEx((Vector2){ c.x, c.y - A }, (Vector2){ c.x, c.y + A }, 1.5f * k, COL_GRID);
    DrawCircleSector(c, A * 0.22f, 270, 270 + th * RAD2DEG - 360.0f * floorf(th / (2 * PI)), 48, Fade(COL_AMBER, 0.35f));
    DrawLineEx(c, tip, 4 * k, COL_AMBER);
    DrawLineEx((Vector2){ c.x, c.y }, (Vector2){ c.x, c.y - x }, 4 * k, Fade(COL_TEAL, 0.7f));
    DrawCircleV(tip, 9 * k, COL_AMBER);

    /* spring hanging from a ceiling; the mass sits at height cy - x */
    float sx = w * 0.40f, top = h * 0.16f;
    DrawLineEx((Vector2){ sx - 70 * k, top }, (Vector2){ sx + 70 * k, top }, 5 * k, COL_AXIS);
    float my = cy - x, half = 38 * k;
    draw_spring((Vector2){ sx, top }, (Vector2){ sx, my - half }, 14, 26 * k, 3 * k, COL_DIM);
    DrawRectangleRounded((Rectangle){ sx - half, my - half, 2 * half, 2 * half }, 0.25f, 8, COL_TEAL);
    DrawLineEx((Vector2){ sx - 90 * k, cy }, (Vector2){ sx + 90 * k, cy }, 1.5f * k, COL_GRID);

    /* graph of x(t) over the last WINDOW_S seconds, newest at the left edge */
    float gx0 = w * 0.48f, gx1 = w * 0.96f;
    draw_axes(gx0, cy - A - 20 * k, gx1, cy + A + 20 * k, cy, COL_AXIS);
    Vector2 pts[NPTS];
    for (int i = 0; i < NPTS; i++) {
        float age = WINDOW_S * i / (NPTS - 1);
        float tt = t - age;
        pts[i] = (Vector2){ gx0 + (gx1 - gx0) * i / (NPTS - 1), cy - A * cosf(OMEGA * tt) };
    }
    draw_polyline(pts, NPTS, 3.5f * k, COL_TEAL);
    DrawCircleV(pts[0], 8 * k, COL_TEAL);

    /* projection lines tie the three views together */
    draw_dashed(tip, (Vector2){ sx - half, tip.y }, 10 * k, 2 * k, Fade(COL_AMBER, 0.6f));
    draw_dashed((Vector2){ sx + half, my }, pts[0], 10 * k, 2 * k, Fade(COL_TEAL, 0.6f));

    char buf[96];
    snprintf(buf, sizeof buf, "wt = %6.2f rad    x = %+5.2f A", fmodf(th, 2 * PI), x / A);
    draw_text(buf, c.x - A, cy + A + 40 * k, 26 * k, COL_DIM);
    draw_text("t ->  (past)", gx1 - 200 * k, cy + A + 40 * k, 22 * k, COL_DIM);
}

static void unload(void) {}

const Scene scene_spring = {
    .name = "Spring and phasor",
    .formula = "x(t) = A cos(wt)    phasor A e^(iwt), projection = displacement",
    .init = init, .update = update, .draw = draw, .unload = unload,
};
