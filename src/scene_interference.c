/* Scene 8: two-source interference, a ripple tank. Two point sources in
 * phase emit circular waves; each update sums them on the CPU into a small
 * texture that is scaled up with bilinear filtering. The separation of the
 * sources grows from 1 to 6 wavelengths over the scene, so the nodal lines
 * (path difference = (m + 1/2) lambda) multiply and fan in. On the right:
 * the time-averaged intensity along the far edge of the tank, in closed form.
 * The field depends on time only, so any dt draws the same picture. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "draw.h"
#include "scene.h"

#define TW 300                    /* field texture, px */
#define TH 200
#define LAMBDA 22.0f              /* wavelength, texture px */
#define FREQ 1.0f                 /* Hz */
#define SRC_X 36.0f               /* sources' x, texture px */
#define CLUT 1024                 /* cosine table, power of two */
#define GAIN 1.6f                 /* displacement -> colour */

static Texture2D tex;
static Color *field;
static Color lut[256];
static float coslut[CLUT];
static float t;

/* source separation in px: eases from 1 lambda at t = 0 to 6 at the end */
static float separation(float tt)
{
    float u = fminf(tt / g_app.cycle, 1.0f);
    return LAMBDA * (1.0f + 2.5f * (1.0f - cosf(PI * u)));
}

/* amplitude of a circular wave falls off as 1/sqrt(r), softened at r = 0 */
static float amp(float r) { return 1.0f / sqrtf(1.0f + r / LAMBDA); }

static Color mix(Color a, Color b, float f)
{
    return (Color){ (unsigned char)(a.r + f * (b.r - a.r)), (unsigned char)(a.g + f * (b.g - a.g)),
                    (unsigned char)(a.b + f * (b.b - a.b)), 255 };
}

static void render(void)
{
    float d = separation(t), y1 = TH * 0.5f - d / 2, y2 = TH * 0.5f + d / 2;
    float per_px = CLUT / LAMBDA;
    float ph = fmodf(FREQ * t, 1.0f) * CLUT;      /* outgoing: cos(k r - w t) */
    for (int y = 0; y < TH; y++) {
        float dy1 = y - y1, dy2 = y - y2;
        for (int x = 0; x < TW; x++) {
            float dx = x - SRC_X;
            float r1 = sqrtf(dx * dx + dy1 * dy1), r2 = sqrtf(dx * dx + dy2 * dy2);
            int i1 = (int)(r1 * per_px - ph + CLUT) & (CLUT - 1);
            int i2 = (int)(r2 * per_px - ph + CLUT) & (CLUT - 1);
            float v = GAIN * (coslut[i1] * amp(r1) + coslut[i2] * amp(r2));
            v = v < -1 ? -1 : v > 1 ? 1 : v;
            field[y * TW + x] = lut[(int)((v + 1) * 127.5f)];
        }
    }
    UpdateTexture(tex, field);
}

static void init(void)
{
    t = 0;
    for (int i = 0; i < CLUT; i++) coslut[i] = cosf(2 * PI * i / CLUT);
    /* diverging: trough blue, rest background, crest teal to near white */
    Color lo = { 40, 80, 190, 255 }, hi = { 170, 245, 225, 255 };
    for (int i = 0; i < 256; i++) {
        float u = i / 127.5f - 1;
        lut[i] = u < 0 ? mix(COL_BG, lo, -u) : mix(COL_BG, hi, u);
    }
    field = malloc(sizeof *field * TW * TH);
    Image img = { .data = field, .width = TW, .height = TH, .mipmaps = 1,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    for (int i = 0; i < TW * TH; i++) field[i] = COL_BG;
    tex = LoadTextureFromImage(img);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    render();
}

static void update(float dt)
{
    t += dt;
    render();
}

static void draw(int w, int h)
{
    float k = draw_scale(h);
    float sc = fminf(w * 0.62f / TW, h * 0.74f / TH);   /* keep circles round */
    Rectangle area = { w * 0.05f, h * 0.17f, TW * sc, TH * sc };
    DrawTexturePro(tex, (Rectangle){ 0, 0, TW, TH }, area, (Vector2){ 0 }, 0, WHITE);
    DrawRectangleLinesEx(area, 2 * k, COL_AXIS);

    /* the far edge, where the intensity on the right is taken */
    float ex = area.x + area.width;
    DrawLineEx((Vector2){ ex, area.y }, (Vector2){ ex, area.y + area.height }, 4 * k, COL_AMBER);

    float d = separation(t), y1 = TH * 0.5f - d / 2, y2 = TH * 0.5f + d / 2;
    Vector2 s1 = { area.x + SRC_X * sc, area.y + y1 * sc }, s2 = { area.x + SRC_X * sc, area.y + y2 * sc };
    DrawCircleV(s1, 8 * k, COL_PINK);
    DrawCircleV(s2, 8 * k, COL_PINK);

    /* time-averaged intensity along x = TW - 1:
     * I = A1^2 + A2^2 + 2 A1 A2 cos(k (r1 - r2)) */
    float px0 = ex + 40 * k, px1 = w * 0.96f;
    DrawLineEx((Vector2){ px0, area.y }, (Vector2){ px0, area.y + area.height }, 1.5f * k, COL_AXIS);
    static Vector2 pts[TH];
    float I[TH], imax = 1e-6f, dx = (TW - 1) - SRC_X;
    for (int y = 0; y < TH; y++) {
        float r1 = sqrtf(dx * dx + (y - y1) * (y - y1)), r2 = sqrtf(dx * dx + (y - y2) * (y - y2));
        float a1 = amp(r1), a2 = amp(r2);
        I[y] = a1 * a1 + a2 * a2 + 2 * a1 * a2 * cosf(2 * PI * (r1 - r2) / LAMBDA);
        if (I[y] > imax) imax = I[y];
    }
    for (int y = 0; y < TH; y++)
        pts[y] = (Vector2){ px0 + (px1 - px0) * I[y] / imax, area.y + (y + 0.5f) * sc };
    draw_polyline(pts, TH, 3 * k, COL_AMBER);
    draw_text("intensity", px0, area.y + area.height + 12 * k, 22 * k, COL_DIM);
    draw_text("at far edge", px0, area.y + area.height + 38 * k, 22 * k, COL_DIM);

    char buf[160];
    snprintf(buf, sizeof buf, "d = %.1f lambda    bright: r1 - r2 = m lambda    dark: r1 - r2 = (m + 1/2) lambda",
             d / LAMBDA);
    draw_text(buf, area.x, area.y - 36 * k, 24 * k, COL_DIM);
}

static void unload(void)
{
    UnloadTexture(tex);
    free(field);
    field = NULL;
}

const Scene scene_interference = {
    .name = "Two-source interference",
    .formula = "y = A1 cos(k r1 - wt) + A2 cos(k r2 - wt),   I = A1^2 + A2^2 + 2 A1 A2 cos(k (r1 - r2))",
    .init = init, .update = update, .draw = draw, .unload = unload,
};
