#pragma once

typedef struct infodb_t infodb_t;

typedef struct {
    int disc_no;
    char title[256];
    char date[32];
    char publisher[256];
    int players;
    char discs[256];
} game_info_t;

extern infodb_t *infodb_open(const char *filename);
extern void infodb_close(infodb_t *db);
extern int infodb_query(infodb_t *db, const char *game_id, game_info_t *info);
