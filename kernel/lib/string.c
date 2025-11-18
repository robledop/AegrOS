#include <stdarg.h>
#include <stdint.h>

#include "defs.h"
#include "printf.h"
#include "types.h"
#include "x86.h"
#include "printf.h"

static int mem_sse_enabled;
static int mem_avx_enabled;

/** @brief Set n bytes of memory to c */
 void *memset(void *dst, int c, size_t n)
{
    if (n == 0) {
        return dst;
    }

    u8 value = (u8)c;

    if (!mem_sse_enabled || n < 16) {
        if ((uintptr_t)dst % 4 == 0 && n % 4 == 0) {
            u32 word = (u32)value;
            word |= word << 8;
            word |= word << 16;
            stosl(dst, (int)word, n / 4);
        } else {
            stosb(dst, value, n);
        }
        return dst;
    }

    u8 *d       = dst;
    size_t left = n;
    u8 pattern[32];
    int pattern_ready   = 0;
    const u32 saved_cr0 = rcr0();
    clts();
    int used_avx = 0;

    if (mem_avx_enabled && left >= 32) {
        if (!pattern_ready) {
            for (int i = 0; i < 32; i++) {
                pattern[i] = value;
            }
            pattern_ready = 1;
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
        if (!pattern_ready) {
            for (int i = 0; i < 16; i++) {
                pattern[i] = value;
            }
            pattern_ready = 1;
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

    lcr0(saved_cr0);

    while (left-- > 0) {
        *d++ = value;
    }

    return dst;
}

/** @brief Compare n bytes of memory */
int memcmp(const void *v1, const void *v2, size_t n)
{
    const u8 *s1 = v1;
    const u8 *s2 = v2;
    if (s1 == s2 || n == 0) {
        return 0;
    }

    if (mem_sse_enabled && n >= 16) {
        const u8 *a         = s1;
        const u8 *b         = s2;
        const u32 saved_cr0 = rcr0();
        clts();

        if (mem_avx_enabled) {
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
                    int diff              = (int)a[idx] - (int)b[idx];
                    lcr0(saved_cr0);
                    return diff;
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
                int diff              = (int)a[idx] - (int)b[idx];
                lcr0(saved_cr0);
                return diff;
            }
            a += 16;
            b += 16;
            n -= 16;
        }

        lcr0(saved_cr0);
        s1 = a;
        s2 = b;
    }

    while (n-- > 0) {
        if (*s1 != *s2) {
            return *s1 - *s2;
        }
        s1++, s2++;
    }

    return 0;
}

/** @brief Move n bytes of memory */
// void *memmove(void *dst, const void *src, size_t n)
// {
//     const char *s = src;
//     char *d = dst;
//     if (s < d && s + n > d)
//     {
//         s += n;
//         d += n;
//         while (n-- > 0)
//         {
//             *--d = *--s;
//         }
//     }
//     else
//     {
//         while (n-- > 0)
//         {
//             *d++ = *s++;
//         }
//     }

//     return dst;
// }

void memory_enable_sse(void)
{
    mem_sse_enabled = 1;
}

void memory_enable_avx(void)
{
    mem_sse_enabled = 1;
    mem_avx_enabled = 1;
}

void memory_disable_avx(void)
{
    mem_avx_enabled = 0;
}

int memory_sse_available(void)
{
    return mem_sse_enabled;
}

int memory_avx_available(void)
{
    return mem_avx_enabled;
}

void *memmove(void *dst, const void *src, size_t n)
{
    if (n == 0 || dst == src) {
        return dst;
    }

    const u8 *s = src;
    u8 *d       = dst;

    if (!mem_sse_enabled) {
        if (s < d && s + n > d) {
            s += n;
            d += n;
            __asm__ volatile("std; rep movsb" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
            __asm__ volatile("cld" :::);
        } else {
            __asm__ volatile("cld; rep movsb" : "+D"(d), "+S"(s), "+c"(n) : : "memory");
        }

        return dst;
    }

    const u32 saved_cr0 = rcr0();
    clts();

    if (mem_avx_enabled && n >= 32) {
        if (s < d && s + n > d) {
            s += n;
            d += n;
            while (n >= 32) {
                s -= 32;
                d -= 32;
                __asm__ volatile("vmovdqu (%0), %%ymm0\n\t"
                    "vmovdqu %%ymm0, (%1)"
                    :
                    : "r"(s), "r"(d)
                    : "memory");
                n -= 32;
            }
        } else {
            while (n >= 32) {
                __asm__ volatile("vmovdqu (%0), %%ymm0\n\t"
                    "vmovdqu %%ymm0, (%1)"
                    :
                    : "r"(s), "r"(d)
                    : "memory");
                d += 32;
                s += 32;
                n -= 32;
            }
        }
    }

    while (n >= 16) {
        __asm__ volatile("movdqu (%0), %%xmm0\n\t"
            "movdqu %%xmm0, (%1)"
            :
            : "r"(s), "r"(d)
            : "memory");
        d += 16;
        s += 16;
        n -= 16;
    }
    while (n-- > 0) {
        *d++ = *s++;
    }

    lcr0(saved_cr0);
    return dst;
}

// memcpy exists to placate GCC.  Use memmove.
/** @brief Copy n bytes of memory (use memmove) */
void *memcpy(void *dst, const void *src, size_t n)
{
    return memmove(dst, src, n);
}

/** @brief Compare n characters of strings */

int strncmp(const char *p, const char *q, size_t n)
{
    while (n > 0 && *p && *p == *q) {
        n--, p++, q++;
    }
    if (n == 0) {
        return 0;
    }
    return (u8)*p - (u8)*q;
}

/** @brief Copy n characters of string */
char *strncpy(char *s, const char *t, size_t n)
{
    char *os = s;
    while (n-- > 0 && (*s++ = *t++) != 0) {
    }
    while (n-- > 0) {
        *s++ = 0;
    }
    return os;
}

// Like strncpy but guaranteed to NUL-terminate.
/** @brief Copy string safely with NUL-termination */
char *safestrcpy(char *s, const char *t, int n)
{
    char *os = s;
    if (n <= 0)
        return os;
    while (--n > 0 && (*s++ = *t++) != 0) {
    }
    *s = 0;
    return os;
}

/** @brief Get length of string */
size_t strlen(const char *s)
{
    size_t n;

    for (n = 0; s[n]; n++) {
    }
    return n;
}

size_t strnlen(const char *s, size_t maxlen)
{
    size_t len = 0;
    while (len < maxlen && s[len] != '\0') {
        len++;
    }
    return len;
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

void reverse(char *s)
{
    int i, j;

    for (i = 0, j = (int)strlen(s) - 1; i < j; i++, j--) {
        const int c = (u8)s[i];
        s[i]        = s[j];
        s[j]        = c;
    }
}

int itoa(int n, char *s)
{
    int sign;

    if ((sign = n) < 0) {
        n = -n;
    }
    int i = 0;
    do {
        s[i++] = n % 10 + '0';
    } while ((n /= 10) > 0);

    if (sign < 0) {
        s[i++] = '-';
    }

    s[i] = '\0';
    reverse(s);

    return i;
}

char *strchr(const char *s, int c)
{
    while (*s != '\0') {
        if (*s == (char)c) {
            return (char *)s;
        }
        s++;
    }
    return nullptr;
}

char *strtok(char *str, const char delim[static 1])
{
    static char *next = nullptr;
    // If str is provided, start from the beginning
    if (str != nullptr) {
        next = str;
    } else {
        // If no more tokens, return nullptr
        if (next == nullptr) {
            return nullptr;
        }
    }

    // Skip leading delimiters
    while (*next != '\0' && strchr(delim, *next) != nullptr) {
        next++;
    }

    // If end of string reached after skipping delimiters
    if (*next == '\0') {
        next = nullptr;
        return nullptr;
    }

    // Mark the start of the token
    char *start = next;

    // Find the end of the token
    while (*next != '\0' && strchr(delim, *next) == nullptr) {
        next++;
    }

    // If end of token is not the end of the string, terminate it
    if (*next != '\0') {
        *next = '\0';
        next++; // Move past the null terminator
    } else {
        // No more tokens
        next = nullptr;
    }

    return start;
}

int sscanf(const char *str, const char *format, ...)
{
    // Simple and limited implementation of sscanf
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

                // Skip whitespace
                while (*s == ' ' || *s == '\t' || *s == '\n') {
                    s++;
                }

                // Handle optional sign
                if (*s == '-') {
                    sign = -1;
                    s++;
                } else if (*s == '+') {
                    s++;
                }

                // Parse integer
                while (*s >= '0' && *s <= '9') {
                    value = value * 10 + (*s - '0');
                    s++;
                }
                *int_ptr = value * sign;
                assigned++;
            }
            if (*f == 's') {
                char *str_ptr = va_arg(args, char *);
                // Skip whitespace
                while (*s == ' ' || *s == '\t' || *s == '\n') {
                    s++;
                }
                // Copy string until next whitespace
                while (*s && *s != ' ' && *s != '\t' && *s != '\n') {
                    *str_ptr++ = *s++;
                }
                *str_ptr = '\0';
                assigned++;
            }
            f++;
        } else {
            if (*f != *s) {
                break; // Mismatch
            }
            f++;
            s++;
        }
    }

    va_end(args);
    return assigned;
}
