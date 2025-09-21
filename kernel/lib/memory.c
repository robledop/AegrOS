#include <memory.h>
#include <stdint.h>

#ifdef __SSE2__
typedef unsigned long long vec128 __attribute__((vector_size(16)));

static inline vec128 loadu128(const void *src)
{
    vec128 v;
    __asm__ volatile("movdqu %1, %0" : "=x"(v) : "m"(*(const unsigned char (*)[16])src));
    return v;
}

static inline void storea128(void *dst, vec128 v)
{
    __asm__ volatile("movdqa %1, %0" : "=m"(*(unsigned char (*)[16])dst) : "x"(v) : "memory");
}

static inline vec128 splat8(unsigned char value)
{
    const unsigned long long chunk = 0x0101010101010101ULL * value;
    const vec128 v                 = {chunk, chunk};
    return v;
}
#endif

void *memset(void *ptr, const int value, const size_t size)
{
    unsigned char *p = ptr;

#ifdef __SSE2__
    if (size >= 64) {
        const vec128 fill = splat8((unsigned char)value);
        size_t remaining  = size;

        while (((uintptr_t)p & 15U) && remaining) {
            *p++ = (unsigned char)value;
            --remaining;
        }

        unsigned char *limit = p + (remaining & ~63ULL);
        while (p < limit) {
            storea128(p + 0, fill);
            storea128(p + 16, fill);
            storea128(p + 32, fill);
            storea128(p + 48, fill);
            p += 64;
        }

        remaining &= 63U;
        while (remaining >= 16) {
            storea128(p, fill);
            p += 16;
            remaining -= 16;
        }

        while (remaining--) {
            *p++ = (unsigned char)value;
        }

        return ptr;
    }
#endif

    for (size_t i = 0; i < size; i++) {
        p[i] = (unsigned char)value;
    }

    return ptr;
}

void *memcpy(void *dest, const void *src, const size_t n)
{
    unsigned char *d       = dest;
    const unsigned char *s = src;

#ifdef __SSE2__
    if (n >= 64) {
        size_t remaining = n;

        while (((uintptr_t)d & 15U) && remaining) {
            *d++ = *s++;
            --remaining;
        }

        unsigned char *limit = d + (remaining & ~63ULL);
        while (d < limit) {
            vec128 a0 = loadu128(s + 0);
            vec128 a1 = loadu128(s + 16);
            vec128 a2 = loadu128(s + 32);
            vec128 a3 = loadu128(s + 48);
            storea128(d + 0, a0);
            storea128(d + 16, a1);
            storea128(d + 32, a2);
            storea128(d + 48, a3);
            d += 64;
            s += 64;
        }

        remaining &= 63U;
        while (remaining >= 16) {
            vec128 chunk = loadu128(s);
            storea128(d, chunk);
            d += 16;
            s += 16;
            remaining -= 16;
        }

        while (remaining--) {
            *d++ = *s++;
        }

        return dest;
    }
#endif

    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memsetw(void *dest, const uint16_t value, size_t n)
{
    uint16_t *ptr = dest;
    while (n--) {
        *ptr++ = value;
    }
    return dest;
}

void *memmove(void *dest, const void *src, const size_t n)
{
    unsigned char *d       = dest;
    const unsigned char *s = src;

    if (d == s || n == 0) {
        return dest;
    }

    if (d < s) {
        return memcpy(dest, src, n);
    }

    size_t remaining           = n;
    unsigned char *d_end       = d + n;
    const unsigned char *s_end = s + n;

#ifdef __SSE2__
    if (n >= 64) {
        while (((uintptr_t)d_end & 15U) && remaining) {
            --d_end;
            --s_end;
            *d_end = *s_end;
            --remaining;
        }

        while (remaining >= 64) {
            remaining -= 64;
            d_end -= 64;
            s_end -= 64;
            vec128 a0 = loadu128(s_end + 48);
            vec128 a1 = loadu128(s_end + 32);
            vec128 a2 = loadu128(s_end + 16);
            vec128 a3 = loadu128(s_end + 0);
            storea128(d_end + 48, a0);
            storea128(d_end + 32, a1);
            storea128(d_end + 16, a2);
            storea128(d_end + 0, a3);
        }

        while (remaining >= 16) {
            remaining -= 16;
            d_end -= 16;
            s_end -= 16;
            vec128 chunk = loadu128(s_end);
            storea128(d_end, chunk);
        }
    }
#endif

    while (remaining) {
        --d_end;
        --s_end;
        *d_end = *s_end;
        --remaining;
    }
    return dest;
}
