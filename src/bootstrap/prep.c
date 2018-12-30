#include "prep.h"

#include "games.h"
#include "cover.h"
#include "cfg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <fts.h>
#include <sys/mount.h>

int try_mount(const char *from, const char *to) {
    while(umount2(to, MNT_DETACH) == 0) usleep(50);
    mount(from, to, NULL, MS_BIND | MS_SILENT, NULL);
}

int unmount(const char *path) {
    while(umount2(path, MNT_DETACH) == 0) usleep(50);
}

int mount_with_base(const char *from_base, const char *to_base, const char *subdir) {
    char path_from[PATH_MAX], path_to[PATH_MAX];
    snprintf(path_from, PATH_MAX, "%s/%s", from_base, subdir);
    snprintf(path_to, PATH_MAX, "%s/%s", to_base, subdir);
    return try_mount(path_from, path_to);
}

int symlink_with_base(const char *from_base, const char *to_base, const char *subdir) {
    char path_from[PATH_MAX], path_to[PATH_MAX];
    snprintf(path_from, PATH_MAX, "%s/%s", from_base, subdir);
    snprintf(path_to, PATH_MAX, "%s/%s", to_base, subdir);
    return symlink(path_from, path_to);
}

int rmdir_recursive(const char *dir) {
    int ret = 0;
    FTS *ftsp = NULL;
    FTSENT *curr;

    char *files[] = { (char *) dir, NULL };

    ftsp = fts_open(files, FTS_NOCHDIR | FTS_PHYSICAL | FTS_XDEV, NULL);
    if (!ftsp) {
        ret = -1;
        goto finish;
    }

    while ((curr = fts_read(ftsp))) {
        switch (curr->fts_info) {
        case FTS_NS:
        case FTS_DNR:
        case FTS_ERR:
            break;

        case FTS_DC:
        case FTS_DOT:
        case FTS_NSOK:
            break;

        case FTS_D:
            break;

        case FTS_DP:
        case FTS_F:
        case FTS_SL:
        case FTS_SLNONE:
        case FTS_DEFAULT:
            if (remove(curr->fts_accpath) < 0) {
                fprintf(stderr, "%s: Failed to remove: %s\n",
                        curr->fts_path, strerror(errno));
                ret = -1;
            }
            break;
        }
    }

finish:
    if (ftsp) {
        fts_close(ftsp);
    }

    return ret;
}

int mkdir_recursive(const char *file_path, mode_t mode) {
    char *p;
    for (p = strchr(file_path+1, '/'); p; p = strchr(p+1, '/')) {
        *p = '\0';
        if (mkdir(file_path, mode) == -1) {
            if (errno != EEXIST) {
                *p = '/';
                return -1;
            }
        }
        *p = '/';
    }
    return 0;
}

int mkpath(const char *file_path, mode_t mode) {
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s/", file_path);
    return mkdir_recursive(path, mode);
}

int copyfile(const char *from, const char *to) {
    FILE *fin, *fout;
    fin = fopen(from, "rb");
    if (fin == NULL) return -1;
    fout = fopen(to, "wb");
    if (fout == NULL) { fclose(fin); return -1; }
    while(!feof(fin)) {
        enum { BUF_SIZE = 256 * 1024 };
        char tmpbuf[BUF_SIZE];
        size_t size = fread(tmpbuf, 1, BUF_SIZE, fin);
        if (fwrite(tmpbuf, 1, size, fout) < size) break;
    }
    fclose(fout);
    fclose(fin);
}

int copydir(const char *from, const char *to, int recursive) {
    struct dirent *d_it;
    DIR *dir_ptr = opendir(from);
    if (dir_ptr == NULL) {
        return -1;
    }
    mkpath(to, 0755);
    while((d_it = readdir(dir_ptr)) != NULL) {
        struct stat st;
        char from_file[PATH_MAX], to_file[PATH_MAX];
        if (d_it->d_name[0] == '.' && (d_it->d_name[1] == 0 || (d_it->d_name[1] == '.' && d_it->d_name[2] == 0)))
            continue;
        snprintf(from_file, PATH_MAX, "%s/%s", from, d_it->d_name);
        if (stat(from_file, &st) != 0) continue;
        if (S_ISREG(st.st_mode)) {
            snprintf(to_file, PATH_MAX, "%s/%s", to, d_it->d_name);
            copyfile(from_file, to_file);
        } else if (recursive && S_ISDIR(st.st_mode)) {
            snprintf(to_file, PATH_MAX, "%s/%s", to, d_it->d_name);
            copydir(from_file, to_file, recursive);
        }
    }
    closedir(dir_ptr);
    return 0;
}

