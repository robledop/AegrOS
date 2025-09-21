#pragma once
#ifdef __SSE2__
typedef unsigned long long vec128 __attribute__((vector_size(16)));

static inline vec128 loadu128(const void *src)
{
    vec128 v;
    __asm__ volatile("movdqu %1, %0" : "=x"(v) : "m"(*(const unsigned char(*)[16])src));
    return v;
}

static inline void storea128(void *dst, vec128 v)
{
    __asm__ volatile("movdqa %1, %0" : "=m"(*(unsigned char(*)[16])dst) : "x"(v) : "memory");
}

static inline void storeu128(void *dst, vec128 v)
{
    __asm__ volatile("movdqu %1, %0" : "=m"(*(unsigned char(*)[16])dst) : "x"(v) : "memory");
}

static inline vec128 splat8(unsigned char value)
{
    const unsigned long long chunk = 0x0101010101010101ULL * value;
    const vec128 v                 = {chunk, chunk};
    return v;
}

static inline vec128 splat32(unsigned int value)
{
    const unsigned long long chunk = ((unsigned long long)value << 32) | (unsigned long long)value;
    const vec128 result            = {chunk, chunk};
    return result;
}


#endif
