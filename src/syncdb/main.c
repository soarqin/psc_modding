#include "ini.h"

#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <sys/stat.h>
#include <dirent.h>

/* (Reserved for future use)
 * Read game id from disc, using CD-ROM mode2 type1 */
int read_game_id(const char *filename, char *gameid) {
    enum {
        SECTOR_SIZE = 2352,
    };
    /* Header + 32bytes ID ("PLAYSTATION" with space paddings) */
    static const char header[] = "\x1" "CD001" "\x1\x0" "PLAYSTATION                     ";
    uint32_t size, offset, offset2, total;
    FILE *fd;
    char *buf = malloc(2048);
    if (buf == NULL) return -5;
    fd = fopen(filename, "rb");
    if (fd == NULL) {
        free(buf);
        return -1;
    }
    fseek(fd, SECTOR_SIZE * 16 + 24, SEEK_SET);
    if (fread(buf, 1, 2048, fd) != 2048) {
        free(buf);
        fclose(fd);
        return -2;
    }
    if (memcmp(buf, header, 40) != 0) {
        free(buf);
        fclose(fd);
        return -3;
    }

    /* Root path record offset and size */
    offset = *(uint32_t*)(buf+158);
    size = *(uint32_t*)(buf+166);
    free(buf);

    /* Read whole root path record */
    buf = malloc(size);
    if (buf == NULL) {
        fclose(fd);
        return -5;
    }
    total = size;
    offset2 = 0u;
    while (offset2 < total) {
        size = offset2 + 2048 < total ? 2048 : total - offset2;
        /* Read 1st 2048 bytes of root path records */
        fseek(fd, offset * SECTOR_SIZE + 24, SEEK_SET);
        if (fread(buf + offset2, 1, size, fd) != size) {
            free(buf);
            fclose(fd);
            return -2;
        }
        offset2 += size;
    }
    offset2 = 0u;
    while (offset2 < total) {
        uint32_t sz = (uint8_t)buf[offset2];
        char *curr;
        if (sz < 33) break;
        if (offset2 + sz > total) break;
        /* Get filename */
        curr = buf + offset2 + 33;
        /* Boot config filename should be "SYSTEM.CNF;1" */
        if (strcasecmp(curr, "SYSTEM.CNF;1") == 0) {
            char buf2[2048], *sub;
            offset = *(uint32_t*)(buf + offset2 + 2);
            size = *(uint32_t*)(buf + offset2 + 10);
            if (size > 2048) size = 2048;
            fseek(fd, offset * SECTOR_SIZE + 24, SEEK_SET);
            if (fread(buf2, 1, size, fd) != size) {
                free(buf);
                fclose(fd);
                return -2;
            }
            /* parse "BOOT = cdrom:[\]IIII-NNN.NN;1" from SYSTEM.CNF */
            sub = strstr(buf2, "BOOT");
            if (sub == NULL) {
                /* Wrong boot config file */
                free(buf);
                fclose(fd);
                return -6;
            }
            sub += 4;
            while(*sub == ' ' || *sub == '\t') ++sub;
            if (*sub != '=') {
                /* Wrong boot config file */
                free(buf);
                fclose(fd);
                return -6;
            }
            ++sub;
            while(*sub == ' ' || *sub == '\t') ++sub;
            if (strncasecmp(sub, "cdrom:", 6) != 0) {
                /* Wrong boot config file */
                free(buf);
                fclose(fd);
                return -6;
            }
            sub += 6;
            while(*sub == '/' || *sub == '\\') ++sub;
            while(*sub != 0 && *sub != ';' && *sub != '\r' && *sub != '\n') {
                switch (*sub) {
                    case '_':
                        *gameid++ = '-';
                    case '.':
                        ++sub;
                        break;
                    default:
                        *gameid++ = *sub++;
                        break;
                }
            }
            *gameid = 0;
            return 0;
        }
        offset2 += sz;
        total -= sz;
    }
    free(buf);
    fclose(fd);
    /* No filename as GameID was found */
    return -4;
}

typedef struct {
    char discs[2048];
    char title[256];
    char publisher[256];
    int players;
    int year;
} game_def_t;

