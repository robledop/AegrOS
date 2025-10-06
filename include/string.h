#pragma once

#include <stddef.h>
#include <stdint.h>

size_t strlen(const char* s);
size_t strnlen(const char* s, size_t maxlen);
int strnlen_terminator(const char* s, size_t maxlen, char terminator);
int memcmp(const void* v1, const void* v2, unsigned int n);
int strcmp(const char* s1, const char* s2);
int strncmp(const char* p, const char* q, unsigned int n);
char tolower(char s1);
int istrncmp(const char s1[static 1], const char s2[static 1], int n);
char* strncpy(char* dest, const char* src, size_t n);
bool isdigit(char c);
int tonumericdigit(char c);
bool isspace(char c);
char* trim(char str[static 1], size_t max);
int itoa(int n, char s[static 1]);
uint32_t atoi(const char str[static 1]);
int itohex(uint32_t n, char s[static 1]);
char* strchr(const char* s, int c);
char* strtok(char* str, const char delim[static 1]);
char* strcat(char dest[static 1], const char src[static 1]);
char* strncat(char* dest, const char* src, size_t n);
bool starts_with(const char pre[static 1], const char str[static 1]);
bool str_ends_with(const char* str, const char* suffix);
char* strstr(const char* haystack, const char* needle);
char* safestrcpy(char* s, const char* t, int n);
void reverse(char s[static 1]);
