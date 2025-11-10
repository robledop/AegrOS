#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"

char *strcpy(char *s, const char *t)
{
    char *os = s;
    while ((*s++ = *t++) != 0) {
    }
    return os;
}

/** @brief Copy n characters of string */
char *strncpy(char *s, const char *t, int n)
{
    char *os = s;
    while (n-- > 0 && (*s++ = *t++) != 0) {
    }
    while (n-- > 0)
        *s++ = 0;
    return os;
}

int strcmp(const char *p, const char *q)
{
    while (*p && *p == *q)
        p++, q++;
    return (u8)*p - (u8)*q;
}


u32 strlen(const char *s)
{
    int n;

    for (n = 0; s[n]; n++) {
    }
    return n;
}


u32 strnlen(const char *s, const u32 maxlen)
{
    u32 n;

    for (n = 0; s[n]; n++) {
        if (n == maxlen) {
            break;
        }
    }
    return n;
}

bool str_ends_with(const char *str, const char *suffix)
{
    if (!str || !suffix) {
        return false;
    }

    const size_t str_len    = strlen(str);
    const size_t suffix_len = strlen(suffix);

    if (suffix_len > str_len) {
        return false;
    }

    return strncmp(str + str_len - suffix_len, suffix, suffix_len) == 0;
}

void *memset(void *dst, int c, u32 n)
{
    stosb(dst, c, n);
    return dst;
}

char *strchr(const char *s, char c)
{
    for (; *s; s++)
        if (*s == c)
            return (char *)s;
    return nullptr;
}

int getkey()
{
    int c = 0;
    read(0, &c, 1); // Read from stdin
    return c;
}

int getkey_blocking()
{
    int key = 0;
    key     = getkey();
    while (key == 0) {
        key = getkey();
    }

    return key;
}

void putchar(char c)
{
    write(0, &c, 1);
}

char *gets(char *buf, int max)
{
    int i;
    char c;

    for (i = 0; i + 1 < max;) {
        int cc = read(0, &c, 1);
        if (cc < 1)
            break;
        buf[i++] = c;
        if (c == '\n' || c == '\r' || c == -30)
            break;
    }
    buf[i] = '\0';
    return buf;
}

int stat(const char *n, struct stat *st)
{
    int fd = open(n, O_RDONLY);
    if (fd < 0)
        return -1;
    int r = fstat(fd, st);
    close(fd);
    return r;
}

int atoi(const char *s)
{
    int n = 0;
    while ('0' <= *s && *s <= '9')
        n = n * 10 + *s++ - '0';
    return n;
}

int abs(int x)
{
    return x < 0 ? -x : x;
}

void *memmove(void *vdst, const void *vsrc, int n)
{
    char *dst       = vdst;
    const char *src = vsrc;
    while (n-- > 0)
        *dst++ = *src++;
    return vdst;
}


int strncmp(const char *p, const char *q, u32 n)
{
    while (n > 0 && *p && *p == *q)
        n--, p++, q++;
    if (n == 0)
        return 0;
    return (u8)*p - (u8)*q;
}

bool starts_with(const char pre[static 1], const char str[static 1])
{
    return strncmp(pre, str, strlen(pre)) == 0;
}

char *strcat(char dest[static 1], const char src[static 1])
{
    char *d = dest;
    while (*d != '\0') {
        d++;
    }
    while (*src != '\0') {
        *d = *src;
        d++;
        src++;
    }
    *d = '\0';
    return dest;
}

char *strncat(char dest[static 1], const char src[static 1], u32 n)
{
    char *d = dest;
    while (*d != '\0') {
        d++;
    }
    u32 i = 0;
    while (i < n && src[i] != '\0') {
        *d = src[i];
        d++;
        i++;
    }
    *d = '\0';
    return dest;
}