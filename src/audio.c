#include "audio.h"

#include <math.h>
#include <stdio.h>

#include "app.h"
#include "raylib.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RATE 48000
#define LEVEL 0.12f                /* peak of the full square is ~0.14 */
#define FADE_S 0.35f               /* master fade time */
#define TERM_FADE_S 0.25f          /* per-harmonic fade time */

static AudioStream stream;
static bool ready;
static volatile int want_active, want_paused, want_terms = 1;
static volatile float fundamental = 110.0f;

/* callback-thread state */
static double phase;               /* fundamental phase, radians */
static float gain;                 /* master gain 0..1 */
static float term_amp[AUDIO_MAX_TERMS];

static void synth(void *buffer, unsigned int frames)
{
    float *out = buffer;
    const float gstep = 1.0f / (FADE_S * RATE), tstep = 1.0f / (TERM_FADE_S * RATE);
    float target = (want_active && !want_paused) ? 1.0f : 0.0f;
    int nterms = want_terms;
    double dphi = 2.0 * M_PI * fundamental / RATE;
    for (unsigned int i = 0; i < frames; i++) {
        if (gain < target) gain = fminf(target, gain + gstep);
        else if (gain > target) gain = fmaxf(target, gain - gstep);
        float s = 0.0f;
        if (gain > 0.0f) {
            for (int k = 0; k < AUDIO_MAX_TERMS; k++) {
                float t = k < nterms ? 1.0f : 0.0f;
                if (term_amp[k] < t) term_amp[k] = fminf(t, term_amp[k] + tstep);
                else if (term_amp[k] > t) term_amp[k] = fmaxf(t, term_amp[k] - tstep);
                if (term_amp[k] > 0.0f) {
                    int h = 2 * k + 1;
                    s += term_amp[k] * (float)sin(h * phase) / h;
                }
            }
            s *= (float)(4.0 / M_PI) * LEVEL * gain;
        }
        out[i] = s;
        phase += dphi;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
    }
}

bool audio_init(void)
{
    if (g_app.mute || g_app.headless) return false;
    InitAudioDevice();
    if (!IsAudioDeviceReady()) {
        fprintf(stderr, "hdmi-wave-demo: no audio output device, continuing silent\n");
        return false;
    }
    SetAudioStreamBufferSizeDefault(1024);
    stream = LoadAudioStream(RATE, 32, 1);
    SetAudioStreamCallback(stream, synth);
    PlayAudioStream(stream);
    ready = true;
    printf("hdmi-wave-demo: audio output ready (%d Hz, float, mono)\n", RATE);
    fflush(stdout);
    return true;
}

void audio_shutdown(void)
{
    if (!ready) return;
    /* let the master gain reach zero before the device stops */
    want_active = 0;
    WaitTime(FADE_S + 0.05);
    UnloadAudioStream(stream);
    CloseAudioDevice();
    ready = false;
}

void audio_set_active(bool on) { want_active = on; }
void audio_set_paused(bool paused) { want_paused = paused; }
void audio_set_terms(int n)
{
    want_terms = n < 0 ? 0 : n > AUDIO_MAX_TERMS ? AUDIO_MAX_TERMS : n;
}
void audio_set_fundamental(float hz) { fundamental = hz; }
