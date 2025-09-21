#include <memory.h>
#include <sse.h>
#include <stdint.h>


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

    // Check for overlapping regions - if they overlap, use memmove instead
    if (n > 0 && ((d >= s && d < s + n) || (s >= d && s < d + n))) {
        return memmove(dest, src, n);
    }

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
        // Forward copy (no overlap or safe overlap) - inline implementation
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
        // Fallback for forward copy
        for (size_t i = 0; i < n; i++) {
            d[i] = s[i];
        }
        return dest;
    }

    // Backward copy for overlapping regions where d > s
    size_t remaining           = n;
    unsigned char *d_end       = d + n;
    const unsigned char *s_end = s + n;

#ifdef __SSE2__
    if (n >= 64) {
        // For backward copy, we can't guarantee alignment after arithmetic operations
        // Use unaligned stores for safety
        while (remaining >= 64) {
            remaining -= 64;
            d_end -= 64;
            s_end -= 64;
            vec128 a0 = loadu128(s_end + 48);
            vec128 a1 = loadu128(s_end + 32);
            vec128 a2 = loadu128(s_end + 16);
            vec128 a3 = loadu128(s_end + 0);
            storeu128(d_end + 48, a0);
            storeu128(d_end + 32, a1);
            storeu128(d_end + 16, a2);
            storeu128(d_end + 0, a3);
        }

        while (remaining >= 16) {
            remaining -= 16;
            d_end -= 16;
            s_end -= 16;
            vec128 chunk = loadu128(s_end);
            storeu128(d_end, chunk);
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
