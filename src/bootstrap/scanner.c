#include "scanner.h"

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <dirent.h>
#include <sys/stat.h>

static inline void scan_dir_internal(const char *dirname, void (*callback)(void*, const char*, const char*, int), int recursive, void *user, int level) {
    struct dirent *d_it;
    DIR *dir_ptr = opendir(dirname);
    if (dir_ptr == NULL) {
        return;
    }
    while((d_it = readdir(dir_ptr)) != NULL) {
        char fullpath[PATH_MAX];
        struct stat st;
        if (d_it->d_name[0] == '.' && (d_it->d_name[1] == 0 || (d_it->d_name[1] == '.' && d_it->d_name[2] == 0)))
            continue;
        snprintf(fullpath, PATH_MAX, "%s/%s", dirname, d_it->d_name);
        if (stat(fullpath, &st) != 0) continue;
        if (S_ISREG(st.st_mode)) {
            callback(user, dirname, d_it->d_name, level);
        } else if (recursive && S_ISDIR(st.st_mode)) {
            scan_dir_internal(fullpath, callback, recursive, user, level + 1);
        }
    }
    closedir(dir_ptr);
}

void scan_dir(const char *dirname, void (*callback)(void*, const char*, const char*, int), int recursive, void *user) {
    scan_dir_internal(dirname, callback, recursive, user, 0);
}
