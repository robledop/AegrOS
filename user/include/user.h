#pragma once
#include <types.h>
#include "printf.h"
#include "mman.h"
#include <termios.h>
#include <sys/ioctl.h>
struct stat;
struct rtcdate;
typedef void (*atexit_function)(void);

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// system calls
int fork(void);
void exit(void) __attribute__ ((noreturn));
#define wait wait_
int wait_(void);
int pipe(int *);
int write(int, const void *, int);
int read(int, void *, int);
int close(int);
int kill(int);
int exec(char *, char **);
int open(const char *, int);
int mknod(const char *, short, short);
int unlink(const char *);
int fstat(int fd, struct stat *);
int lseek(int, int, int);
int link(const char *, const char *);
int mkdir(const char *);
int chdir(const char *);
int getcwd(char *, int);
int dup(int);
int getpid(void);
int atexit(atexit_function func);
void atexit_init(void);
int tcgetattr(int fd, struct termios *t);
int tcsetattr(int fd, int action, const struct termios *t);
int ioctl(int fd, int request, void *argp);
char *sbrk(int);
void *mmap(void *addr, u32 length, int prot, int flags, int fd, u32 offset);
int munmap(void *addr, u32 length);
int sleep(int);
int yield(void);
int uptime(void);
int reboot(void);
int shutdown(void);
void panic(const char *);

// ulib.c
int stat(const char *, struct stat *);
char *strcpy(char *, const char *);
char *strncpy(char *, const char *, int);
void *memmove(void *, const void *, int);
void *memcpy(void *, const void *, u32);
int memcmp(const void *, const void *, u32);
char *strchr(const char *, char c);
char *strstr(const char *, const char *);
int strcmp(const char *, const char *);
// void printf(int, const char *, ...);
char *gets(char *, int max);
u32 strlen(const char *);
void *memset(void *, int, u32);
void *malloc(u32);
void *realloc(void *, u32);
void free(void *);
char *strdup(const char *);
int atoi(const char *);
int abs(int x);
int strncmp(const char *p, const char *q, u32 n);
bool starts_with(const char pre[static 1], const char str[static 1]);
char *strcat(char dest[static 1], const char src[static 1]);
char *strncat(char dest[static 1], const char src[static 1], u32 n);
u32 strnlen(const char *s, u32 maxlen);
int sscanf(const char *str, const char *format, ...);
int getkey();
int getkey_blocking();
// void putchar(char c);
bool str_ends_with(const char *str, const char *suffix);
int isspace(int c);
int iscntrl(int c);
int isdigit(int c);
int isprint(int c);
int isatty(int fd);
