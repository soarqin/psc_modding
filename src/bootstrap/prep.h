#pragma once

typedef struct game_list_t game_list_t;

extern void prepare_shell(const char *mod_dir);
extern void symlink_games(const char *mod_dir, const char *cover_zip, const game_list_t *game_list);
extern int ui_menu_run();
