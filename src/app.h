/* Process-wide settings shared by main and the scenes. */
#ifndef HWD_APP_H
#define HWD_APP_H

#include <stdbool.h>
#include <stdint.h>

#define HWD_MAX_SVG 8

typedef struct AppConfig {
    float cycle;                 /* seconds per scene in attract mode */
    bool mute;                   /* no audio output at all */
    bool headless;               /* --headless-shot: deterministic, no devices */
    uint32_t seed;
    const char *svg[HWD_MAX_SVG];/* --svg PATH, in order; empty = built-in shape */
    int nsvg;
} AppConfig;

extern AppConfig g_app;

/* Deterministic PRNG (xorshift32) seeded from g_app.seed by app_rng_reset. */
void app_rng_reset(uint32_t salt);
uint32_t app_rng_u32(void);
float app_rng_unit(void);        /* [0, 1) */

#endif