int mkdir_with_base(const char *basedir, const char *subdir) {
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s/%s", basedir, subdir);
    return mkpath(path, 0755);
}

int copydir_with_base(const char *from_base, const char *to_base, const char *subdir, int recursive) {
    char path_from[PATH_MAX], path_to[PATH_MAX];
    snprintf(path_from, PATH_MAX, "%s/%s", from_base, subdir);
    snprintf(path_to, PATH_MAX, "%s/%s", to_base, subdir);
    return copydir(path_from, path_to, recursive);
}

int check_type(const char *basedir, const char *filename) {
    struct stat st;
    char path[PATH_MAX];
    snprintf(path, PATH_MAX, "%s/%s", basedir, filename);
    if (stat(path, &st) != 0) return -1;
    if (S_ISREG(st.st_mode))
        return 0;
    return 1;
}

void prepare_shell(const char *mod_dir) {
    mkdir_with_base(mod_dir, "gaadata/system/bios");
    mkdir_with_base(mod_dir, "gaadata/preferences");
    mkdir_with_base(mod_dir, "data/AppData/sony/ui");
    mkdir_with_base(mod_dir, "data/AppData/sony/auto_dimmer");
    mkdir_with_base(mod_dir, "gaadata/databases");
    mkdir_with_base(mod_dir, "gaadata/geninfo");
    mkdir_with_base(mod_dir, "logs");
    mkdir_with_base(mod_dir, "data/sony/ui");
    
    if (check_type(mod_dir, "gaadata/system/bios/romw.bin") < 0) {
        copydir_with_base("", mod_dir, "gaadata/system/bios", 0);
    }
    if (check_type(mod_dir, "gaadata/preferences/regional.pre") < 0) {
        copydir_with_base("", mod_dir, "gaadata/preferences", 0);
    }
    if (check_type(mod_dir, "data/AppData/sony/ui/user.pre") < 0) {
        copydir_with_base("", mod_dir, "data/AppData/sony/ui", 0);
    }
    if (check_type(mod_dir, "data/AppData/sony/auto_dimmer/config.cnf") < 0) {
        copydir_with_base("", mod_dir, "data/AppData/sony/auto_dimmer", 0);
    }
    if (check_type(mod_dir, "gaadata/geninfo/REGION") < 0) {
        copydir_with_base("", mod_dir, "gaadata/geninfo", 0);
    }
    if (check_type(mod_dir, "data/sony/ui") < 0) {
        copydir_with_base("", mod_dir, "data/sony/ui", 0);
    }
    sync();

    unmount("/gaadata");
    unmount("/data");
    mkpath("/tmp/mod/gaadata", 0755);
    mkpath("/tmp/mod/data", 0755);

    mount_with_base("/tmp/mod", "", "gaadata");
    mount_with_base("/tmp/mod", "", "data");
    mount_with_base(mod_dir, "", "etc/udev/rules.d/20-joystick.rules");


    symlink_with_base(mod_dir, "/tmp/mod", "gaadata/databases");
    symlink_with_base(mod_dir, "/tmp/mod", "gaadata/geninfo");
    symlink_with_base(mod_dir, "/tmp/mod", "gaadata/preferences");
    symlink_with_base(mod_dir, "/tmp/mod", "gaadata/system");

    mkpath("/tmp/mod/data/sony/sgmo", 0755);
    mkpath("/tmp/mod/data/AppData/sony/pcsx", 0755);
    symlink_with_base(mod_dir, "/tmp/mod", "data/AppData/sony/ui");
    symlink_with_base(mod_dir, "/tmp/mod", "data/AppData/sony/auto_dimmer");
    symlink_with_base(mod_dir, "/tmp/mod", "data/sony/ui");
    symlink("/tmp/diag", "/tmp/mod/data/sony/sgmo/diag");
    symlink("/dev/shm/power", "/tmp/mod/data/power");
    symlink("/usr/sony/bin/plugins", "/tmp/mod/data/AppData/sony/pcsx/plugins");

    system("udevadm control --reload-rules");
    system("udevadm trigger");
}

