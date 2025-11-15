#pragma once

#include <stddef.h>
#include "types.h"

int memcmp(const void *lhs, const void *rhs, size_t count);
void *memcpy(void *dst, const void *src, size_t count);
void *memmove(void *dst, const void *src, size_t count);
void *memset(void *dst, int c, size_t count);
char *safestrcpy(char *, const char *, int);
char *strcpy(char *dest, const char *src);
size_t strlen(const char *);
size_t strnlen(const char *s, size_t maxlen);
int strncmp(const char *, const char *, size_t);
int strcmp(const char *, const char *);
char *strncpy(char *, const char *, size_t);
bool starts_with(const char pre[static 1], const char str[static 1]);
bool str_ends_with(const char *str, const char *suffix);
char *strcat(char dest[static 1], const char src[static 1]);
char *strncat(char dest[static 1], const char src[static 1], size_t n);
char *strstr(const char *haystack, const char *needle);
void reverse(char s[static 1]);
int itoa(int n, char s[static 1]);
char *strchr(const char *s, int c);
char *strrchr(const char *s, int c);
char *strtok(char *str, const char delim[static 1]);
int sscanf(const char *str, const char *format, ...);
