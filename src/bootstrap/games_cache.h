#pragma once

#include <stdint.h>
#include <time.h>
#include <limits.h>

typedef struct {
    char fullpath[PATH_MAX];
    char game_id[32];
    time_t mtime;
    int group;
    int found;
} game_cache_t;

typedef struct {
    int dirty;
    size_t cache_count, cache_cap;
    game_cache_t *caches;
    size_t bad_count, bad_cap;
    game_cache_t *bad_caches;
} games_cache_t;

// Read game conf from file
extern games_cache_t *games_cache_load(const char *filename);

// Add a new empty game conf to list
extern game_cache_t *games_cache_new(games_cache_t *list, int good);

// Try add game to list, if not exists or mtime is newer than original
extern game_cache_t *games_cache_try_add(games_cache_t *list, const char *fullpath, int group);

// Check for games cache dirty
extern void games_cache_check_dirty(games_cache_t *list);

// Save game conf to file
extern void games_cache_save(games_cache_t *list, const char *filename);

// Free game conf list
extern void games_cache_free(games_cache_t *list);
