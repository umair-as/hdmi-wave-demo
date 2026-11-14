#ifndef HWD_THERMAL_H
#define HWD_THERMAL_H
/* SoC temperature in degrees C, re-read at most once per second.
 * Zone choice: first whose type contains "cpu" or "soc", else zone0.
 * Returns false (caller shows "n/a") when no zone is readable. */
#include <stdbool.h>
bool thermal_read(double now, float *celsius);
#endif
