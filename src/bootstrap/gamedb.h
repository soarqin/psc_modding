#pragma once

typedef struct gamedb_t gamedb_t;

typedef struct game_list_t game_list_t;

extern gamedb_t *gamedb_open(const char *filename);
extern void gamedb_clear(gamedb_t *db);

extern void gamedb_write(gamedb_t *db, game_list_t *list);
extern void gamedb_read(gamedb_t *db, game_list_t *list);

extern void gamedb_close(gamedb_t *db);
