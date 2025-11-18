#include <stdarg.h>
#include <stdint.h>

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

void __ae_exit(int status)
{
    run_atexit_handlers();
    sys_exit();
    __builtin_unreachable();
}

void panic(const char *s)
{
    printf("panic: %s\n", s);
    __ae_exit(1);
}

char *strcpy(char *s, const char *t)
{
    char *os = s;
    while ((*s++ = *t++) != 0) {
    }
    return os;
}

char *strncpy(char *dst, const char *src, size_t n)
{
    size_t i = 0;

    for (; i < n && src[i] != '\0'; i++) {
        dst[i] = src[i];
    }

    for (; i < n; i++) {
        dst[i] = '\0';
    }

    return dst;
}

int strcmp(const char *p, const char *q)
{
    while (*p && *p == *q) {
        p++;
        q++;
    }
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

int isalpha(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int isalnum(int c)
{
    return isalpha(c) || isdigit(c);
}

int islower(int c)
{
    return c >= 'a' && c <= 'z';
}

int isupper(int c)
{
    return c >= 'A' && c <= 'Z';
}

int isxdigit(int c)
{
    return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

int isblank(int c)
{
    return c == ' ' || c == '\t';
}

int isgraph(int c)
{
    return c > 0x20 && c <= 0x7e;
}

int ispunct(int c)
{
    return isprint(c) && !isalnum(c) && c != ' ';
}

int tolower(int c)
{
    if (isupper(c)) {
        return c - 'A' + 'a';
    }
    return c;
}

int toupper(int c)
{
    if (islower(c)) {
        return c - 'a' + 'A';
    }
    return c;
}


size_t strlen(const char *s)
{
    size_t n;

    for (n = 0; s[n]; n++) {
    }
    return n;
}

char *strdup(const char *s)
{
    if (s == nullptr) {
        return nullptr;
    }
    size_t len = strlen(s);
    char *p    = malloc(len + 1);
    if (p == nullptr) {
        return nullptr;
    }
    memmove(p, s, len + 1);
    return p;
}


size_t strnlen(const char *s, size_t maxlen)
{
    size_t n = 0;
    while (n < maxlen && s[n] != '\0') {
        n++;
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

void *memset(void *dst, int c, size_t n)
{
    if (n == 0) {
        return dst;
    }

    u8 value     = (u8)c;
    u8 *d        = dst;
    size_t left  = n;
    int has_sse2 = simd_has_sse2();

    if (!has_sse2 || n < 16) {
        stosb(dst, value, n);
        return dst;
    }

    int has_avx      = simd_has_avx();
    u8 pattern[32];
    int pattern_init = 0;
    int used_avx     = 0;

    if (has_avx && left >= 32) {
        if (!pattern_init) {
            for (int i = 0; i < 32; i++) {
                pattern[i] = value;
            }
            pattern_init = 1;
        }
        __asm__ volatile("vmovdqu (%0), %%ymm0" : : "r"(pattern));
        while (left >= 32) {
            __asm__ volatile("vmovdqu %%ymm0, (%0)" : : "r"(d) : "memory");
            d += 32;
            left -= 32;
        }
        used_avx = 1;
    }

    if (left >= 16) {
        if (!pattern_init) {
            for (int i = 0; i < 16; i++) {
                pattern[i] = value;
            }
            pattern_init = 1;
        }
        __asm__ volatile("movdqu (%0), %%xmm0" : : "r"(pattern));
        while (left >= 16) {
            __asm__ volatile("movdqu %%xmm0, (%0)" : : "r"(d) : "memory");
            d += 16;
            left -= 16;
        }
    }

    if (used_avx) {
        __asm__ volatile("vzeroupper" ::: "memory");
    }

    while (left-- > 0) {
        *d++ = value;
    }

    return dst;
}

char *strchr(const char *s, int c)
{
    while (*s) {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    if (c == '\0') {
        return (char *)s;
    }
    return nullptr;
}

char *strrchr(const char *s, int c)
{
    const char *last = nullptr;
    char ch          = (char)c;
    while (*s != '\0') {
        if (*s == ch) {
            last = s;
        }
        s++;
    }
    if (ch == '\0') {
        return (char *)s;
    }
    return (char *)last;
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

int putchar(int c)
{
    unsigned char ch = (unsigned char)c;
    if (write(STDOUT_FILENO, &ch, 1) != 1) {
        errno = -EIO;
        return -1;
    }
    return (int)ch;
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

struct memmove_simd_caps
{
    int initialized;
    int has_sse2;
    int has_avx;
};

static struct memmove_simd_caps memmove_caps;

static void memmove_detect_simd_caps(void)
{
    if (memmove_caps.initialized) {
        return;
    }

    u32 eax, ebx, ecx, edx;
    cpuid(0x01, &eax, &ebx, &ecx, &edx);

    memmove_caps.has_sse2 = (edx & CPUID_FEAT_EDX_SSE2) != 0;

    const bool cpu_has_xsave = (ecx & CPUID_FEAT_ECX_XSAVE) != 0;
    const bool os_uses_xsave = (ecx & CPUID_FEAT_ECX_OSXSAVE) != 0;
    const bool cpu_has_avx   = (ecx & CPUID_FEAT_ECX_AVX) != 0;

    if (cpu_has_avx && cpu_has_xsave && os_uses_xsave) {
        const u64 xcr0 = xgetbv(0);
        if ((xcr0 & (XCR0_SSE | XCR0_AVX)) == (XCR0_SSE | XCR0_AVX)) {
            memmove_caps.has_avx  = 1;
            memmove_caps.has_sse2 = 1;
        }
    }

    memmove_caps.initialized = 1;
}

int simd_has_sse2(void)
{
    memmove_detect_simd_caps();
    return memmove_caps.has_sse2;
}

int simd_has_avx(void)
{
    memmove_detect_simd_caps();
    return memmove_caps.has_avx;
}

void *memmove(void *vdst, const void *vsrc, size_t n)
{
    memmove_detect_simd_caps();

    const char *src = vsrc;
    char *dst       = vdst;

    if (n == 0 || src == dst) {
        return vdst;
    }

    const int use_avx  = memmove_caps.has_avx;
    const int use_sse2 = memmove_caps.has_sse2;
    int used_avx       = 0;

    if (src < dst && src + n > dst) {
        src += n;
        dst += n;

        if (use_avx) {
            while (n >= 32) {
                src -= 32;
                dst -= 32;
                __asm__ volatile("vmovdqu (%[s]), %%ymm0\n\t"
                                 "vmovdqu %%ymm0, (%[d])"
                                 :
                                 : [s] "r"(src), [d] "r"(dst)
                                 : "memory");
                n -= 32;
                used_avx = 1;
            }
        }

        if (use_sse2) {
            while (n >= 16) {
                src -= 16;
                dst -= 16;
                __asm__ volatile("movdqu (%[s]), %%xmm0\n\t"
                                 "movdqu %%xmm0, (%[d])"
                                 :
                                 : [s] "r"(src), [d] "r"(dst)
                                 : "memory");
                n -= 16;
            }
        }

        while (n-- > 0) {
            *--dst = *--src;
        }
    } else {
        if (use_avx) {
            while (n >= 32) {
                __asm__ volatile("vmovdqu (%[s]), %%ymm0\n\t"
                                 "vmovdqu %%ymm0, (%[d])"
                                 :
                                 : [s] "r"(src), [d] "r"(dst)
                                 : "memory");
                src += 32;
                dst += 32;
                n -= 32;
                used_avx = 1;
            }
        }

        if (use_sse2) {
            while (n >= 16) {
                __asm__ volatile("movdqu (%[s]), %%xmm0\n\t"
                                 "movdqu %%xmm0, (%[d])"
                                 :
                                 : [s] "r"(src), [d] "r"(dst)
                                 : "memory");
                src += 16;
                dst += 16;
                n -= 16;
            }
        }

        while (n-- > 0) {
            *dst++ = *src++;
        }
    }

    if (used_avx) {
        __asm__ volatile("vzeroupper" ::: "memory");
    }

    return vdst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    return memmove(dst, src, n);
}

int memcmp(const void *v1, const void *v2, size_t n)
{
    const unsigned char *s1 = v1;
    const unsigned char *s2 = v2;
    if (s1 == s2 || n == 0) {
        return 0;
    }

    const int has_sse2 = simd_has_sse2();
    if (has_sse2 && n >= 16) {
        const int has_avx = simd_has_avx();
        const unsigned char *a = s1;
        const unsigned char *b = s2;

        if (has_avx) {
            while (n >= 32) {
                unsigned int mask;
                __asm__ volatile("vmovdqu (%[a]), %%ymm0\n\t"
                                 "vmovdqu (%[b]), %%ymm1\n\t"
                                 "vpcmpeqb %%ymm1, %%ymm0, %%ymm0\n\t"
                                 "vpmovmskb %%ymm0, %[mask]"
                                 : [mask] "=r"(mask)
                                 : [a] "r"(a), [b] "r"(b)
                                 : "memory");
                if (mask != 0xFFFFFFFFu) {
                    unsigned int mismatch = ~mask;
                    unsigned int idx      = __builtin_ctz(mismatch);
                    return (int)a[idx] - (int)b[idx];
                }
                a += 32;
                b += 32;
                n -= 32;
            }
        }

        while (n >= 16) {
            unsigned int mask;
            __asm__ volatile("movdqu (%[a]), %%xmm0\n\t"
                             "movdqu (%[b]), %%xmm1\n\t"
                             "pcmpeqb %%xmm1, %%xmm0\n\t"
                             "pmovmskb %%xmm0, %[mask]"
                             : [mask] "=r"(mask)
                             : [a] "r"(a), [b] "r"(b)
                             : "memory");
            mask &= 0xFFFFu;
            if (mask != 0xFFFFu) {
                unsigned int mismatch = (~mask) & 0xFFFFu;
                unsigned int idx      = __builtin_ctz(mismatch);
                return (int)a[idx] - (int)b[idx];
            }
            a += 16;
            b += 16;
            n -= 16;
        }

        s1 = a;
        s2 = b;
    }

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


int strncmp(const char *p, const char *q, size_t n)
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
    char *d       = dest;
    const char *s = src;
    while (*d != '\0') {
        d++;
    }
    while (*s != '\0') {
        *d++ = *s++;
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

char *strncat(char dest[static 1], const char src[static 1], size_t n)
{
    char *d       = dest;
    const char *s = src;
    size_t copied = 0;
    while (*d != '\0') {
        d++;
    }
    while (copied < n && *s != '\0') {
        *d++ = *s++;
        copied++;
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
