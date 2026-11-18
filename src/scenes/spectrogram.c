/* Scene 5: spectrogram waterfall. A Hann-windowed radix-2 FFT of the latest
 * block of samples becomes one texture row per update; the texture is a
 * ring of rows, drawn in two pieces so no pixels move on the CPU.
 * Source: a USB microphone if one is present (not in --headless-shot),
 * otherwise a synthetic chirp plus tones plus a little noise. */
#include <complex.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "app.h"
#include "draw.h"
#include "fft.h"
#include "mic.h"
#include "scene.h"

#define COLS 512                  /* frequency columns 0..FMAX */
#define ROWS 360                  /* history rows */
#define FMAX 8000.0
#define SYN_RATE 16000
#define SYN_N 2048
#define MIC_N 4096
#define DB_FLOOR -80.0
#define CHIRP_S 8.0               /* chirp 200 Hz -> 7 kHz, repeating */

static Texture2D tex;
static Color row[COLS];
static Color lut[256];
static int head;                  /* next row to write */
static double t;
static bool use_mic;
static int n_fft, rate;
static double complex *buf;
static float *samples, *win;
static float peak_hz;

static Color heat(float u)        /* black -> blue -> teal -> yellow -> white */
{
    static const float st[][3] = {
        { 12, 16, 24 }, { 30, 40, 120 }, { 40, 170, 170 }, { 250, 210, 70 }, { 255, 255, 240 } };
    float x = u * 4.0f;
    int i = (int)x;
    if (i >= 4) return (Color){ 255, 255, 240, 255 };
    float f = x - i;
    return (Color){ (unsigned char)(st[i][0] + f * (st[i + 1][0] - st[i][0])),
                    (unsigned char)(st[i][1] + f * (st[i + 1][1] - st[i][1])),
                    (unsigned char)(st[i][2] + f * (st[i + 1][2] - st[i][2])), 255 };
}

/* deterministic noise from the sample index and the seed */
static float noise(long i)
{
    uint32_t x = (uint32_t)i * 2654435761u ^ g_app.seed;
    x ^= x >> 15; x *= 0x2c1b3c6du; x ^= x >> 12; x *= 0x297a2d39u; x ^= x >> 15;
    return (float)(x >> 8) / 8388608.0f - 1.0f;
}

static float synth_sample(long i)
{
    double ts = (double)i / SYN_RATE, tc = fmod(ts, CHIRP_S);
    double f0 = 200, f1 = 7000;
    double chirp = sin(2 * M_PI * (f0 * tc + (f1 - f0) * tc * tc / (2 * CHIRP_S)));
    double tone1 = sin(2 * M_PI * 1000 * ts);
    double gate = fmod(ts, 2.0) < 1.0 ? 1.0 : 0.0;          /* 2.5 kHz beeps */
    double tone2 = gate * sin(2 * M_PI * 2500 * ts);
    /* FM tone: f(t) = 4500 + 300 sin(pi t) Hz, phase = 2 pi * integral of f */
    double tone3 = 0.5 * sin(2 * M_PI * 4500 * ts - 600.0 * cos(M_PI * ts));
    return (float)(0.5 * chirp + 0.2 * tone1 + 0.25 * tone2 + 0.15 * tone3 + 0.003 * noise(i));
}

static void init(void)
{
    t = 0;
    head = 0;
    use_mic = !g_app.headless && mic_open();
    if (use_mic) {
        rate = mic_rate();
        n_fft = MIC_N;
        printf("hdmi-wave-demo: spectrogram source: microphone \"%s\" at %d Hz\n", mic_name(), rate);
    } else {
        rate = SYN_RATE;
        n_fft = SYN_N;
        printf("hdmi-wave-demo: spectrogram source: synthetic (no USB capture device)\n");
    }
    fflush(stdout);
    buf = malloc(sizeof *buf * n_fft);
    samples = malloc(sizeof *samples * n_fft);
    win = malloc(sizeof *win * n_fft);
    for (int i = 0; i < n_fft; i++) win[i] = (float)hann(i, n_fft);
    for (int i = 0; i < 256; i++) lut[i] = heat(i / 255.0f);
    Image img = { .width = COLS, .height = ROWS, .mipmaps = 1,
                  .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 };
    img.data = calloc((size_t)COLS * ROWS, 4);
    for (int i = 0; i < COLS * ROWS; i++) ((Color *)img.data)[i] = lut[0];
    tex = LoadTextureFromImage(img);
    free(img.data);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
}

