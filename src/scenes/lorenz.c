/* Scene 9: the Lorenz attractor. Three trajectories start 1e-4 apart in x
 * and are integrated with classical RK4 at a fixed internal step; they ride
 * together for about 27 s of the 45 s scene, then split onto different
 * wings. The view (orthographic, tilted) sways about the z axis around the
 * angle that shows both wings side by side. The two non-trivial fixed
 * points C+ and C-, the centres of the wings, are marked. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "raylib.h"
#include "scene.h"

#define SIGMA 10.0
#define RHO 28.0
#define BETA (8.0 / 3.0)
#define H 0.002                   /* integrator step, model time */
#define SPEED 0.8                 /* model time per second */
#define NTRAJ 3
#define TRAIL 1500                /* every 2nd step: 6 model units = 7.5 s */
#define SWAY 0.6f                 /* view sway amplitude, rad */
#define SWAY_W 0.15f              /* view sway, rad/s */
#define TILT 0.30f                /* view elevation, rad */

typedef struct Traj {
    double s[3];
    Vector3 trail[TRAIL];
    int head, count;
} Traj;

static Traj tr[NTRAJ];
static const double offset[NTRAJ] = { 0, 1e-4, -1e-4 };
static double acc;
static long steps;
static float t;

static void deriv(const double *s, double *d)
{
    d[0] = SIGMA * (s[1] - s[0]);
    d[1] = s[0] * (RHO - s[2]) - s[1];
    d[2] = s[0] * s[1] - BETA * s[2];
}

static void rk4(double *s, double h)
{
    double k1[3], k2[3], k3[3], k4[3], tmp[3];
    deriv(s, k1);
    for (int i = 0; i < 3; i++) tmp[i] = s[i] + 0.5 * h * k1[i];
    deriv(tmp, k2);
    for (int i = 0; i < 3; i++) tmp[i] = s[i] + 0.5 * h * k2[i];
    deriv(tmp, k3);
    for (int i = 0; i < 3; i++) tmp[i] = s[i] + h * k3[i];
    deriv(tmp, k4);
    for (int i = 0; i < 3; i++) s[i] += h / 6.0 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]);
}

static void init(void)
{
    memset(tr, 0, sizeof tr);
    for (int i = 0; i < NTRAJ; i++) {
        tr[i].s[0] = 1.0 + offset[i];
        tr[i].s[1] = 1.0;
        tr[i].s[2] = 1.0;
    }
    acc = 0;
    steps = 0;
    t = 0;
}

static void update(float dt)
{
    t += dt;
    acc += dt * SPEED;
    while (acc >= H) {
        acc -= H;
        for (int i = 0; i < NTRAJ; i++) {
            Traj *q = &tr[i];
            rk4(q->s, H);
            if ((steps & 1) == 0) {
                q->trail[q->head] = (Vector3){ (float)q->s[0], (float)q->s[1], (float)q->s[2] };
                q->head = (q->head + 1) % TRAIL;
                if (q->count < TRAIL) q->count++;
            }
        }
        steps++;
    }
}

typedef struct View { float cphi, sphi, ce, se, sc; Vector2 c; } View;

static Vector2 project(const View *v, float x, float y, float z)
{
    float u = x * v->cphi - y * v->sphi, depth = x * v->sphi + y * v->cphi;
    float up = (z - 25.0f) * v->ce - depth * v->se;
    return (Vector2){ v->c.x + u * v->sc, v->c.y - up * v->sc };
}

static void draw_traj(const Traj *q, const View *v, Color col, float k)
{
    static Vector2 pts[TRAIL];
    int n = q->count;
    for (int i = 0; i < n; i++) {
        Vector3 p = q->trail[(q->head - n + i + TRAIL) % TRAIL];
        pts[i] = project(v, p.x, p.y, p.z);
    }
    /* the trail fades with age: four pieces of rising opacity */
    for (int seg = 0; seg < 4; seg++) {
        int a = n * seg / 4, b = n * (seg + 1) / 4 + 1;
        if (b > n) b = n;
        if (b - a >= 2) draw_polyline(pts + a, b - a, 2.5f * k, Fade(col, 0.15f + 0.25f * seg));
    }
    DrawCircleV(project(v, (float)q->s[0], (float)q->s[1], (float)q->s[2]), 9 * k, col);
}

static void draw(int w, int h)
{
    float k = draw_scale(h), phi = -PI / 4 + SWAY * sinf(SWAY_W * t);
    View v = { cosf(phi), sinf(phi), cosf(TILT), sinf(TILT), h * 0.74f / 60.0f, { w * 0.58f, h * 0.56f } };
    static const Color col[NTRAJ] = { COL_TEAL, COL_PINK, COL_AMBER };

    double cfix = sqrt(BETA * (RHO - 1));
    for (int sgn = -1; sgn <= 1; sgn += 2) {
        Vector2 p = project(&v, (float)(sgn * cfix), (float)(sgn * cfix), (float)(RHO - 1));
        DrawRing(p, 6 * k, 9 * k, 0, 360, 24, COL_AXIS);
        draw_text(sgn > 0 ? "C+" : "C-", p.x + 14 * k, p.y - 10 * k, 22 * k, COL_DIM);
    }
    for (int i = NTRAJ - 1; i >= 0; i--) draw_traj(&tr[i], &v, col[i], k);

    char buf[96];
    float x0 = w * 0.04f, y0 = h * 0.18f;
    snprintf(buf, sizeof buf, "t = %.1f (model time)", steps * H);
    draw_text(buf, x0, y0, 26 * k, COL_DIM);
    static const char *const start[NTRAJ] = { "x0 = 1", "x0 = 1 + 1e-4", "x0 = 1 - 1e-4" };
    for (int i = 0; i < NTRAJ; i++) {
        double dd = 0;
        for (int j = 0; j < 3; j++) dd += (tr[i].s[j] - tr[0].s[j]) * (tr[i].s[j] - tr[0].s[j]);
        if (i == 0) snprintf(buf, sizeof buf, "%s", start[i]);
        else snprintf(buf, sizeof buf, "%-14s |d| = %.1e", start[i], sqrt(dd));
        draw_text(buf, x0, y0 + (i + 1) * 36 * k, 26 * k, col[i]);
    }
}

static void unload(void) {}

const Scene scene_lorenz = {
    .name = "Lorenz attractor",
    .formula = "x' = s(y - x),  y' = x(r - z) - y,  z' = xy - bz;   s = 10, r = 28, b = 8/3, RK4",
    .init = init, .update = update, .draw = draw, .unload = unload,
};
