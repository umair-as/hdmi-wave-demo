/* Audio output: one raylib AudioStream fed by a synthesis callback that
 * plays the odd-harmonic square-wave partial sum. All parameter changes are
 * smoothed per sample in the callback, so scene changes, pause and harmonic
 * additions fade instead of clicking. */
#ifndef HWD_AUDIO_H
#define HWD_AUDIO_H

#include <stdbool.h>

#define AUDIO_MAX_TERMS 8          /* odd harmonics 1, 3, ..., 15 */

bool audio_init(void);             /* no-op (false) when muted or headless */
void audio_shutdown(void);
void audio_set_active(bool on);    /* a scene wants sound */
void audio_set_paused(bool paused);
void audio_set_terms(int nterms);  /* harmonics 1..(2*nterms-1) audible */
void audio_set_fundamental(float hz);

#endif
