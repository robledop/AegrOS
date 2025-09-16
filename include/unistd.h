#pragma once

#include <attributes.h>
#include <stddef.h>

int open(const char name[static 1], int mode);
int close(int fd);
NON_NULL int read(int fd, void *ptr, unsigned int size);
int write(int fd, const char *buffer, size_t size);
int lseek(int fd, int offset, int whence);

int ftruncate(int fd, int length);

int isatty(int fd);