/* Minimal SVG loader for scene 3.
 *
 * Elements: <path> with absolute M/L/C/Z (one subpath), and <circle cx cy r>.
 * Transforms: scale(), translate() and matrix() on enclosing <g> elements and
 * on the element itself are applied, so the shapes come out in SVG user space
 * (y down, i.e. upright on screen) even when the file flips y with
 * <g transform="scale(1,-1)">.
 *
 * Every shape is returned as a CLOSED polyline (pts[n-1] == pts[0]) ready for
 * arc-length resampling and a DFT:
 *   closed path (Z):  cubics flattened in 64 steps; Z adds a closing line
 *                     when the end does not meet the start
 *   open path:        traced forward then back (out-and-back), so it closes
 *                     without a jump; pts[turn] is the stroke's end
 *   circle:           CIRCLE_STEPS points around the loop */
#ifndef HWD_SVGPATH_H
#define HWD_SVGPATH_H

#include <stdbool.h>
#include <stddef.h>

#define SVG_CIRCLE_STEPS 64

typedef struct { double x, y; } Pt2;

typedef enum { SVG_PATH, SVG_CIRCLE } SvgKind;

typedef struct SvgShape {
    char id[64];
    SvgKind kind;
    Pt2 *pts;            /* closed polyline, transform applied */
    int n;
    bool closed;         /* path ended with Z; always true for circles */
    bool out_and_back;   /* open path traced forward then back */
    int turn;            /* out-and-back: index of the turnaround vertex, else -1 */
    int nsub, nseg;      /* path: M count, L/C segments (+ implicit closing line) */
    Pt2 raw_start, raw_end;   /* path end points before the transform */
    Pt2 end;             /* path end point after the transform */
    double cx, cy, r;    /* circle, before the transform */
} SvgShape;

/* Number of <tag ...> elements in an SVG text. */
int svg_count_elements(const char *svg_text, const char *tag);

/* Load the shapes of a file, in document order. id == NULL loads every
 * <path> and <circle>; otherwise only the element with that id.
 * Returns the number of shapes stored (>= 1), or -1 with a message in err. */
int svg_parse_shapes(const char *svg_text, const char *id, SvgShape *out, int max,
                     char *err, size_t errlen);
int svg_load_shapes(const char *file, const char *id, SvgShape *out, int max,
                    char *err, size_t errlen);
void svg_shape_free(SvgShape *s);

/* Resample a closed polyline (pts[n-1] == pts[0]) to nout points evenly
 * spaced by arc length. Returns the total length. */
double svg_resample(const Pt2 *pts, int n, Pt2 *out, int nout);

/* Read a whole file; caller frees. NULL on error. */
char *svg_read_file(const char *file);

#endif
