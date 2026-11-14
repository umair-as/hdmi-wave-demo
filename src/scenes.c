#include "scene.h"

extern const Scene scene_spring, scene_square, scene_epicycles,
                   scene_standing, scene_spectrogram, scene_pendulum;

/* Attract-mode order; keys 1..N jump to the entry at that index. */
const Scene *const hwd_scenes[] = {
    &scene_spring,
    &scene_square,
    &scene_epicycles,
    &scene_standing,
    &scene_spectrogram,
    &scene_pendulum,
};
const int hwd_scene_count = (int)(sizeof hwd_scenes / sizeof hwd_scenes[0]);
