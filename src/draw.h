/* Small drawing helpers and the shared palette. Sizes are given for a
 * 1080-line target and scaled by draw_scale(h). */
#ifndef HWD_DRAW_H
#define HWD_DRAW_H

#include "raylib.h"

#define COL_BG      (Color){ 12, 16, 24, 255 }
#define COL_GRID    (Color){ 32, 42, 58, 255 }
#define COL_AXIS    (Color){ 70, 86, 110, 255 }
#define COL_TEXT    (Color){ 220, 228, 240, 255 }
#define COL_DIM     (Color){ 120, 136, 160, 255 }
#define COL_TEAL    (Color){ 47, 211, 168, 255 }
#define COL_PINK    (Color){ 255, 120, 200, 255 }
#define COL_AMBER   (Color){ 255, 190, 80, 255 }
#define COL_BLUE    (Color){ 90, 160, 255, 255 }

float draw_scale(int h);
void draw_text(const char *s, float x, float y, float size, Color c);
float draw_text_width(const char *s, float size);
void draw_polyline(const Vector2 *p, int n, float thick, Color c);
void draw_spring(Vector2 a, Vector2 b, int coils, float width, float thick, Color c);
void draw_dashed(Vector2 a, Vector2 b, float dash, float thick, Color c);
void draw_axes(float x0, float y0, float x1, float y1, float ymid, Color c);

#endif
