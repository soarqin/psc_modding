#include "games.h"
#include "gamedb.h"
#include "prep.h"
#include "cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>

int main(int argc, char *argv[]) {
    const struct option opts[] = {
            {"games", 1, NULL, 'g'},
            {"cache", 1, NULL, 'c'},
            {"gamedb", 1, NULL, 'd'},
            {"infodb", 1, NULL, 'i'},
            {"moddir", 1, NULL, 'm'},
            {"covers", 1, NULL, 'v'},
    };
    const char *games_dir = "/media/games";
    const char *mod_dir = "/media/mod";
    char cache_file[PATH_MAX] = {};
    char gamedb_file[PATH_MAX] = {};
    char infodb_file[PATH_MAX] = {};
    char covers_file[PATH_MAX] = {};
    char config_file[PATH_MAX] = {};
    int c;
    while((c = getopt_long(argc, argv, "c:d:g:i:m:v:", opts, NULL)) != -1) {
        switch(c) {
            case 'g':
                games_dir = optarg;
                break;
            case 'm':
                mod_dir = optarg;
                break;
            case 'c':
                strncpy(cache_file, optarg, PATH_MAX-1);
                break;
            case 'd':
                strncpy(gamedb_file, optarg, PATH_MAX-1);
                break;
            case 'i':
                strncpy(infodb_file, optarg, PATH_MAX-1);
                break;
            case 'v':
                strncpy(covers_file, optarg, PATH_MAX-1);
                break;
            case '?':
                fprintf(stderr, "Unknown option: %s\n", argv[optind - 1]);
                break;
            default:
                break;
        }
    }
    snprintf(config_file, PATH_MAX, "%s/etc/config.ini", mod_dir);
    if (cache_file[0] == 0) snprintf(cache_file, PATH_MAX, "%s/var/cache/cache.ini", mod_dir);
    if (gamedb_file[0] == 0) snprintf(gamedb_file, PATH_MAX, "%s/gaadata/databases/regional.db", mod_dir);
    if (infodb_file[0] == 0) snprintf(infodb_file, PATH_MAX, "%s/share/db/psx_games.db", mod_dir);
    if (covers_file[0] == 0) snprintf(covers_file, PATH_MAX, "%s/share/db/covers.zip", mod_dir);
    cfg_load(config_file);
#if !defined(_WIN32)
    prepare_shell(mod_dir);
#endif
    {
        game_list_t *game_list = games_scan_dir(games_dir, cache_file, infodb_file);
        gamedb_t *gamedb;
        if (game_list == NULL) return -1;
        gamedb = gamedb_open(gamedb_file);
        if (gamedb == NULL) {
            game_list_free(game_list);
            return -1;
        }
        if (game_list->dirty) {
            gamedb_clear(gamedb);
            gamedb_write(gamedb, game_list);
        } else {
            gamedb_read(gamedb, game_list);
        }
        gamedb_close(gamedb);
#if !defined(_WIN32)
        symlink_games(mod_dir, covers_file, game_list);
#endif
        game_list_free(game_list);
    }
    return
#if defined(_WIN32)
    0;
#else
    ui_menu_run();
#endif
}
