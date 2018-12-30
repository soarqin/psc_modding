#include "cdrom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cdrom_get_game_id(const char *filename, char *gameid) {
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
        /* Read 2048 bytes of sector */
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
            // Skip '/' after cdrom:
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
    }
    free(buf);
    fclose(fd);
    /* No SYSTEM.CNF was found */
    return -4;
}
