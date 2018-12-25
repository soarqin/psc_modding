#include "ini.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* (Reserved for future use)
 * Read game id from disc, using CD-ROM mode2 type1 */
int read_game_id(const char *filename, char *gameid) {
    enum {
        SECTOR_SIZE = 2352,
    };
    /* Header + 32bytes ID ("PLAYSTATION" with space paddings) */
    static const char header[] = "\x1" "CD001" "\x1\x0" "PLAYSTATION                     ";
    char buf[2048 * 2];
    uint32_t size, offset, offset2, total;
    FILE *fd = fopen(filename, "rb");
    if (fd == NULL) return -1;
    fseek(fd, SECTOR_SIZE * 16 + 24, SEEK_SET);
    if (fread(buf, 1, 2048, fd) != 2048) {
        fclose(fd);
        return -2;
    }
    if (memcmp(buf, header, 40) != 0) {
        fclose(fd);
        return -3;
    }

    /* Path table size and offset */
    size = *(uint32_t*)(buf+132);
    offset = *(uint32_t*)(buf+140);

    /* Read path table */
    fseek(fd, SECTOR_SIZE * offset + 24, SEEK_SET);
    if (size < 8 || fread(buf, 1, size, fd) < size) {
        fclose(fd);
        return -2;
    }

    /* Read 1st 2048 bytes of root path records */
    offset = *(uint32_t*)(buf+2) * SECTOR_SIZE + 24;
    fseek(fd, offset, SEEK_SET);
    if (fread(buf, 1, 2048, fd) != 2048) {
        fclose(fd);
        return -2;
    }
    offset2 = 0u;
    total = 2048u;
    while (1) {
        uint32_t sz = (uint8_t)buf[offset2];
        char *n, *curr;
        if (sz < 33) break;
        /* Read next 2048 bytes of root path records if needed */
        if (offset2 + sz > total) {
            uint32_t left = total - offset2;
            memmove(buf, buf + offset2, left);
            offset += SECTOR_SIZE;
            fseek(fd, offset, SEEK_SET);
            if (fread(buf + left, 1, 2048, fd) < 2048) {
                fclose(fd);
                return -2;
            }
            total = left + 2048u;
            offset2 = 0u;
        }
        /* Get filename */
        curr = buf + offset2 + 33;
        /* Remove trail ';' and file index from filename */
        n = strchr(curr, ';');
        if (n != NULL) *n = 0;
        /* Check filename format S[CL][PEU]?_NNN.NN */
        if (curr[0] == 'S' && (curr[1] == 'C' || curr[1] == 'L') && (curr[2] == 'P' || curr[2] == 'E' || curr[2] == 'U') && curr[4] == '_'
            && curr[5] >= '0' && curr[5] <= '9' && curr[6] >= '0' && curr[6] <= '9' && curr[7] >= '0' && curr[7] <= '9' && curr[8] == '.'
            && curr[9] >= '0' && curr[9] <= '9' && curr[10] >= '0' && curr[10] <= '9' && curr[11] == '0') {
            strcpy(gameid, curr);
            /* Remove '.' */
            memmove(gameid + 8, gameid + 9, strlen(gameid + 9) + 1);
            /* Replace '_' with '-' */
            gameid[4] = '-';
            fclose(fd);
            return 0;
        }
        offset2 += sz;
        total -= sz;
    }
    /* No filename as GameID was found */
    fclose(fd);
    return -4;
}

typedef struct {
    char discs[256];
    char title[256];
    char publisher[256];
    int players;
    int year;
} game_def_t;

static int _ini_handler(void* user, const char* section, const char* name, const char* value) {
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    game_def_t *def = (game_def_t*)user;
    if (MATCH("Game", "Discs")) {
        strncpy(def->discs, value, 256);
        return 1;
    }
    if (MATCH("Game", "Title")) {
        strncpy(def->title, value, 256);
        return 1;
    }
    if (MATCH("Game", "Publisher")) {
        strncpy(def->publisher, value, 256);
        return 1;
    }
    if (MATCH("Game", "Players")) {
        def->players = strtol(value, NULL, 10);
        return 1;
    }
    if (MATCH("Game", "Year")) {
        def->year = strtol(value, NULL, 10);
        return 1;
    }
    return 0;
#undef MATCH
}