static int _ini_handler(void* user, const char* section, const char* name, const char* value) {
#define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0
    game_def_t *def = (game_def_t*)user;
    if (MATCH("Game", "Discs")) {
        strncpy(def->discs, value, 2047);
        return 1;
    }
    if (MATCH("Game", "Title")) {
        strncpy(def->title, value, 255);
        return 1;
    }
    if (MATCH("Game", "Publisher")) {
        strncpy(def->publisher, value, 255);
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

typedef struct {
    int disc_number;
    char filename[256];
    char game_id[32];
} disc_info_t;

static int disc_info_compare(const void *v1, const void *v2) {
    const disc_info_t *d1 = (const disc_info_t *)v1;
    const disc_info_t *d2 = (const disc_info_t *)v2;
    if (d1->disc_number == d2->disc_number) return strcmp(d1->filename, d2->filename);
    return d1->disc_number < d2->disc_number ? -1 : 1;
}

static int scan_dir(const char *dirname, sqlite3 *sql, game_def_t *def) {
    int i;
    char filename[256] = {};
    int disc_count = 0;
    disc_info_t discs[8];
    struct dirent *d_it;
    DIR *p_dir = opendir(dirname);
    if (p_dir == NULL)
        return -1;
    memset(def, 0, sizeof(game_def_t));
    while(disc_count < 8 && (d_it = readdir(p_dir)) != NULL) {
        sqlite3_stmt *stmt;
        size_t len;
        char query_str[256];
        if (d_it->d_name[0] == '.') continue;
        len = strlen(d_it->d_name);
        if (len < 4 || strcmp(d_it->d_name + len - 4, ".cue") != 0 || strchr(d_it->d_name, ',') != NULL) continue;
        d_it->d_name[len - 4] = 0;
        snprintf(filename, 256, "%s/%s.bin", dirname, d_it->d_name);
        strncpy(discs[disc_count].filename, d_it->d_name, 255);
        if (read_game_id(filename, discs[disc_count].game_id) < 0) continue;
        snprintf(query_str, 256, "SELECT `discno`,`name`,`date`,`publisher`,`players`,`discs` FROM `games` WHERE `id`='%s'", discs[disc_count].game_id);
        if (sqlite3_prepare_v2(sql, query_str, -1, &stmt, NULL) != SQLITE_OK)
            continue;
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *discs_str = (const char*)sqlite3_column_text(stmt, 5);
            if (def->discs[0] != 0) {
                if (strcmp(discs_str, def->discs) != 0) {
                    fprintf(stderr, "More than one game found in %s (%s <-> %s)", dirname, def->discs, discs_str);
                    fflush(stderr);
                    break;
                }
            } else {
                const char *date = (const char*)sqlite3_column_text(stmt, 2);
                char *s = strrchr(date, ' ');
                if (s != NULL) def->year = strtol(s+1, NULL, 10);
                if (def->year == 0) def->year = 1999;
                def->players = sqlite3_column_int(stmt, 4);
                strncpy(def->title, (const char*)sqlite3_column_text(stmt, 1), 255);
                strncpy(def->publisher, (const char*)sqlite3_column_text(stmt, 3), 255);
                strncpy(def->discs, discs_str, 2047);
            }
            discs[disc_count].disc_number = sqlite3_column_int(stmt, 0);
            ++disc_count;
        }
        sqlite3_finalize(stmt);
    }
    closedir(p_dir);
    if (disc_count <= 0) return -1;
    qsort(discs, disc_count, sizeof(disc_info_t), disc_info_compare);
    def->discs[0] = 0;
    for (i = 0; i < disc_count; ++i) {
        if (i == 0)
            strncpy(def->discs, discs[i].filename, 2047);
        else {
            strncat(def->discs, ",", 2047 - strlen(def->discs));
            strncat(def->discs, discs[i].filename, 2047 - strlen(def->discs));
        }
    }
    return 0;
}

/* Sync games to sqlite database */
int database_sync(const char *basedir, const char *dbpath, const char *gamesdb) {
    sqlite3 *sql, *games_sql;
    int i = 0, disc_id = 0;

    /* Open database */
    if (sqlite3_open(dbpath, &sql) != SQLITE_OK)
        return -1;

    /* Try to open games database */
    if (gamesdb == NULL || sqlite3_open_v2(gamesdb, &games_sql, SQLITE_OPEN_READONLY, NULL) != SQLITE_OK) {
        games_sql = NULL;
    }

    /* Delete old data and create tables if needed */
    sqlite3_exec(sql, "DELETE FROM `GAME`", NULL, NULL, NULL);
    sqlite3_exec(sql, "DELETE FROM `DISC`", NULL, NULL, NULL);
    sqlite3_exec(sql, "DELETE FROM `sqlite_sequence`", NULL, NULL, NULL);
    sqlite3_exec(sql, "CREATE TABLE IF NOT EXISTS `GAME` ( `GAME_ID` INTEGER NOT NULL CONSTRAINT `PK_GAME` PRIMARY KEY AUTOINCREMENT, `GAME_TITLE_STRING` TEXT NULL, `PUBLISHER_NAME` TEXT NULL, `RELEASE_YEAR` INTEGER NOT NULL, `PLAYERS` INTEGER NOT NULL, `RATING_IMAGE` TEXT NULL, `GAME_MANUAL_QR_IMAGE` TEXT NULL, `LINK_GAME_ID` TEXT NULL )", NULL, NULL, NULL);
    sqlite3_exec(sql, "CREATE TABLE IF NOT EXISTS `DISC` ( `DISC_ID` INTEGER NOT NULL CONSTRAINT `PK_DISC` PRIMARY KEY AUTOINCREMENT, `GAME_ID` INTEGER NOT NULL, `DISC_NUMBER` INTEGER NOT NULL, `BASENAME` TEXT NULL, CONSTRAINT `FK_DISC_GAME_GAME_ID` FOREIGN KEY (`GAME_ID`) REFERENCES `GAME` (`GAME_ID`) ON DELETE CASCADE )", NULL, NULL, NULL);
    sqlite3_exec(sql, "CREATE TABLE IF NOT EXISTS `LANGUAGE_SPECIFIC` ( `LANGUAGE_ID` TEXT NOT NULL CONSTRAINT `PK_LANGUAGE_SPECIFIC` PRIMARY KEY, `DEFAULT_VALUE` TEXT NULL, `VALUE` TEXT NULL )", NULL, NULL, NULL);

    while (1) {
        int use_ini = 1;
        sqlite3_stmt *stmt = NULL;
        char dirname[256], filename[256];
        game_def_t def = {};
        struct stat st;
        snprintf(dirname, 256, "%s/%d/GameData", basedir, ++i);
        stat(dirname, &st);
        if (!S_ISDIR(st.st_mode))
            break;
        /* Parse ini file */
        snprintf(filename, 256, "%s/%d/GameData/Game.ini", basedir, i);
        if (ini_parse(filename, _ini_handler, &def) < 0) {
            use_ini = 0;
            if (games_sql == NULL || scan_dir(dirname, games_sql, &def) < 0)
                continue;
        }
        /* Insert game data into table */
        if (use_ini)
            fprintf(stdout, "Adding %s (from %s)... ", def.title, filename);
        else
            fprintf(stdout, "Adding %s (from internal database)... ", def.title);
        if (sqlite3_prepare_v2(sql, "INSERT INTO `GAME`(`GAME_ID`,`GAME_TITLE_STRING`,`PUBLISHER_NAME`,`RELEASE_YEAR`,`PLAYERS`,`RATING_IMAGE`,`GAME_MANUAL_QR_IMAGE`,`LINK_GAME_ID`) "
                                    "VALUES(?,?,?,?,?,'CERO_A','QR_Code_GM','')", -1, &stmt, NULL) == SQLITE_OK) {
            int disc_number = 0;
            char *token;
            sqlite3_bind_int(stmt, 1, i);
            sqlite3_bind_text(stmt, 2, def.title, -1, NULL);
            sqlite3_bind_text(stmt, 3, def.publisher, -1, NULL);
            sqlite3_bind_int(stmt, 4, def.year);
            sqlite3_bind_int(stmt, 5, def.players);
            if(sqlite3_step(stmt) != SQLITE_DONE) {
                sqlite3_finalize(stmt);
                fprintf(stdout, "Unable to insert game record: %s (from %s)\n", def.title, filename);
                fflush(stdout);
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
                    sqlite3_bind_int(stmt, 2, i);
                    sqlite3_bind_int(stmt, 3, ++disc_number);
                    sqlite3_bind_text(stmt, 4, token, -1, NULL);
                    if(sqlite3_step(stmt) != SQLITE_DONE) {
                        fprintf(stdout, "Unable to insert disc record: %s (from %s)\n", token, filename);
                        fflush(stdout);
                    }
                    sqlite3_finalize(stmt);
                }
                token = strtok(NULL, ",");
            }
            fprintf(stdout, "Success\n");
            fflush(stdout);
        }
    }
    /* Execute VACUUM to shrink space use */
    sqlite3_exec(sql, "VACUUM", NULL, NULL, NULL);
    sqlite3_close(sql);
    if (games_sql != NULL) sqlite3_close(games_sql);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 3) return -1;
    database_sync(argv[1], argv[2], argc > 3 ? argv[3] : NULL);
    return 0;
}
