#pragma once

typedef struct {
    int menu_shortcut;
} cfg_t;

extern cfg_t g_cfg;

extern void cfg_load(const char *filename);
