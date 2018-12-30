#include "games_cache.h"

#include "cdrom.h"

#include "ini.h"

#include <stdlib.h>
#include <string.h>

#include <sys/stat.h>

typedef struct {
    games_cache_t *list;
    char section[256];
    game_cache_t *curr;
} ini_user_data;

static int _ini_handler(void* user, const char* section, const char* name, const char* value) {
    ini_user_data *user_data = (ini_user_data*)user;
    if (strcmp(section, user_data->section) != 0) {
        int index = strtol(section, NULL, 10);
        strncpy(user_data->section, section, 255);
        user_data->curr = games_cache_new(user_data->list, index > 0);
    }
    if (user_data->curr == NULL) return 0;
#define MATCH(n) strcmp(name, n) == 0
    if (MATCH("fullpath")) {
        strncpy(user_data->curr->fullpath, value, PATH_MAX-1);
    }
    if (MATCH("game_id")) {
        strncpy(user_data->curr->game_id, value, 31);
    }
    if (MATCH("mtime")) {
        user_data->curr->mtime = strtoull(value, 0, 10);
    }
    if (MATCH("group")) {
        user_data->curr->group = strtol(value, 0, 10);
    }
#undef MATCH
}

games_cache_t *games_cache_load(const char *filename) {
    ini_user_data user_data;
    games_cache_t *list = (games_cache_t*)calloc(1, sizeof(games_cache_t));
    if (!list) return NULL;
    user_data.list = list;
    user_data.section[0] = 0;
    user_data.curr = NULL;
    ini_parse(filename, _ini_handler, &user_data);
    return list;
}

game_cache_t *games_cache_new(games_cache_t *list, int good) {
    game_cache_t *result;
    if (good) {
        if (list->cache_count >= list->cache_cap) {
            list->cache_cap += 8;
            list->caches = realloc(list->caches, list->cache_cap * sizeof(game_cache_t));
        }
        if (!list->caches) return NULL;
        result = &list->caches[list->cache_count++];
        memset(result, 0, sizeof(game_cache_t));
        return result;
    }
    if (list->bad_count >= list->bad_cap) {
        list->bad_cap += 8;
        list->bad_caches = realloc(list->bad_caches, list->bad_cap * sizeof(game_cache_t));
    }
    if (!list->bad_caches) return NULL;
    result = &list->bad_caches[list->bad_count++];
    memset(result, 0, sizeof(game_cache_t));
    return result;
}

game_cache_t *games_cache_try_add(games_cache_t *list, const char *fullpath, int group) {
    int i;
    int was_bad = 0;
    struct stat st;
    game_cache_t *found = NULL;
    for (i = 0; i < list->cache_count; ++i) {
        if (strcmp(list->caches[i].fullpath, fullpath) == 0) {
            list->caches[i].found = stat(fullpath, &st) == 0;
            found = &list->caches[i];
            break;
        }
    }
    if (!found) {
        for (i = 0; i < list->bad_count; ++i) {
            if (strcmp(list->bad_caches[i].fullpath, fullpath) == 0) {
                list->bad_caches[i].found = stat(fullpath, &st) == 0;
                found = &list->bad_caches[i];
                was_bad = 1;
                break;
            }
        }
    }
    if (found) {
        if (st.st_mtime != list->caches[i].mtime || group != list->caches[i].group) {
            char game_id[32];
            int good = cdrom_get_game_id(fullpath, game_id) == 0;
            game_cache_t *conf;
            if (good) {
                if (was_bad) {
                    conf = games_cache_new(list, 1);
                    if (!conf) return NULL;
                    --list->bad_count;
                    memmove(&list->bad_caches[i], &list->bad_caches[i + 1],
                            (list->bad_count - i) * sizeof(game_cache_t));
                } else {
                    conf = found;
                }
                strncpy(conf->game_id, game_id, 31);
                conf->group = group;
            } else {
                if (was_bad) {
                    conf = found;
                } else {
                    conf = games_cache_new(list, 0);
                    if (!conf) return NULL;
                    --list->cache_count;
                    memmove(&list->caches[i], &list->caches[i + 1],
                            (list->cache_count - i) * sizeof(game_cache_t));
                }
                conf->game_id[0] = 0;
            }
            strncpy(conf->fullpath, fullpath, PATH_MAX-1);
            conf->mtime = st.st_mtime;
            list->dirty = 1;
            return conf;
        }
        return NULL;
    } else {
        struct stat st;
        char game_id[32];
        int good;
        game_cache_t *conf;
        if (stat(fullpath, &st) != 0) return NULL;
        good = cdrom_get_game_id(fullpath, game_id) == 0;
        conf = games_cache_new(list, good);
        if (!conf) return NULL;
        conf->found = 1;
        conf->group = group;
        if (good) strncpy(conf->game_id, game_id, 31);
        else conf->game_id[0] = 0;
        strncpy(conf->fullpath, fullpath, PATH_MAX-1);
        conf->mtime = st.st_mtime;
        list->dirty = 1;
        return conf;
    }
}

void games_cache_check_dirty(games_cache_t *list) {
    int i;
    for (i = 0; i < list->cache_count; ++i) {
        if (!list->caches[i].found) { list->dirty = 1; return; }
    }
    for (i = 0; i < list->bad_count; ++i) {
        if (!list->bad_caches[i].found) { list->dirty = 1; return; }
    }
}

void games_cache_save(games_cache_t *list, const char *filename) {
    int i;
    int idx = 0;
    FILE *f = fopen(filename, "wb");
    if (f == NULL) {
        fprintf(stderr, "[ERR] Unable to save game config list to %s", filename);
        return;
    }
    for (i = 0; i < list->cache_count; ++i) {
        if (!list->caches[i].found) { list->dirty = 1; continue; }
        fprintf(f, "[%d]\n", ++idx);
        fprintf(f, "fullpath=%s\n", list->caches[i].fullpath);
        fprintf(f, "game_id=%s\n", list->caches[i].game_id);
        fprintf(f, "mtime=%llu\n", (unsigned long long)list->caches[i].mtime);
        fprintf(f, "group=%d\n", (unsigned long long)list->caches[i].group);
    }
    idx = 0;
    for (i = 0; i < list->bad_count; ++i) {
        if (!list->bad_caches[i].found) continue;
        fprintf(f, "[%d]\n", --idx);
        fprintf(f, "fullpath=%s\n", list->bad_caches[i].fullpath);
        fprintf(f, "mtime=%llu\n", (unsigned long long)list->bad_caches[i].mtime);
    }
    fflush(f);
    fclose(f);
}

void games_cache_free(games_cache_t *list) {
    if (list->caches) free(list->caches);
    if (list->bad_caches) free(list->bad_caches);
    free(list);
}
