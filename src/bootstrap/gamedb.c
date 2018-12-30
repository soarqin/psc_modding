#include "gamedb.h"

#include "games.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct gamedb_t {
    sqlite3 *sql;
} gamedb_t;

gamedb_t *gamedb_open(const char *filename) {
    gamedb_t *db = (gamedb_t*)malloc(sizeof(gamedb_t));
    if (filename == NULL || sqlite3_open(filename, &db->sql) != SQLITE_OK) {
        free(db);
        return NULL;
    }
    sqlite3_exec(db->sql, "ALTER TABLE `DISC` ADD `FULLPATH` TEXT NULL", NULL, NULL, NULL);
    return db;
}

void gamedb_clear(gamedb_t *db) {
    /* Delete old data and create tables if needed */
    sqlite3_exec(db->sql, "DELETE FROM `GAME`", NULL, NULL, NULL);
    sqlite3_exec(db->sql, "DELETE FROM `DISC`", NULL, NULL, NULL);
    sqlite3_exec(db->sql, "DELETE FROM `sqlite_sequence`", NULL, NULL, NULL);
    sqlite3_exec(db->sql, "CREATE TABLE IF NOT EXISTS `GAME` ( `GAME_ID` INTEGER NOT NULL CONSTRAINT `PK_GAME` PRIMARY KEY AUTOINCREMENT, `GAME_TITLE_STRING` TEXT NULL, `PUBLISHER_NAME` TEXT NULL, `RELEASE_YEAR` INTEGER NOT NULL, `PLAYERS` INTEGER NOT NULL, `RATING_IMAGE` TEXT NULL, `GAME_MANUAL_QR_IMAGE` TEXT NULL, `LINK_GAME_ID` TEXT NULL )", NULL, NULL, NULL);
    sqlite3_exec(db->sql, "CREATE TABLE `DISC` (`GAME_ID` integer, `DISC_NUMBER` integer, `BASENAME` text, `FULLPATH` TEXT NULL, UNIQUE (`GAME_ID`, `DISC_NUMBER`))", NULL, NULL, NULL);
    sqlite3_exec(db->sql, "CREATE TABLE IF NOT EXISTS `LANGUAGE_SPECIFIC` ( `LANGUAGE_ID` TEXT NOT NULL CONSTRAINT `PK_LANGUAGE_SPECIFIC` PRIMARY KEY, `DEFAULT_VALUE` TEXT NULL, `VALUE` TEXT NULL )", NULL, NULL, NULL);
}

void gamedb_read(gamedb_t *db, game_list_t *list) {
    sqlite3_stmt *stmt;
    int lastidx = -1;
    game_t *this_game = NULL;
    game_list_clear(list);
    if (sqlite3_prepare_v2(db->sql, "SELECT `GAME_ID`,`DISC_NUMBER`,`BASENAME`,`FULLPATH` FROM `DISC` ORDER BY `GAME_ID`,`DISC_NUMBER`", -1, &stmt, NULL) != SQLITE_OK) return;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        disc_t *disc;
        int gidx = sqlite3_column_int(stmt, 0);
        int didx = sqlite3_column_int(stmt, 1);
        if (gidx != lastidx) {
            lastidx = gidx;
            if (list->games_count >= list->games_cap) {
                list->games_cap += 8;
                list->games = realloc(list->games, list->games_cap * sizeof(game_t));
            }
            this_game = &list->games[list->games_count++];
            strncpy(this_game->game_id, sqlite3_column_text(stmt, 2), 31);
        }
        if (!this_game) continue;
        if (this_game->discs_count >= this_game->discs_cap) {
            this_game->discs_cap += 8;
            this_game->discs = realloc(this_game->discs, this_game->discs_cap * sizeof(disc_t));
        }
        disc = &this_game->discs[this_game->discs_count++];
        strncpy(disc->game_id, sqlite3_column_text(stmt, 2), 31);
        strncpy(disc->fullpath, sqlite3_column_text(stmt, 3), 31);
    }
    sqlite3_finalize(stmt);
}

void gamedb_write(gamedb_t *db, game_list_t *list) {
    int i;
    for (i = 0; i < list->games_count; ++i) {
        int j;
        char discs[256];
        sqlite3_stmt *stmt;
        game_t *game = &list->games[i];
        fprintf(stdout, "Writing %s [%s] to database... ", game->title, game->game_id);
        if (sqlite3_prepare_v2(db->sql,
                               "INSERT INTO `GAME`(`GAME_ID`,`GAME_TITLE_STRING`,`PUBLISHER_NAME`,`RELEASE_YEAR`,`PLAYERS`,`RATING_IMAGE`,`GAME_MANUAL_QR_IMAGE`,`LINK_GAME_ID`) "
                               "VALUES(?,?,?,?,?,'CERO_A','QR_Code_GM','')", -1, &stmt, NULL) != SQLITE_OK) continue;
        sqlite3_bind_int(stmt, 1, i + 1);
        sqlite3_bind_text(stmt, 2, game->title, -1, NULL);
        sqlite3_bind_text(stmt, 3, game->publisher, -1, NULL);
        sqlite3_bind_int(stmt, 4, game->year);
        sqlite3_bind_int(stmt, 5, game->players);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            sqlite3_finalize(stmt);
            fprintf(stderr, "[ERR] Unable to insert game record: %s\n", game->title);
            fflush(stderr);
            continue;
        }
        sqlite3_finalize(stmt);
        for (j = 0; j < game->discs_count; ++j) {
            disc_t *disc = &game->discs[j];
            if (sqlite3_prepare_v2(db->sql, "INSERT INTO `DISC`(`GAME_ID`,`DISC_NUMBER`,`BASENAME`,`FULLPATH`) "
                                        "VALUES(?,?,?,?)", -1, &stmt, NULL) != SQLITE_OK) continue;
            sqlite3_bind_int(stmt, 1, i + 1);
            sqlite3_bind_int(stmt, 2, j + 1);
            sqlite3_bind_text(stmt, 3, disc->game_id, -1, NULL);
            sqlite3_bind_text(stmt, 4, disc->fullpath, -1, NULL);
            if (sqlite3_step(stmt) != SQLITE_DONE) {
                fprintf(stderr, "[ERR] Unable to insert disc record: %s (from %s)\n", disc->game_id, disc->fullpath);
                fflush(stderr);
            }
            sqlite3_finalize(stmt);
        }
        fprintf(stdout, "Success\n");
        fflush(stdout);
    }
}

void gamedb_close(gamedb_t *db) {
    if (!db) return;
    if (db->sql) {
        sqlite3_exec(db->sql, "VACUUM", NULL, NULL, NULL);
        sqlite3_close(db->sql);
    }
    free(db);
}
