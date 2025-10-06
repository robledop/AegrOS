#pragma once

#ifdef __SSE2__
typedef unsigned long long vec128 __attribute__((vector_size(16)));

/**
 * @brief Load unaligned 128-bit value
 * @param src Source pointer (no alignment requirement)
 * @return Value
 */
static inline vec128 loadu128(const void *src)
{
    vec128 v;
    __asm__ volatile("movdqu %1, %0" : "=x"(v) : "m"(*(const unsigned char(*)[16])src));
    return v;
}

/**
 * @brief Store aligned 128-bit value
 * @param dst Destination pointer (must be 16-byte aligned)
 * @param v Value
 */
static inline void storea128(void *dst, vec128 v)
{
    __asm__ volatile("movdqa %1, %0" : "=m"(*(unsigned char(*)[16])dst) : "x"(v) : "memory");
}

/**
 * @brief Store unaligned 128-bit value
 * @param dst Destination pointer
 * @param v Value
 */
static inline void storeu128(void *dst, vec128 v)
{
    __asm__ volatile("movdqu %1, %0" : "=m"(*(unsigned char(*)[16])dst) : "x"(v) : "memory");
}

/**
 * @brief Creates a SIMD vector with all bytes set to the specified value.
 * @param value byte that will be "splatted" across the vector
 * @return
 */
static inline vec128 splat8(unsigned char value)
{
    // The function takes a single parameter, value, of type unsigned char. This is the byte value that will be
    // replicated across the entire 128-bit vector. To achieve this, the function first calculates a 64-bit value,
    // chunk, by multiplying the input value with the constant 0x0101010101010101ULL. This constant ensures that the
    // byte value is repeated across all 8 bytes of the 64-bit integer. For example, if value is 0xFF, the resulting
    // chunk will be 0xFFFFFFFFFFFFFFFF.
    const unsigned long long chunk = 0x0101010101010101ULL * value;
    const vec128 v                 = {chunk, chunk};
    return v;
}

/**
 * @brief Creates a SIMD vector with the specified 32-bit value repeated four times.
 * @param value 32-bit value that will be "splatted" across the vector
 * @return
 */
static inline vec128 splat32(unsigned int value)
{
    const unsigned long long chunk = ((unsigned long long)value << 32) | (unsigned long long)value;
    const vec128 result            = {chunk, chunk};
    return result;
}

#endif
