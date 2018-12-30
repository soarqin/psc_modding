#include "cover.h"

#include <miniz.h>
#include <stdio.h>
#include <stdlib.h>

int cover_extract(const char *cover_zip, const char *game_id, const char *target_filename) {
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_reader_init_file(&zip, cover_zip, 0)) return -1;
    char internal_name[256];
    snprintf(internal_name, 256, "%s.png", game_id);
    if (!mz_zip_reader_extract_file_to_file(&zip, internal_name, target_filename, 0)) {
        mz_zip_reader_end(&zip);
        return -2;
    }
    mz_zip_reader_end(&zip);
    return 0;
}
