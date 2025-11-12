#include <stdarg.h>

#include "types.h"
#include "stat.h"
#include "fcntl.h"
#include "user.h"
#include "x86.h"
#include "errno.h"
#include "status.h"

extern void sys_exit(void);

static atexit_function atexit_slots[32];

void atexit_init(void)
{
    for (int i = 0; i < (int)(sizeof(atexit_slots) / sizeof(atexit_slots[0])); i++) {
        atexit_slots[i] = nullptr;
    }
}

int atexit(atexit_function func)
{
    if (func == nullptr) {
        errno = -EINVARG;
        return -1;
    }
    for (int i = 0; i < (int)(sizeof(atexit_slots) / sizeof(atexit_slots[0])); i++) {
        if (atexit_slots[i] == nullptr) {
            atexit_slots[i] = func;
            return 0;
        }
    }
    errno = -ENOMEM;
    return -1;
}

static void run_atexit_handlers(void)
{
    for (int i = (int)(sizeof(atexit_slots) / sizeof(atexit_slots[0])) - 1; i >= 0; i--) {
        if (atexit_slots[i] != nullptr) {
            atexit_function fn = atexit_slots[i];
            atexit_slots[i]    = nullptr;
            fn();
        }
    }
}

void exit(void)
{
    run_atexit_handlers();
    sys_exit();
    __builtin_unreachable();
}

void panic(const char *s)
{
    printf("panic: %s\n", s);
    exit();
}

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

int isspace(int c)
{
    switch (c) {
    case ' ':
    case '\t':
    case '\n':
    case '\r':
    case '\v':
    case '\f':
        return 1;
    default:
        return 0;
    }
}

int iscntrl(int c)
{
    if (c < 0) {
        return 0;
    }
    return (u32)c < 0x20 || c == 0x7f;
}

int isdigit(int c)
{
    return c >= '0' && c <= '9';
}

int isprint(int c)
{
    return c >= 0x20 && c <= 0x7e;
}


u32 strlen(const char *s)
{
    int n;

    for (n = 0; s[n]; n++) {
    }
    return n;
}

char *strdup(const char *s)
{
    if (s == nullptr) {
        return nullptr;
    }
    u32 len = strlen(s);
    char *p = malloc(len + 1);
    if (p == nullptr) {
        return nullptr;
    }
    memmove(p, s, len + 1);
    return p;
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

void *memcpy(void *dst, const void *src, u32 n)
{
    return memmove(dst, src, (int)n);
}

int memcmp(const void *v1, const void *v2, u32 n)
{
    const u8 *s1 = v1;
    const u8 *s2 = v2;
    while (n-- > 0) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        s1++;
        s2++;
    }
    return 0;
}

int isatty(int fd)
{
    if (fd >= 0 && fd <= 2) {
        return 1;
    }
    errno = -ENOTTY;
    return 0;
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

int sscanf(const char *str, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    const char *s = str;
    const char *f = format;
    int assigned  = 0;

    while (*f && *s) {
        if (*f == '%') {
            f++;
            if (*f == 'd') {
                int *int_ptr = va_arg(args, int *);
                int value    = 0;
                int sign     = 1;

                while (isspace(*s)) {
                    s++;
                }
                if (*s == '-') {
                    sign = -1;
                    s++;
                } else if (*s == '+') {
                    s++;
                }
                while (*s >= '0' && *s <= '9') {
                    value = value * 10 + (*s - '0');
                    s++;
                }
                *int_ptr = value * sign;
                assigned++;
            } else if (*f == 's') {
                char *str_ptr = va_arg(args, char *);
                while (isspace(*s)) {
                    s++;
                }
                while (*s && !isspace(*s)) {
                    *str_ptr++ = *s++;
                }
                *str_ptr = '\0';
                assigned++;
            }
            f++;
        } else {
            if (*f != *s) {
                break;
            }
            f++;
            s++;
        }
    }

    va_end(args);
    return assigned;
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

char *strstr(const char *haystack, const char *needle)
{
    if (!haystack || !needle) {
        return nullptr;
    }
    if (*needle == '\0') {
        return (char *)haystack;
    }
    for (; *haystack; haystack++) {
        if (*haystack != *needle) {
            continue;
        }
        const char *h = haystack + 1;
        const char *n = needle + 1;
        while (*n && *h == *n) {
            h++;
            n++;
        }
        if (*n == '\0') {
            return (char *)haystack;
        }
    }
    return nullptr;
}