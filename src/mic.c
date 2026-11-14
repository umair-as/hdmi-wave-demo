#define _GNU_SOURCE
#include "mic.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#if defined(HWD_HAVE_MINIAUDIO)
/* Must match the configuration raudio.c compiled miniaudio with, so the
 * structures declared here have the same layout as the compiled code. */
#define MA_NO_JACK
#define MA_NO_WAV
#define MA_NO_FLAC
#define MA_NO_MP3
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
#define MA_NO_GENERATION
#include "miniaudio.h"

#define RING 16384                 /* power of two */

static ma_context ctx;
static ma_device dev;
static bool ctx_ok, dev_ok;
static float ring[RING];
static volatile unsigned long long written;
static char name[256];

static void on_data(ma_device *d, void *out, const void *in, ma_uint32 frames)
{
    (void)d; (void)out;
    const float *s = in;
    unsigned long long w = written;
    for (ma_uint32 i = 0; i < frames; i++) ring[(w + i) & (RING - 1)] = s[i];
    written = w + frames;
}

bool mic_open(void)
{
    if (ma_context_init(NULL, 0, NULL, &ctx) != MA_SUCCESS) return false;
    ctx_ok = true;
    ma_device_info *cap = NULL;
    ma_uint32 ncap = 0;
    if (ma_context_get_devices(&ctx, NULL, NULL, &cap, &ncap) != MA_SUCCESS) { mic_close(); return false; }
    int pick = -1;
    for (ma_uint32 i = 0; i < ncap; i++)
        if (strcasestr(cap[i].name, "usb")) { pick = (int)i; break; }
    if (pick < 0) { mic_close(); return false; }
    ma_device_config cfg = ma_device_config_init(ma_device_type_capture);
    cfg.capture.pDeviceID = &cap[pick].id;
    cfg.capture.format = ma_format_f32;
    cfg.capture.channels = 1;
    cfg.sampleRate = 48000;
    cfg.dataCallback = on_data;
    snprintf(name, sizeof name, "%s", cap[pick].name);
    if (ma_device_init(&ctx, &cfg, &dev) != MA_SUCCESS) { mic_close(); return false; }
    dev_ok = true;
    if (ma_device_start(&dev) != MA_SUCCESS) { mic_close(); return false; }
    written = 0;
    return true;
}

void mic_close(void)
{
    if (dev_ok) ma_device_uninit(&dev);
    if (ctx_ok) ma_context_uninit(&ctx);
    dev_ok = ctx_ok = false;
}

int mic_rate(void) { return dev_ok ? (int)dev.sampleRate : 48000; }
const char *mic_name(void) { return name; }

bool mic_latest(float *dst, int n)
{
    unsigned long long w = written;
    if (!dev_ok || n > RING || w < (unsigned long long)n) return false;
    for (int i = 0; i < n; i++) dst[i] = ring[(w - (unsigned long long)n + i) & (RING - 1)];
    return true;
}
#else
bool mic_open(void) { return false; }
void mic_close(void) {}
int mic_rate(void) { return 48000; }
bool mic_latest(float *dst, int n) { (void)dst; (void)n; return false; }
const char *mic_name(void) { return ""; }
#endif
