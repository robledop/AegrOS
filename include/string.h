#pragma once

#include "types.h"

int memcmp(const void *, const void *, u32);
void *memmove(void *, const void *, u32);
void *memset(void *, int, u32);
char *safestrcpy(char *, const char *, int);
int strlen(const char *);
int strnlen(const char *s, int maxlen);
int strncmp(const char *, const char *, u32);
char *strncpy(char *, const char *, int);
bool starts_with(const char pre[static 1], const char str[static 1]);
char *strcat(char dest[static 1], const char src[static 1]);
void reverse(char s[static 1]);
int itoa(int n, char s[static 1]);
char *strchr(const char *s, int c);
char *strtok(char *str, const char delim[static 1]);
int sscanf(const char *str, const char *format, ...);