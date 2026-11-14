#include "svgpath.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define FLATTEN_STEPS 64
#define MAX_DEPTH 32

typedef struct { double a, b, c, d, e, f; } Affine;   /* x' = a x + c y + e, y' = b x + d y + f */

static const Affine IDENT = { 1, 0, 0, 1, 0, 0 };

static Affine mul(Affine m, Affine n)                  /* m * n: apply n first */
{
    return (Affine){ m.a * n.a + m.c * n.b, m.b * n.a + m.d * n.b,
                     m.a * n.c + m.c * n.d, m.b * n.c + m.d * n.d,
                     m.a * n.e + m.c * n.f + m.e, m.b * n.e + m.d * n.f + m.f };
}

static Pt2 apply(Affine m, Pt2 p)
{
    return (Pt2){ m.a * p.x + m.c * p.y + m.e, m.b * p.x + m.d * p.y + m.f };
}

static void seterr(char *err, size_t len, const char *msg)
{
    if (err && len) snprintf(err, len, "%s", msg);
}

static const char *skip_sep(const char *s)
{
    while (*s && (isspace((unsigned char)*s) || *s == ',')) s++;
    return s;
}

static int read_nums(const char **s, double *v, int k)
{
    for (int i = 0; i < k; i++) {
        const char *q = skip_sep(*s);
        char *end;
        v[i] = strtod(q, &end);
        if (end == q) return -1;
        *s = end;
    }
    return 0;
}

/* Value of attribute `name` inside the tag [tag, end). Returns a malloc'd
 * string or NULL. */
static char *get_attr(const char *tag, const char *end, const char *name)
{
    size_t nl = strlen(name);
    for (const char *a = tag; a + nl + 2 < end; a++) {
        if (!isspace((unsigned char)a[0]) || strncmp(a + 1, name, nl) != 0) continue;
        const char *q = skip_sep(a + 1 + nl);
        if (*q != '=') continue;
        q = skip_sep(q + 1);
        char quote = *q;
        if (quote != '"' && quote != '\'') continue;
        const char *b = memchr(q + 1, quote, (size_t)(end - (q + 1)));
        if (!b) return NULL;
        char *v = malloc((size_t)(b - q));
        memcpy(v, q + 1, (size_t)(b - q - 1));
        v[b - q - 1] = 0;
        return v;
    }
    return NULL;
}

static int parse_transform(const char *s, Affine *out)
{
    Affine m = IDENT;
    for (;;) {
        s = skip_sep(s);
        if (!*s) break;
        double v[6];
        int k = 0;
        const char *open = strchr(s, '('), *close = open ? strchr(open, ')') : NULL;
        if (!open || !close) return -1;
        const char *p = open + 1;
        while (k < 6) {
            p = skip_sep(p);
            if (p >= close) break;
            char *e;
            v[k] = strtod(p, &e);
            if (e == p) return -1;
            k++;
            p = e;
        }
        Affine t;
        if (!strncmp(s, "scale", 5) && (k == 1 || k == 2))
            t = (Affine){ v[0], 0, 0, k == 2 ? v[1] : v[0], 0, 0 };
        else if (!strncmp(s, "translate", 9) && (k == 1 || k == 2))
            t = (Affine){ 1, 0, 0, 1, v[0], k == 2 ? v[1] : 0 };
        else if (!strncmp(s, "matrix", 6) && k == 6)
            t = (Affine){ v[0], v[1], v[2], v[3], v[4], v[5] };
        else
            return -1;                                 /* rotate/skew: not needed */
        m = mul(m, t);
        s = close + 1;
    }
    *out = m;
    return 0;
}

static int push(SvgShape *p, int *cap, Pt2 v)
{
    if (p->n == *cap) {
        int nc = *cap ? *cap * 2 : 256;
        Pt2 *np = realloc(p->pts, (size_t)nc * sizeof *np);
        if (!np) return -1;
        p->pts = np; *cap = nc;
    }
    p->pts[p->n++] = v;
    return 0;
}

