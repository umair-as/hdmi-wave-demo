/* hdmi-wave-demo: attract-mode physics/maths scenes for an HDMI screen. */
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "app.h"
#include "audio.h"
#include "draw.h"
#include "raylib.h"
#include "scene.h"
#include "rlgl.h"
#include "thermal.h"

#define SHOT_W 1920
#define SHOT_H 1080
#define SHOT_FRAMES 120
#define SHOT_DT 0.25f      /* 120 x 0.25 s = 30 s of scene time per shot */

static volatile sig_atomic_t quit_signal;

/* raylib's SDL backend times frames with SDL_GetTicks (1 ms steps); the
 * statistics use the monotonic clock instead. */
static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + ts.tv_nsec * 1e-9;
}
static void on_signal(int sig) { quit_signal = sig; }

typedef struct Options {
    int scene;              /* 0-based start scene */
    bool fullscreen;
    int win_w, win_h;
    const char *shot_dir;
    float duration;         /* exit after this many seconds (0 = run forever) */
    int render_w, render_h; /* fixed internal render size, scaled to the window (0 = window size) */
    bool stats;
} Options;

static void usage(FILE *f)
{
    fprintf(f,
        "usage: hdmi-wave-demo [options]\n"
        "  --scene N           start at scene N (1-%d)\n"
        "  --cycle SECONDS     attract-mode time per scene (default 45)\n"
        "  --fullscreen        fullscreen (default on the drm build)\n"
        "  --windowed WxH      window size (default 1280x720)\n"
        "  --mute              no audio output\n"
        "  --svg FILE[#ID]     shapes for the epicycle scene (repeatable)\n"
        "  --seed N            seed for the random parts (default 12345)\n"
        "  --headless-shot DIR render %d frames per scene at %dx%d, write DIR/sceneN.png, exit\n"
        "  --duration SECONDS  quit after SECONDS (tests, benchmarks)\n"
        "  --render WxH        draw at a fixed WxH offscreen and scale to the window\n"
        "  --stats             print frame-time statistics (per second, and per scene on change/exit)\n"
        "keys: 1-%d scene, SPACE pause, LEFT/RIGHT prev/next, ESC quit\n",
        hwd_scene_count, SHOT_FRAMES, SHOT_W, SHOT_H, hwd_scene_count);
}

static int parse_args(int argc, char **argv, Options *o)
{
    *o = (Options){ .win_w = 1280, .win_h = 720 };
#if defined(HWD_DRM)
    o->fullscreen = true;
#endif
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
#define NEED_VALUE() do { if (!v) { fprintf(stderr, "%s needs a value\n", a); return -1; } i++; } while (0)
        if (!strcmp(a, "--scene")) {
            NEED_VALUE();
            o->scene = atoi(v) - 1;
            if (o->scene < 0 || o->scene >= hwd_scene_count) { fprintf(stderr, "bad --scene %s\n", v); return -1; }
        } else if (!strcmp(a, "--cycle")) {
            NEED_VALUE();
            g_app.cycle = (float)atof(v);
            if (!(g_app.cycle >= 1.0f)) { fprintf(stderr, "bad --cycle %s\n", v); return -1; }
        } else if (!strcmp(a, "--fullscreen")) {
            o->fullscreen = true;
        } else if (!strcmp(a, "--windowed")) {
            NEED_VALUE();
            if (sscanf(v, "%dx%d", &o->win_w, &o->win_h) != 2 || o->win_w < 64 || o->win_h < 64) {
                fprintf(stderr, "bad --windowed %s (want WxH)\n", v); return -1;
            }
            o->fullscreen = false;
        } else if (!strcmp(a, "--mute")) {
            g_app.mute = true;
        } else if (!strcmp(a, "--svg")) {
            NEED_VALUE();
            if (g_app.nsvg == HWD_MAX_SVG) { fprintf(stderr, "too many --svg\n"); return -1; }
            g_app.svg[g_app.nsvg++] = v;
        } else if (!strcmp(a, "--seed")) {
            NEED_VALUE();
            g_app.seed = (uint32_t)strtoul(v, NULL, 0);
        } else if (!strcmp(a, "--headless-shot")) {
            NEED_VALUE();
            o->shot_dir = v;
        } else if (!strcmp(a, "--duration")) {
            NEED_VALUE();
            o->duration = (float)atof(v);
        } else if (!strcmp(a, "--render")) {
            NEED_VALUE();
            if (sscanf(v, "%dx%d", &o->render_w, &o->render_h) != 2 || o->render_w < 64 || o->render_h < 64) {
                fprintf(stderr, "bad --render %s (want WxH)\n", v); return -1;
            }
        } else if (!strcmp(a, "--stats")) {
            o->stats = true;
        } else if (!strcmp(a, "-h") || !strcmp(a, "--help")) {
            usage(stdout); exit(0);
        } else {
            fprintf(stderr, "unknown option %s\n", a); usage(stderr); return -1;
        }
#undef NEED_VALUE
    }
    return 0;
}

