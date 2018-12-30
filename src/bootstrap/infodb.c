#include "infodb.h"

#include <sqlite3.h>

#include <stdlib.h>
#include <string.h>

typedef struct infodb_t {
    sqlite3 *sql;
} infodb_t;

infodb_t *infodb_open(const char *filename) {
    infodb_t *db = (infodb_t*)malloc(sizeof(infodb_t));
    if (filename == NULL || sqlite3_open_v2(filename, &db->sql, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        free(db);
        return NULL;
    }
    return db;
}

void infodb_close(infodb_t *db) {
    if (!db) return;
    if (db->sql) sqlite3_close(db->sql);
    free(db);
}

int infodb_query(infodb_t *db, const char *game_id, game_info_t *info) {
    sqlite3_stmt *stmt;
    memset(info, 0, sizeof(game_info_t));
    if (sqlite3_prepare_v2(db->sql, "SELECT `discno`,`name`,`date`,`publisher`,`players`,`discs` FROM `games` WHERE `id`=?", -1, &stmt, NULL) != SQLITE_OK)
        return -1;
    sqlite3_bind_text(stmt, 1, game_id, -1, NULL);
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return -1;
    }
    info->disc_no = sqlite3_column_int(stmt, 0);
    strncpy(info->title, (const char*)sqlite3_column_text(stmt, 1), 255);
    strncpy(info->date, (const char*)sqlite3_column_text(stmt, 2), 31);
    strncpy(info->publisher, (const char*)sqlite3_column_text(stmt, 3), 255);
    info->players = sqlite3_column_int(stmt, 4);
    strncpy(info->discs, (const char*)sqlite3_column_text(stmt, 5), 255);
    sqlite3_finalize(stmt);
    return 0;
}