void symlink_games(const char *mod_dir, const char *cover_zip, const game_list_t *game_list) {
    int i;
    copydir_with_base("/usr/sony/share/recovery", "/tmp/mod/data", "AppData/sony/pcsx", 1);
    for (i = 0; i < game_list->games_count; ++i) {
        char path_base[PATH_MAX];
        char file_from[PATH_MAX], file_to[PATH_MAX];
        game_t *g = &game_list->games[i];
        int j;
        struct stat st;
        snprintf(path_base, PATH_MAX, "%s/var/config/%s/.pcsx", mod_dir, g->game_id);
        mkpath(path_base, 0755);
        snprintf(file_to, PATH_MAX, "%s/pcsx.cfg", path_base);
        if (stat(file_to, &st) != 0) {
            snprintf(file_from, PATH_MAX, "%s/share/defaults/pcsx.cfg", mod_dir);
            copyfile(file_from, file_to);
        }
        strncpy(file_from, path_base, PATH_MAX-1);
        snprintf(path_base, PATH_MAX, "/tmp/mod/data/AppData/sony/pcsx/%d", i+1);
        mkpath(path_base, 0755);
        snprintf(file_to, PATH_MAX, "%s/.pcsx", path_base);
        rmdir_recursive(file_to);
        symlink(file_from, file_to);
        snprintf(path_base, PATH_MAX, "/tmp/mod/gaadata/%d", i+1);
        mkdir(path_base, 0755);
        snprintf(file_to, PATH_MAX, "%s/pcsx.cfg", path_base);
        if (stat(file_to, &st) != 0) {
            snprintf(file_from, PATH_MAX, "%s/share/defaults/pcsx.cfg", mod_dir);
            copyfile(file_from, file_to);
        }
        symlink(file_from, file_to);
        for (j = 0; j < g->discs_count; ++j) {
            char file_base[PATH_MAX], *dot;
            disc_t *d = &g->discs[j];
            snprintf(file_to, PATH_MAX, "%s/%s.bin", path_base, d->game_id);
            symlink(d->fullpath, file_to);
            strncpy(file_base, d->fullpath, PATH_MAX);
            if((dot = strrchr(file_base, '.')) != NULL) *dot = 0;
            snprintf(file_from, PATH_MAX, "%s.cue", file_base);
            if (stat(file_from, &st) != 0) {
                int fd = open(file_from, O_RDWR | O_CREAT, 0755);
                char content[PATH_MAX+256];
                snprintf(content, PATH_MAX+256, "FILE \"%s.bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00", d->game_id);
                write(fd, content, strlen(content));
                if (fd >= 0) {
                    close(fd);
                }
            }
            snprintf(file_to, PATH_MAX, "%s/%s.cue", path_base, d->game_id);
            symlink(file_from, file_to);
            if (j == 0) {
                snprintf(file_from, PATH_MAX, "%s.png", file_base);
                if (stat(file_from, &st) != 0) {
                    if (cover_extract(cover_zip, g->game_id, file_from) != 0) {
                        snprintf(file_to, PATH_MAX, "%s/share/defaults/cover.png", mod_dir);
                        copyfile(file_to, file_from);
                    }
                }
                snprintf(file_to, PATH_MAX, "%s/%s.png", path_base, d->game_id);
                symlink(file_from, file_to);
                snprintf(file_from, PATH_MAX, "%s.lic", file_base);
                if (stat(file_from, &st) == 0) {
                    snprintf(file_to, PATH_MAX, "%s/%s.lic", path_base, d->game_id);
                    symlink(file_from, file_to);
                }
            }
        }
    }
    sync();
    sync();
}

int ui_menu_run() {
    char *const argv[] = {"--power-off-enable", NULL};
    if (g_cfg.menu_shortcut) setenv("PCSX_ESC_KEY","2",1);
    chdir("/data/AppData/sony/pcsx");
    return execv("/usr/sony/bin/ui_menu", argv);
}
