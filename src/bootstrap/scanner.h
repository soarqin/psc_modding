#pragma once

extern void scan_dir(const char *dirname, void (*callback)(void*, const char*, const char*, int), int recursive, void *user);
