#include "cfg.h"

#include "ini.h"

#include <stdlib.h>
#include <string.h>

cfg_t g_cfg = {};

static int _ini_handler(void* user, const char* section, const char* name, const char* value) {
#define MATCH(m, n) strcmp(section, m) == 0 && strcmp(name, n) == 0
    if (MATCH("mod", "menu_shortcut")) {
        g_cfg.menu_shortcut = strtol(value, 0, 10);
    }
#undef MATCH
}

void cfg_load(const char *filename) {
    ini_parse(filename, _ini_handler, NULL);
}
