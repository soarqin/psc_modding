#pragma once

#include <stdint.h>

/* Read game id from disc, using CD-ROM mode2 type1 */
extern int cdrom_get_game_id(const char *filename, char *gameid);
