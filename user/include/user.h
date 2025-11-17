#pragma once
#include <types.h>
#include <string.h>
#include <stdlib.h>
#define PRINTF_SUPPRESS_PUTCHAR_DECL 1
#include "printf.h"
#undef PRINTF_SUPPRESS_PUTCHAR_DECL
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
int usleep(unsigned int usec);
int yield(void);
int uptime(void);
int reboot(void);
int shutdown(void);
void panic(const char *);

// ulib.c
int stat(const char *, struct stat *);
char *gets(char *, int max);
void *malloc(size_t size);
void *calloc(size_t nmemb, size_t size);
void *realloc(void *, size_t size);
void free(void *);
char *strdup(const char *);
int atoi(const char *);
int abs(int x);
int getkey();
int getkey_blocking();
int simd_has_sse2(void);
int simd_has_avx(void);

int isspace(int c);
int iscntrl(int c);
int isdigit(int c);
int isprint(int c);
int isatty(int fd);
