#include "games.h"

#include "games_cache.h"
#include "infodb.h"

#include "scanner.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char fullpath[PATH_MAX];
    int group;
} disc_scan_t;

typedef struct {
    disc_scan_t *discs;
    int discs_count, discs_cap;
    int group;
    char group_path[PATH_MAX];
} disc_info_t;

static void scan_callback(void *user, const char *dirname, const char *filename, int level) {
    disc_info_t *di;
    disc_scan_t *d;
    size_t len = strlen(filename);
    /* check .bin/cue/img file */
    if (len < 4 || (strcasecmp(filename + len - 4, ".bin") != 0 && strcasecmp(filename + len - 4, ".iso") != 0 && strcasecmp(filename + len - 4, ".img") != 0)) return;
    di = (disc_info_t*)user;
    if (di->discs_count >= di->discs_cap) {
        di->discs_cap += 8;
        di->discs = realloc(di->discs, sizeof(disc_scan_t) * di->discs_cap);
        if (di->discs == NULL) return;
    }
    d = &di->discs[di->discs_count++];
    memset(d, 0, sizeof(disc_scan_t));
    snprintf(d->fullpath, PATH_MAX, "%s/%s", dirname, filename);
    if (level > 0) {
        if (strcmp(dirname, di->group_path) != 0) {
            d->group = ++di->group;
            strncpy(di->group_path, dirname, PATH_MAX-1);
        } else d->group = di->group;
    }
}

static int games_compare(const void *v1, const void *v2) {
    const game_t *g1 = (const game_t*)v1;
    const game_t *g2 = (const game_t*)v2;
    return strcmp(g1->game_id, g2->game_id);
}

static int discs_compare(const void *v1, const void *v2) {
    const disc_t *d1 = (const disc_t*)v1;
    const disc_t *d2 = (const disc_t*)v2;
    if (d1->disc_no == d2->disc_no) return 0;
    return d1->disc_no < d2->disc_no ? -1 : 1;
}

game_list_t *games_scan_dir(const char *dirname, const char *conf_file, const char *infodb_file) {
    int i;
    games_cache_t *gcache;
    disc_info_t di;
    game_list_t *list = (game_list_t*)calloc(1, sizeof(game_list_t));
    if (list == NULL) return NULL;
    memset(&di, 0, sizeof(disc_info_t));
    scan_dir(dirname, scan_callback, 1, &di);
    if (!di.discs) {
        free(list);
        return NULL;
    }
    gcache = games_cache_load(conf_file);
    if (gcache == NULL) {
        free(list);
        return NULL;
    }
    for (i = 0; i < di.discs_count; ++i) {
        games_cache_try_add(gcache, di.discs[i].fullpath, di.discs[i].group);
    }
    games_cache_check_dirty(gcache);
    list->dirty = gcache->dirty;
    if (gcache->dirty) {
        infodb_t *infodb;
        fprintf(stdout, "[INF] Found new disc, refreshing database... ");
        infodb = infodb_open(infodb_file);
        if (infodb == NULL) {
            free(list);
            fprintf(stdout, "failed (PSX games info db is not found)\n");
            fflush(stdout);
            return NULL;
        }
        for (i = 0; i < gcache->cache_count; ++i) {
            int j, group;
            game_t *this_game = NULL;
            disc_t *this_disc = NULL;
            game_info_t info;
            if (!gcache->caches[i].found) continue;
            if (infodb_query(infodb, gcache->caches[i].game_id, &info) != 0) {
                char fullpath[PATH_MAX], *basepath, *dot;
                info.disc_no = 1;
                strncpy(fullpath, gcache->caches[i].fullpath, PATH_MAX-1);
                basepath = strrchr(fullpath, '/');
                if (basepath == NULL) basepath = fullpath;
                dot = strrchr(basepath, '.');
                if (dot != NULL) *dot = 0;
                strncpy(info.title, basepath, 255);
                strncpy(info.date, "1 January 1999", 31);
                info.publisher[0] = 0;
                info.players = 1;
                strncpy(info.discs, gcache->caches[i].game_id, 255);
            }
            group = gcache->caches[i].group;
            if (group) {
                for (j = 0; this_game == NULL && j < list->games_count; ++j) {
                    int k;
                    game_t *g = &list->games[j];
                    for (k = 0; k < g->discs_count; ++k) {
                        if (g->discs[k].group == group) {
                            this_game = g;
                            break;
                        }
                    }
                }
            } else {
                for (j = 0; j < list->games_count; ++j) {
                    if (strcmp(list->games[j].disc_set, info.discs) == 0 && list->games[j].discs[0].group == 0) {
                        this_game = &list->games[j];
                        break;
                    }
                }
            }
            if (this_game == NULL) {
                const char *year_str;
                if (list->games_count >= list->games_cap) {
                    list->games_cap += 8;
                    list->games = realloc(list->games, list->games_cap * sizeof(game_t));
                }
                this_game = &list->games[list->games_count++];
                memset(this_game, 0, sizeof(game_t));
                strncpy(this_game->title, info.title, 255);
                strncpy(this_game->publisher, info.publisher, 255);
                this_game->players = info.players;
                year_str = strrchr(info.date, ' ');
                this_game->year = year_str == NULL ? 1999 : strtol(year_str + 1, NULL, 10);
                strncpy(this_game->disc_set, info.discs, 255);
            }
            if (this_game->discs_count) {
                int exist = 0;
                for (j = 0; j < this_game->discs_count; ++j) {
                    if (strcmp(this_game->discs[j].game_id, gcache->caches[i].game_id) == 0) {
                        exist = 1;
                        break;
                    }
                }
                if (exist) continue;
            }
            if (this_game->discs_count >= this_game->discs_cap) {
                this_game->discs_cap += 8;
                this_game->discs = realloc(this_game->discs, this_game->discs_cap * sizeof(disc_t));
            }
            this_disc = &this_game->discs[this_game->discs_count++];
            strncpy(this_disc->game_id, gcache->caches[i].game_id, 31);
            strncpy(this_disc->fullpath, gcache->caches[i].fullpath, PATH_MAX-1);
            this_disc->disc_no = info.disc_no;
            this_disc->group = group;
        }
        infodb_close(infodb);
        qsort(list->games, list->games_count, sizeof(game_t), games_compare);
        for (i = 0; i < list->games_count; ++i) {
            game_t *this_game = &list->games[i];
            qsort(this_game->discs, this_game->discs_count, sizeof(disc_t), discs_compare);
            strncpy(this_game->game_id, this_game->discs[0].game_id, 31);
        }
        games_cache_save(gcache, conf_file);
        fprintf(stdout, "done\n");
        fflush(stdout);
    }
    games_cache_free(gcache);
    free(di.discs);
    return list;
}

void game_list_clear(game_list_t *list) {
    if (list->games) {
        int i;
        for (i = 0; i < list->games_count; ++i) {
            if (list->games[i].discs)
                free(list->games[i].discs);
        }
        free(list->games);
    }
    memset(list, 0, sizeof(game_list_t));
}

void game_list_free(game_list_t *list) {
    game_list_clear(list);
    free(list);
}