static void update(float dt)
{
    t += dt;
    if (use_mic) {
        if (!mic_latest(samples, n_fft)) return;
    } else {
        long end = (long)(t * SYN_RATE);
        for (int i = 0; i < n_fft; i++) samples[i] = synth_sample(end - n_fft + i);
    }
    double wsum = 0;
    for (int i = 0; i < n_fft; i++) { buf[i] = samples[i] * win[i]; wsum += win[i]; }
    fft_inplace(buf, n_fft);
    /* column c covers [c, c+1) * FMAX / COLS; take the strongest bin in it */
    double best = -1;
    int bins = n_fft / 2;
    for (int c = 0; c < COLS; c++) {
        int b0 = (int)(c * FMAX / COLS * n_fft / rate), b1 = (int)((c + 1) * FMAX / COLS * n_fft / rate);
        if (b1 <= b0) b1 = b0 + 1;
        if (b1 > bins) b1 = bins;
        double m = 0;
        for (int b = b0; b < b1; b++) {
            double a = cabs(buf[b]);
            if (a > m) m = a;
            if (a > best) { best = a; peak_hz = (float)b * rate / n_fft; }
        }
        double db = 20 * log10(2 * m / wsum + 1e-12);
        double u = (db - DB_FLOOR) / -DB_FLOOR;
        row[c] = lut[u <= 0 ? 0 : u >= 1 ? 255 : (int)(u * 255)];
    }
    UpdateTextureRec(tex, (Rectangle){ 0, (float)head, COLS, 1 }, row);
    head = (head + 1) % ROWS;
}

static void draw(int w, int h)
{
    float k = draw_scale(h);
    Rectangle area = { w * 0.08f, h * 0.15f, w * 0.88f, h * 0.74f };
    /* newest row at the top. A negative source height draws rows [y, y+h)
     * flipped, so row head-1 lands on top, then head-2 ... 0, ROWS-1 ... head. */
    float rh = area.height / ROWS;
    int newer = head;                     /* rows [0, head) are the newest, top part */
    DrawTexturePro(tex, (Rectangle){ 0, 0, COLS, -(float)newer },
                   (Rectangle){ area.x, area.y, area.width, rh * newer }, (Vector2){ 0 }, 0, WHITE);
    DrawTexturePro(tex, (Rectangle){ 0, (float)newer, COLS, -(float)(ROWS - newer) },
                   (Rectangle){ area.x, area.y + rh * newer, area.width, rh * (ROWS - newer) },
                   (Vector2){ 0 }, 0, WHITE);
    DrawRectangleLinesEx(area, 2 * k, COL_AXIS);
    for (int f = 0; f <= 8000; f += 1000) {
        float x = area.x + area.width * f / (float)FMAX;
        DrawLineEx((Vector2){ x, area.y + area.height }, (Vector2){ x, area.y + area.height + 10 * k }, 2 * k, COL_AXIS);
        char lab[16];
        snprintf(lab, sizeof lab, "%d kHz", f / 1000);
        draw_text(lab, x - draw_text_width(lab, 22 * k) / 2, area.y + area.height + 16 * k, 22 * k, COL_DIM);
    }
    char buf2[160];
    snprintf(buf2, sizeof buf2, "source: %s   N = %d, fs = %d Hz, Hann   peak %.0f Hz   (newest at top)",
             use_mic ? mic_name() : "synthetic chirp + tones", n_fft, rate, peak_hz);
    draw_text(buf2, area.x, area.y - 36 * k, 24 * k, COL_DIM);
}

static void unload(void)
{
    if (use_mic) mic_close();
    UnloadTexture(tex);
    free(buf); free(samples); free(win);
    buf = NULL; samples = win = NULL;
}

const Scene scene_spectrogram = {
    .name = "Spectrogram waterfall",
    .formula = "X_k = sum_n w_n x_n e^(-2 pi i k n / N),  w_n = 0.5 - 0.5 cos(2 pi n / (N-1))  (radix-2 FFT)",
    .init = init, .update = update, .draw = draw, .unload = unload,
};
