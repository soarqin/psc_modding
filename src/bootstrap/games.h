#pragma once

#include <stdint.h>
#include <stddef.h>
#include <limits.h>

typedef struct {
    int disc_no, group;
    char fullpath[PATH_MAX];
    char game_id[32];
} disc_t;

typedef struct {
    char title[256];
    char publisher[256];
    char game_id[32];
    int players;
    int year;
    char disc_set[256];
    size_t discs_count, discs_cap;
    disc_t *discs;
} game_t;

typedef struct game_list_t {
    size_t games_count, games_cap;
    game_t *games;
    int dirty;
} game_list_t;

/* scan games dir and return game list */
extern game_list_t *games_scan_dir(const char *dirname, const char *conf_file, const char *infodb_file);

/* clear game list data */
extern void game_list_clear(game_list_t *list);

/* free game list */
extern void game_list_free(game_list_t *list);
