/* Scene interface. Each scene is one scene_*.c file exporting one Scene;
 * scenes.c lists them in attract-mode order. Nothing else in the program
 * knows a scene by its id: adding a scene = one new file + one table entry.
 *
 * init:   called when the scene becomes current (state is reset each time)
 * update: advance by dt seconds (fixed or measured); never called while paused
 * draw:   draw into a w x h target; must not change state
 * unload: release what init acquired
 * key:    optional; return true if the key was consumed */
#ifndef HWD_SCENE_H
#define HWD_SCENE_H

#include <stdbool.h>

typedef struct Scene {
    const char *name;
    const char *formula;
    void (*init)(void);
    void (*update)(float dt);
    void (*draw)(int w, int h);
    void (*unload)(void);
    bool (*key)(int key);
    const char *keys_help;   /* optional extra key hint for the overlay */
} Scene;

extern const Scene *const hwd_scenes[];
extern const int hwd_scene_count;

#endif
