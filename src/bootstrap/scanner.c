#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>

#include <dirent.h>
#include <sys/stat.h>

void scan_dir(const char *dirname, void (*callback)(void*, const char*, const char*), void *user) {
    struct dirent *d_it;
    DIR *dir_ptr = opendir(dirname);
    if (dir_ptr == NULL) {
        return;
    }
    while((d_it = readdir(dir_ptr)) != NULL) {
        /* skip files/dirs started with '.' */
        if (d_it->d_name[0] == '.') continue;
        callback(user, dirname, d_it->d_name);
    }
    closedir(dir_ptr);
}
