/* Scene 6: double pendulum (point masses, massless rods) integrated with
 * classical RK4 at a fixed internal step. Key T toggles a twin started
 * 0.001 rad away, to show sensitive dependence on initial conditions. */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "draw.h"
#include "raylib.h"
#include "scene.h"

#define G 9.81
#define L1 1.0
#define L2 1.0
#define M1 1.0
#define M2 1.0
#define H (1.0 / 480.0)          /* integrator step, s */
#define TRAIL 900                /* trail samples (every 4th step: 7.5 s) */

typedef struct Pend {
    double s[4];                 /* theta1, theta2, omega1, omega2 */
    Vector2 trail[TRAIL];        /* bob-2 positions in rod units */
    int head, count;
} Pend;

static Pend p[2];
static bool twin = true;
static double acc_t;
static long steps;

static void deriv(const double *s, double *d)
{
    double t1 = s[0], t2 = s[1], w1 = s[2], w2 = s[3], dl = t1 - t2;
    double den = 2 * M1 + M2 - M2 * cos(2 * dl);
    d[0] = w1;
    d[1] = w2;
    d[2] = (-G * (2 * M1 + M2) * sin(t1) - M2 * G * sin(t1 - 2 * t2)
            - 2 * sin(dl) * M2 * (w2 * w2 * L2 + w1 * w1 * L1 * cos(dl))) / (L1 * den);
    d[3] = (2 * sin(dl) * (w1 * w1 * L1 * (M1 + M2) + G * (M1 + M2) * cos(t1)
            + w2 * w2 * L2 * M2 * cos(dl))) / (L2 * den);
}

static void rk4(double *s, double h)
{
    double k1[4], k2[4], k3[4], k4[4], tmp[4];
    deriv(s, k1);
    for (int i = 0; i < 4; i++) tmp[i] = s[i] + 0.5 * h * k1[i];
    deriv(tmp, k2);
    for (int i = 0; i < 4; i++) tmp[i] = s[i] + 0.5 * h * k2[i];
    deriv(tmp, k3);
    for (int i = 0; i < 4; i++) tmp[i] = s[i] + h * k3[i];
    deriv(tmp, k4);
    for (int i = 0; i < 4; i++) s[i] += h / 6.0 * (k1[i] + 2 * k2[i] + 2 * k3[i] + k4[i]);
}

static Vector2 bob2(const double *s)
{
    return (Vector2){ (float)(L1 * sin(s[0]) + L2 * sin(s[1])), (float)(L1 * cos(s[0]) + L2 * cos(s[1])) };
}

static void init(void)
{
    memset(p, 0, sizeof p);
    p[0].s[0] = 2.0; p[0].s[1] = 2.6;
    p[1] = p[0];
    p[1].s[0] += 0.001;
    acc_t = 0;
    steps = 0;
}

static void update(float dt)
{
    acc_t += dt;
    while (acc_t >= H) {
        acc_t -= H;
        for (int i = 0; i < 2; i++) {
            rk4(p[i].s, H);
            if ((steps & 3) == 0) {
                p[i].trail[p[i].head] = bob2(p[i].s);
                p[i].head = (p[i].head + 1) % TRAIL;
                if (p[i].count < TRAIL) p[i].count++;
            }
        }
        steps++;
    }
}

static void draw_one(const Pend *q, Vector2 o, float sc, Color col, float k)
{
    Vector2 pts[TRAIL];
    int n = q->count;
    for (int i = 0; i < n; i++) {
        Vector2 v = q->trail[(q->head - n + i + TRAIL) % TRAIL];
        pts[i] = (Vector2){ o.x + v.x * sc, o.y + v.y * sc };
    }
    /* trail fades with age: draw in four segments of rising opacity */
    for (int seg = 0; seg < 4; seg++) {
        int a = n * seg / 4, b = n * (seg + 1) / 4 + 1;
        if (b > n) b = n;
        if (b - a >= 2) draw_polyline(pts + a, b - a, 2.5f * k, Fade(col, 0.15f + 0.25f * seg));
    }
    Vector2 b1 = { o.x + (float)(L1 * sin(q->s[0])) * sc, o.y + (float)(L1 * cos(q->s[0])) * sc };
    Vector2 v2 = bob2(q->s), b2 = { o.x + v2.x * sc, o.y + v2.y * sc };
    DrawLineEx(o, b1, 5 * k, COL_TEXT);
    DrawLineEx(b1, b2, 5 * k, COL_TEXT);
    DrawCircleV(b1, 16 * k, col);
    DrawCircleV(b2, 16 * k, col);
}

static void draw(int w, int h)
{
    float k = draw_scale(h), sc = h * 0.19f;
    Vector2 o = { w * 0.5f, h * 0.50f };
    if (twin) draw_one(&p[1], o, sc, COL_PINK, k);
    draw_one(&p[0], o, sc, COL_TEAL, k);
    DrawCircleV(o, 7 * k, COL_AXIS);
    double d = fabs(p[1].s[1] - p[0].s[1]);
    char buf[96];
    snprintf(buf, sizeof buf, "t = %.1f s", steps * H);
    draw_text(buf, w * 0.04f, h * 0.16f, 26 * k, COL_DIM);
    if (twin) {
        snprintf(buf, sizeof buf, "twin: d(theta2) = %.4f rad", d);
        draw_text(buf, w * 0.04f, h * 0.16f + 36 * k, 26 * k, COL_PINK);
    }
}

static bool key(int kc)
{
    if (kc != KEY_T) return false;
    twin = !twin;
    return true;
}

static void unload(void) {}

const Scene scene_pendulum = {
    .name = "Double pendulum",
    .formula = "Lagrangian point-mass double pendulum, RK4, h = 1/480 s; twin starts 0.001 rad apart",
    .init = init, .update = update, .draw = draw, .unload = unload, .key = key,
    .keys_help = "T twin",
};
