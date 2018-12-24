#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

enum {
    SECTOR_SIZE = 2352,
};

static const char header[] = "\x1" "CD001" "\x1\x0" "PLAYSTATION                     ";

int read_game_id(const char *filename, char *gameid) {
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
    size = *(uint32_t*)(buf+132);
    offset = *(uint32_t*)(buf+140);
    fseek(fd, SECTOR_SIZE * offset + 24, SEEK_SET);
    if (size < 8 || fread(buf, 1, size, fd) < size) {
        fclose(fd);
        return -2;
    }
    offset = *(uint32_t*)(buf+2) * SECTOR_SIZE + 24;
    fseek(fd, offset, SEEK_SET);
    if (fread(buf, 1, 2048, fd) != 2048) {
        fclose(fd);
        return -2;
    }
    offset2 = 0u;
    total = 2048u;
    while(1) {
        uint32_t sz = (uint8_t)buf[offset2];
        char *n, *curr;
        if (sz < 33) break;
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
        curr = buf + offset2 + 33;
        n = strchr(curr, ';');
        if (n != NULL) *n = 0;
        if (curr[0] == 'S' && curr[1] == 'L' && curr[4] == '_' && curr[5] >= '0' && curr[5] <= '9' && curr[8] == '.') {
            strcpy(gameid, curr);
            memmove(gameid + 8, gameid + 9, strlen(gameid + 9) + 1);
            gameid[4] = '-';
            fclose(fd);
            return 0;
        }
        offset2 += sz;
        total -= sz;
    }
    fclose(fd);
    return -4;
}

int database_sync() {
    sqlite3 *sql;
    if (sqlite3_open("F:\\regional.db", &sql) != SQLITE_OK)
        return -1;
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(sql, "SELECT `DISC_ID`,`GAME_ID`,`DISC_NUMBER`,`BASENAME` FROM `DISC`", -1, &stmt, NULL) == SQLITE_OK) {
        while(sqlite3_step(stmt) == SQLITE_ROW) {
            printf("%d\n", sqlite3_column_int(stmt, 0));
            printf("%d\n", sqlite3_column_int(stmt, 1));
            printf("%d\n", sqlite3_column_int(stmt, 2));
            printf("%s\n", (const char *)sqlite3_column_text(stmt, 3));
        }
        sqlite3_finalize(stmt);
    }
    sqlite3_exec(sql, "VACUUM", NULL, NULL, NULL);
    sqlite3_close(sql);
    return 0;
}

int main() {
    char gameid[16];
    if (read_game_id("E:\\Emu\\PS1\\isos\\FFT.ISO", gameid) == 0)
        printf("%s\n", gameid);
    database_sync();
    return 0;
}
