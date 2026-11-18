/* Optional live input for the spectrogram: a capture device whose name
 * contains "USB", opened through the miniaudio that raylib compiles in.
 * Without one (or without miniaudio) mic_open fails and the caller uses
 * its synthetic source. */
#ifndef HWD_MIC_H
#define HWD_MIC_H

#include <stdbool.h>

bool mic_open(void);
void mic_close(void);
int mic_rate(void);
/* Copy the most recent n samples (oldest first). Returns false if fewer
 * than n have been captured so far. */
bool mic_latest(float *dst, int n);
const char *mic_name(void);

#endif