static int parse_path_d(const char *d, Affine m, SvgShape *out, char *err, size_t errlen)
{
    int cap = 0;
    Pt2 cur = { 0, 0 }, start = { 0, 0 };
    const char *s = d;
    for (;;) {
        s = skip_sep(s);
        if (!*s) break;
        char c = *s++;
        if (out->closed) { seterr(err, errlen, "content after Z"); return -1; }
        double v[6];
        if (c == 'M') {
            if (read_nums(&s, v, 2)) { seterr(err, errlen, "bad M"); return -1; }
            if (++out->nsub > 1) { seterr(err, errlen, "more than one subpath"); return -1; }
            cur = start = (Pt2){ v[0], v[1] };
            if (push(out, &cap, cur)) return -1;
        } else if (c == 'L') {
            if (!out->nsub || read_nums(&s, v, 2)) { seterr(err, errlen, "bad L"); return -1; }
            cur = (Pt2){ v[0], v[1] };
            out->nseg++;
            if (push(out, &cap, cur)) return -1;
        } else if (c == 'C') {
            if (!out->nsub || read_nums(&s, v, 6)) { seterr(err, errlen, "bad C"); return -1; }
            for (int k = 1; k <= FLATTEN_STEPS; k++) {
                double t = (double)k / FLATTEN_STEPS, u = 1 - t;
                double a = u * u * u, b = 3 * u * u * t, e = 3 * u * t * t, f = t * t * t;
                if (push(out, &cap, (Pt2){ a * cur.x + b * v[0] + e * v[2] + f * v[4],
                                           a * cur.y + b * v[1] + e * v[3] + f * v[5] })) return -1;
            }
            cur = (Pt2){ v[4], v[5] };
            out->nseg++;
        } else if (c == 'Z') {
            if (!out->nsub) { seterr(err, errlen, "Z before M"); return -1; }
            out->closed = true;
        } else {
            char msg[64];
            snprintf(msg, sizeof msg, "unsupported command '%c' (only absolute M/L/C/Z)", c);
            seterr(err, errlen, msg);
            return -1;
        }
    }
    if (out->n < 2) { seterr(err, errlen, "path too short"); return -1; }
    out->raw_start = start;
    out->raw_end = cur;
    if (out->closed) {
        if (hypot(cur.x - start.x, cur.y - start.y) > 1e-9) {   /* Z draws the closing line */
            if (push(out, &cap, start)) return -1;
            out->nseg++;
        }
    } else {
        /* out-and-back: append the stroke reversed, ending on the start */
        int n = out->n;
        out->turn = n - 1;
        for (int i = n - 2; i >= 0; i--)
            if (push(out, &cap, out->pts[i])) return -1;
        out->out_and_back = true;
    }
    for (int i = 0; i < out->n; i++) out->pts[i] = apply(m, out->pts[i]);
    out->end = apply(m, cur);
    return 0;
}

static int parse_circle(const char *tag, const char *end, Affine m, SvgShape *out,
                        char *err, size_t errlen)
{
    const char *names[3] = { "cx", "cy", "r" };
    double v[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++) {
        char *a = get_attr(tag, end, names[i]);
        if (a) { v[i] = strtod(a, NULL); free(a); }
        else if (i == 2) { seterr(err, errlen, "circle without r"); return -1; }
    }
    if (!(v[2] > 0)) { seterr(err, errlen, "circle r <= 0"); return -1; }
    out->cx = v[0]; out->cy = v[1]; out->r = v[2];
    out->closed = true;
    out->n = SVG_CIRCLE_STEPS + 1;
    out->pts = malloc(sizeof *out->pts * (size_t)out->n);
    if (!out->pts) return -1;
    for (int i = 0; i <= SVG_CIRCLE_STEPS; i++) {
        double a = 2 * M_PI * (i % SVG_CIRCLE_STEPS) / SVG_CIRCLE_STEPS;
        out->pts[i] = apply(m, (Pt2){ v[0] + v[2] * cos(a), v[1] + v[2] * sin(a) });
    }
    return 0;
}

static bool tag_is(const char *t, const char *name)
{
    size_t l = strlen(name);
    return !strncmp(t, name, l) && (isspace((unsigned char)t[l]) || t[l] == '>' || t[l] == '/');
}

int svg_count_elements(const char *svg, const char *tag)
{
    int n = 0;
    for (const char *t = strchr(svg, '<'); t; t = strchr(t + 1, '<'))
        if (tag_is(t + 1, tag)) n++;
    return n;
}