/* ---- overlay --------------------------------------------------------- */

static void draw_overlay(int idx, int w, int h, float scene_t, bool paused, const char *fps_text, double now)
{
    const Scene *s = hwd_scenes[idx];
    float k = draw_scale(h), pad = 24 * k;
    char line[160];
    snprintf(line, sizeof line, "%d/%d  %s", idx + 1, hwd_scene_count, s->name);
    DrawRectangle(0, 0, w, (int)(112 * k), Fade(COL_BG, 0.82f));
    draw_text(line, pad, pad * 0.8f, 40 * k, COL_TEXT);
    draw_text(s->formula, pad, pad * 0.8f + 48 * k, 28 * k, COL_TEAL);

    float c;
    char right[96];
    if (thermal_read(now, &c)) snprintf(right, sizeof right, "%s   SoC %.1f C", fps_text, c);
    else snprintf(right, sizeof right, "%s   SoC n/a", fps_text);
    draw_text(right, w - pad - draw_text_width(right, 28 * k), pad, 28 * k, COL_DIM);

    char hint[160];
    snprintf(hint, sizeof hint, "1-%d scene   SPACE pause%s%s%s", hwd_scene_count,
             s->keys_help ? "   " : "", s->keys_help ? s->keys_help : "", paused ? "   [PAUSED]" : "");
    draw_text(hint, pad, h - pad - 22 * k, 22 * k, paused ? COL_AMBER : COL_DIM);
    float frac = fminf(scene_t / g_app.cycle, 1.0f);
    DrawRectangle(0, h - (int)(4 * k + 1), (int)(w * frac), (int)(4 * k + 1), Fade(COL_TEAL, 0.6f));
}

/* ---- frame statistics ------------------------------------------------ */

#define STAT_MAX 8192
typedef struct Stats { float dt[STAT_MAX], work[STAT_MAX]; int n; } Stats;
static Stats stats;

static int cmpf(const void *a, const void *b)
{
    float x = *(const float *)a, y = *(const float *)b;
    return (x > y) - (x < y);
}

static void stats_flush(int idx, int w, int h)
{
    if (stats.n < 2) { stats.n = 0; return; }
    int n = stats.n - 1;                 /* drop the first frame (scene init) */
    float *dt = stats.dt + 1, *wk = stats.work + 1;
    double sd = 0, sw = 0;
    for (int i = 0; i < n; i++) { sd += dt[i]; sw += wk[i]; }
    qsort(dt, (size_t)n, sizeof *dt, cmpf);
    qsort(wk, (size_t)n, sizeof *wk, cmpf);
    printf("stats scene=%d %dx%d frames=%d interval_ms mean=%.2f p50=%.2f p99=%.2f max=%.2f | "
           "cpu_ms mean=%.2f p99=%.2f max=%.2f\n",
           idx + 1, w, h, n, 1e3 * sd / n, 1e3 * dt[n / 2], 1e3 * dt[(int)(n * 0.99)], 1e3 * dt[n - 1],
           1e3 * sw / n, 1e3 * wk[(int)(n * 0.99)], 1e3 * wk[n - 1]);
    fflush(stdout);
    stats.n = 0;
}

/* ---- headless screenshots -------------------------------------------- */

static int headless_shots(const char *dir)
{
    g_app.headless = true;
    mkdir(dir, 0755);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(320, 180, "hdmi-wave-demo (headless)");
    if (!IsWindowReady()) { fprintf(stderr, "headless: no GL context\n"); return 1; }
    RenderTexture2D rt = LoadRenderTexture(SHOT_W, SHOT_H);
    int rc = 0;
    for (int i = 0; i < hwd_scene_count && !rc; i++) {
        const Scene *s = hwd_scenes[i];
        s->init();
        float t = 0;
        for (int f = 0; f < SHOT_FRAMES; f++) {
            s->update(SHOT_DT);
            t += SHOT_DT;
            BeginTextureMode(rt);
            ClearBackground(COL_BG);
            s->draw(SHOT_W, SHOT_H);
            draw_overlay(i, SHOT_W, SHOT_H, t, false, "fixed dt", GetTime());
            EndTextureMode();
        }
        Image img = LoadImageFromTexture(rt.texture);
        ImageFlipVertical(&img);
        char path[1024];
        snprintf(path, sizeof path, "%s/scene%d.png", dir, i + 1);
        if (!ExportImage(img, path)) { fprintf(stderr, "headless: cannot write %s\n", path); rc = 1; }
        else printf("headless: wrote %s\n", path);
        UnloadImage(img);
        s->unload();
    }
    UnloadRenderTexture(rt);
    CloseWindow();
    return rc;
}

/* ---- interactive loop ------------------------------------------------ */

