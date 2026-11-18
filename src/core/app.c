#include "app.h"

AppConfig g_app = { .cycle = 45.0f, .seed = 12345u };

static uint32_t rng_state = 1u;

void app_rng_reset(uint32_t salt)
{
    rng_state = (g_app.seed ^ (salt * 0x9E3779B9u)) | 1u;
}

uint32_t app_rng_u32(void)
{
    uint32_t x = rng_state;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return rng_state = x;
}

float app_rng_unit(void)
{
    return (float)(app_rng_u32() >> 8) / 16777216.0f;
}