/* Sync games to sqlite database */
int database_sync(const char *basedir, const char *dbpath) {
    sqlite3 *sql;
    int i = 0, game_id = 0, disc_id = 0;

    /* Open database */
    if (sqlite3_open(dbpath, &sql) != SQLITE_OK)
        return -1;

    /* Delete old data and create tables if needed */
    sqlite3_exec(sql, "DELETE FROM `GAME`", NULL, NULL, NULL);
    sqlite3_exec(sql, "DELETE FROM `DISC`", NULL, NULL, NULL);
    sqlite3_exec(sql, "DELETE FROM `sqlite_sequence`", NULL, NULL, NULL);
    sqlite3_exec(sql, "CREATE TABLE IF NOT EXISTS `GAME` ( `GAME_ID` INTEGER NOT NULL CONSTRAINT `PK_GAME` PRIMARY KEY AUTOINCREMENT, `GAME_TITLE_STRING` TEXT NULL, `PUBLISHER_NAME` TEXT NULL, `RELEASE_YEAR` INTEGER NOT NULL, `PLAYERS` INTEGER NOT NULL, `RATING_IMAGE` TEXT NULL, `GAME_MANUAL_QR_IMAGE` TEXT NULL, `LINK_GAME_ID` TEXT NULL )", NULL, NULL, NULL);
    sqlite3_exec(sql, "CREATE TABLE IF NOT EXISTS `DISC` ( `DISC_ID` INTEGER NOT NULL CONSTRAINT `PK_DISC` PRIMARY KEY AUTOINCREMENT, `GAME_ID` INTEGER NOT NULL, `DISC_NUMBER` INTEGER NOT NULL, `BASENAME` TEXT NULL, CONSTRAINT `FK_DISC_GAME_GAME_ID` FOREIGN KEY (`GAME_ID`) REFERENCES `GAME` (`GAME_ID`) ON DELETE CASCADE )", NULL, NULL, NULL);
    sqlite3_exec(sql, "CREATE TABLE IF NOT EXISTS `LANGUAGE_SPECIFIC` ( `LANGUAGE_ID` TEXT NOT NULL CONSTRAINT `PK_LANGUAGE_SPECIFIC` PRIMARY KEY, `DEFAULT_VALUE` TEXT NULL, `VALUE` TEXT NULL )", NULL, NULL, NULL);

    while (1) {
        sqlite3_stmt *stmt = NULL;
        char filename[256];
        game_def_t def = {};
        /* Parse ini file */
        snprintf(filename, 256, "%s/%d/GameData/Game.ini", basedir, ++i);
        if (ini_parse(filename, _ini_handler, &def) < 0)
            break;
        /* Insert game data into table */
        printf("Adding %s (from %s)...\n", def.title, filename);
        if (sqlite3_prepare_v2(sql, "INSERT INTO `GAME`(`GAME_ID`,`GAME_TITLE_STRING`,`PUBLISHER_NAME`,`RELEASE_YEAR`,`PLAYERS`,`RATING_IMAGE`,`GAME_MANUAL_QR_IMAGE`,`LINK_GAME_ID`) "
                                    "VALUES(?,?,?,?,?,'CERO_A','QR_Code_GM','')", -1, &stmt, NULL) == SQLITE_OK) {
            int disc_number = 0;
            char *token;
            sqlite3_bind_int(stmt, 1, ++game_id);
            sqlite3_bind_text(stmt, 2, def.title, -1, NULL);
            sqlite3_bind_text(stmt, 3, def.publisher, -1, NULL);
            sqlite3_bind_int(stmt, 4, def.year);
            sqlite3_bind_int(stmt, 5, def.players);
            if(sqlite3_step(stmt) != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                printf("Unable to insert game record: %s (from %s)\n", def.title, filename);
                continue;
            }
            sqlite3_finalize(stmt);
            /* Split discs */
            token = strtok(def.discs, ",");
            while(token != NULL) {
                /* Insert disc data into table */
                if (sqlite3_prepare_v2(sql, "INSERT INTO `DISC`(`DISC_ID`,`GAME_ID`,`DISC_NUMBER`,`BASENAME`) "
                                            "VALUES(?,?,?,?)", -1, &stmt, NULL) == SQLITE_OK) {
                    sqlite3_bind_int(stmt, 1, ++disc_id);
                    sqlite3_bind_int(stmt, 2, game_id);
                    sqlite3_bind_int(stmt, 3, ++disc_number);
                    sqlite3_bind_text(stmt, 4, token, -1, NULL);
                    if(sqlite3_step(stmt) != SQLITE_DONE) {
                        printf("Unable to insert disc record: %s (from %s)\n", token, filename);
                    }
                    sqlite3_finalize(stmt);
                }
                token = strtok(NULL, ",");
            }
        }
    }
    /* Execute VACUUM to shrink space use */
    sqlite3_exec(sql, "VACUUM", NULL, NULL, NULL);
    sqlite3_close(sql);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) return -1;
    database_sync(argv[1], argv[2]);
    return 0;
}
