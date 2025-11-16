#pragma once
#include <file.h>

int devtab_lookup_major(struct inode *ip);
void devtab_add_entry(struct inode *ip, const char *path);
void devtab_load();
