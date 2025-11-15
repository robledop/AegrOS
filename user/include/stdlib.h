#pragma once

#include <stddef.h>

void exit(void) __attribute__((noreturn));
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *ptr, size_t size);
void free(void *ptr);

int atoi(const char *s);
double atof(const char *s);
int abs(int x);

char *getenv(const char *name);
int putenv(char *string);
int system(const char *command);
char *strdup(const char *s);