int svg_parse_shapes(const char *svg, const char *id, SvgShape *out, int max,
                     char *err, size_t errlen)
{
    Affine stack[MAX_DEPTH];
    int depth = 0, count = 0;
    Affine cur = IDENT;
    for (const char *t = strchr(svg, '<'); t; t = strchr(t + 1, '<')) {
        const char *end = strchr(t, '>');
        if (!end) break;
        bool self_close = end > t && end[-1] == '/';
        if (!strncmp(t, "</g", 3)) {
            if (depth > 0) cur = stack[--depth];
            continue;
        }
        bool is_g = tag_is(t + 1, "g"), is_path = tag_is(t + 1, "path"), is_circle = tag_is(t + 1, "circle");
        if (!is_g && !is_path && !is_circle) continue;
        Affine m = cur;
        char *tr = get_attr(t, end, "transform");
        if (tr) {
            Affine local;
            int bad = parse_transform(tr, &local);
            free(tr);
            if (bad) { seterr(err, errlen, "unsupported transform (scale/translate/matrix only)"); goto fail; }
            m = mul(cur, local);
        }
        if (is_g) {
            if (!self_close) {
                if (depth == MAX_DEPTH) { seterr(err, errlen, "groups nested too deep"); goto fail; }
                stack[depth++] = cur;
                cur = m;
            }
            continue;
        }
        char *eid = get_attr(t, end, "id");
        bool want = !id || (eid && !strcmp(eid, id));
        if (!want) { free(eid); continue; }
        if (count == max) { free(eid); seterr(err, errlen, "too many shapes"); goto fail; }
        SvgShape *s = &out[count];
        memset(s, 0, sizeof *s);
        s->turn = -1;
        snprintf(s->id, sizeof s->id, "%s", eid ? eid : (is_path ? "path" : "circle"));
        free(eid);
        int rc;
        if (is_path) {
            s->kind = SVG_PATH;
            char *d = get_attr(t, end, "d");
            if (!d) { seterr(err, errlen, "<path> has no d attribute"); goto fail; }
            rc = parse_path_d(d, m, s, err, errlen);
            free(d);
        } else {
            s->kind = SVG_CIRCLE;
            rc = parse_circle(t, end, m, s, err, errlen);
        }
        if (rc) { svg_shape_free(s); goto fail; }
        count++;
    }
    if (count == 0) {
        seterr(err, errlen, id ? "no element with that id" : "no <path> or <circle>");
        return -1;
    }
    return count;
fail:
    for (int i = 0; i < count; i++) svg_shape_free(&out[i]);
    return -1;
}

char *svg_read_file(const char *file)
{
    FILE *f = fopen(file, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = (sz > 0 && sz <= (1 << 24)) ? malloc((size_t)sz + 1) : NULL;
    if (text) text[fread(text, 1, (size_t)sz, f)] = 0;
    fclose(f);
    return text;
}

int svg_load_shapes(const char *file, const char *id, SvgShape *out, int max,
                    char *err, size_t errlen)
{
    char *text = svg_read_file(file);
    if (!text) { seterr(err, errlen, "cannot read file"); return -1; }
    int rc = svg_parse_shapes(text, id, out, max, err, errlen);
    free(text);
    return rc;
}

void svg_shape_free(SvgShape *s)
{
    free(s->pts);
    s->pts = NULL;
    s->n = 0;
}

double svg_resample(const Pt2 *pts, int n, Pt2 *out, int nout)
{
    double *cum = malloc((size_t)n * sizeof *cum);
    if (!cum) return 0;
    cum[0] = 0;
    for (int i = 1; i < n; i++)
        cum[i] = cum[i - 1] + hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
    double total = cum[n - 1];
    int j = 0;
    for (int k = 0; k < nout; k++) {
        double s = total * k / nout;
        while (j < n - 2 && cum[j + 1] < s) j++;
        double seg = cum[j + 1] - cum[j];
        double t = seg > 0 ? (s - cum[j]) / seg : 0;
        out[k].x = pts[j].x + t * (pts[j + 1].x - pts[j].x);
        out[k].y = pts[j].y + t * (pts[j + 1].y - pts[j].y);
    }
    free(cum);
    return total;
}
