#include "draw.h"

#include <math.h>

float draw_scale(int h) { return (float)h / 1080.0f; }

void draw_text(const char *s, float x, float y, float size, Color c)
{
    DrawTextEx(GetFontDefault(), s, (Vector2){ x, y }, size, size / 10.0f, c);
}

float draw_text_width(const char *s, float size)
{
    return MeasureTextEx(GetFontDefault(), s, size, size / 10.0f).x;
}

void draw_polyline(const Vector2 *p, int n, float thick, Color c)
{
    if (n < 2) return;
    if (thick <= 1.5f) DrawLineStrip(p, n, c);
    else DrawSplineLinear(p, n, thick, c);
}

void draw_spring(Vector2 a, Vector2 b, int coils, float width, float thick, Color c)
{
    float dx = b.x - a.x, dy = b.y - a.y, len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-3f) return;
    float ux = dx / len, uy = dy / len, nx = -uy, ny = ux;
    Vector2 pts[2 * 32 + 4];
    int n = 0, zig = 2 * coils;
    if (zig > 64) zig = 64;
    float lead = len * 0.08f;
    pts[n++] = a;
    pts[n++] = (Vector2){ a.x + ux * lead, a.y + uy * lead };
    for (int i = 0; i < zig; i++) {
        float t = lead + (len - 2 * lead) * (i + 0.5f) / zig;
        float s = (i & 1) ? -width : width;
        pts[n++] = (Vector2){ a.x + ux * t + nx * s, a.y + uy * t + ny * s };
    }
    pts[n++] = (Vector2){ b.x - ux * lead, b.y - uy * lead };
    pts[n++] = b;
    draw_polyline(pts, n, thick, c);
}

void draw_dashed(Vector2 a, Vector2 b, float dash, float thick, Color c)
{
    float dx = b.x - a.x, dy = b.y - a.y, len = sqrtf(dx * dx + dy * dy);
    if (len < 1e-3f || dash <= 0) return;
    int n = (int)(len / dash);
    for (int i = 0; i < n; i += 2) {
        float t0 = i * dash / len, t1 = fminf((i + 1) * dash / len, 1.0f);
        DrawLineEx((Vector2){ a.x + dx * t0, a.y + dy * t0 },
                   (Vector2){ a.x + dx * t1, a.y + dy * t1 }, thick, c);
    }
}

void draw_axes(float x0, float y0, float x1, float y1, float ymid, Color c)
{
    DrawLineEx((Vector2){ x0, ymid }, (Vector2){ x1, ymid }, 1.5f, c);
    DrawLineEx((Vector2){ x0, y0 }, (Vector2){ x0, y1 }, 1.5f, c);
}