int main(int argc, char **argv)
{
    Options o;
    if (parse_args(argc, argv, &o)) return 2;
    if (o.shot_dir) return headless_shots(o.shot_dir);

    struct sigaction sa = { .sa_handler = on_signal };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    unsigned flags = FLAG_VSYNC_HINT | FLAG_MSAA_4X_HINT;
    if (o.fullscreen) flags |= FLAG_FULLSCREEN_MODE;
    else flags |= FLAG_WINDOW_RESIZABLE;
    SetConfigFlags(flags);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(o.fullscreen ? 0 : o.win_w, o.fullscreen ? 0 : o.win_h, "hdmi-wave-demo");
    if (!IsWindowReady()) { fprintf(stderr, "hdmi-wave-demo: cannot open a window\n"); return 1; }
    SetTargetFPS(0);
    if (o.fullscreen) HideCursor();
    printf("hdmi-wave-demo %s (%s build): %dx%d\n", HWD_VERSION, HWD_PLATFORM_NAME,
           GetScreenWidth(), GetScreenHeight());
    fflush(stdout);
    audio_init();

    RenderTexture2D rt = { 0 };
    if (o.render_w) {
        rt = LoadRenderTexture(o.render_w, o.render_h);
        SetTextureFilter(rt.texture, TEXTURE_FILTER_BILINEAR);
    }
    int cur = o.scene;
    hwd_scenes[cur]->init();
    double last_frame = now_s();
    float scene_t = 0;
    bool paused = false;
    double start = GetTime();

    while (!WindowShouldClose() && !quit_signal) {
        double frame_start = now_s(), interval = frame_start - last_frame;
        last_frame = frame_start;
        if (o.duration > 0 && GetTime() - start >= o.duration) break;
        float dt = fminf(GetFrameTime(), 0.1f);
        int sw = GetScreenWidth(), sh = GetScreenHeight();
        int w = o.render_w ? o.render_w : sw, h = o.render_w ? o.render_h : sh;

        int next = cur;
        for (int k; (k = GetKeyPressed()) != 0;) {
            if (k >= KEY_ONE && k < KEY_ONE + hwd_scene_count) next = k - KEY_ONE;
            else if (k == KEY_SPACE) { paused = !paused; audio_set_paused(paused); }
            else if (k == KEY_RIGHT) next = (cur + 1) % hwd_scene_count;
            else if (k == KEY_LEFT) next = (cur + hwd_scene_count - 1) % hwd_scene_count;
            else if (hwd_scenes[cur]->key) hwd_scenes[cur]->key(k);
        }
        if (!paused) {
            scene_t += dt;
            if (scene_t >= g_app.cycle && next == cur) next = (cur + 1) % hwd_scene_count;
        }
        if (next != cur) {
            if (o.stats) stats_flush(cur, w, h);
            hwd_scenes[cur]->unload();
            cur = next;
            hwd_scenes[cur]->init();
            scene_t = 0;
        } else if (!paused) {
            hwd_scenes[cur]->update(dt);
        }

        char fps[32];
        snprintf(fps, sizeof fps, "%d FPS", GetFPS());
        if (o.render_w) BeginTextureMode(rt);
        else BeginDrawing();
        ClearBackground(COL_BG);
        hwd_scenes[cur]->draw(w, h);
        draw_overlay(cur, w, h, scene_t, paused, fps, GetTime());
        if (o.render_w) {
            EndTextureMode();
            BeginDrawing();
            ClearBackground(BLACK);
            float s = fminf((float)sw / w, (float)sh / h);
            DrawTexturePro(rt.texture, (Rectangle){ 0, 0, (float)w, -(float)h },
                           (Rectangle){ (sw - w * s) / 2, (sh - h * s) / 2, w * s, h * s },
                           (Vector2){ 0 }, 0, WHITE);
        }
        if (o.stats && stats.n < STAT_MAX) {
            rlDrawRenderBatchActive();   /* count the GL submission of this frame too */
            stats.dt[stats.n] = (float)interval;
            stats.work[stats.n++] = (float)(now_s() - frame_start);
            /* one line per second: what the scene costs right now */
            static double win_t0, win_max, win_sum;
            static int win_n;
            double wk = stats.work[stats.n - 1];
            if (win_n == 0) win_t0 = frame_start;
            win_sum += wk; win_n++;
            if (wk > win_max) win_max = wk;
            if (frame_start - win_t0 >= 1.0) {
                printf("stats-1s scene=%d t=%.1f frames=%d cpu_ms mean=%.2f max=%.2f\n",
                       cur + 1, scene_t, win_n, 1e3 * win_sum / win_n, 1e3 * win_max);
                win_n = 0; win_sum = win_max = 0;
            }
        }
        EndDrawing();
    }
    if (o.stats) stats_flush(cur, o.render_w ? o.render_w : GetScreenWidth(), o.render_w ? o.render_h : GetScreenHeight());
    hwd_scenes[cur]->unload();
    if (o.render_w) UnloadRenderTexture(rt);
    audio_shutdown();
    CloseWindow();
    if (quit_signal) printf("hdmi-wave-demo: %s, clean exit\n", quit_signal == SIGTERM ? "SIGTERM" : "SIGINT");
    return 0;
}
