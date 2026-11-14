#include "thermal.h"

#include <ctype.h>
#include <glob.h>
#include <stdio.h>
#include <string.h>

static char zone_temp[256];
static int probed;
static double last_read = -1e9;
static float last_c;
static bool last_ok;

static void probe(void)
{
    probed = 1;
    glob_t g;
    if (glob("/sys/class/thermal/thermal_zone*", 0, NULL, &g) != 0) return;
    for (size_t i = 0; i < g.gl_pathc; i++) {
        char path[256], type[64] = "";
        snprintf(path, sizeof path, "%s/type", g.gl_pathv[i]);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        if (!fgets(type, sizeof type, f)) type[0] = 0;
        fclose(f);
        for (char *p = type; *p; p++) *p = (char)tolower((unsigned char)*p);
        if (strstr(type, "cpu") || strstr(type, "soc")) {
            snprintf(zone_temp, sizeof zone_temp, "%s/temp", g.gl_pathv[i]);
            break;
        }
    }
    globfree(&g);
    if (!zone_temp[0])
        snprintf(zone_temp, sizeof zone_temp, "/sys/class/thermal/thermal_zone0/temp");
}

bool thermal_read(double now, float *celsius)
{
    if (!probed) probe();
    if (now - last_read >= 1.0) {
        last_read = now;
        long milli = 0;
        FILE *f = fopen(zone_temp, "r");
        last_ok = f && fscanf(f, "%ld", &milli) == 1;
        if (f) fclose(f);
        last_c = milli / 1000.0f;
    }
    *celsius = last_c;
    return last_ok;
}
