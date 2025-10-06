#pragma once

#include <attributes.h>
#include <stddef.h>
#include <stdint.h>

NON_NULL void *memset(void *ptr, int value, size_t size);
NON_NULL void *memcpy(void *dest, const void *src, size_t n);
NON_NULL void *memsetw(void *dest, uint16_t value, size_t n);
NON_NULL void *memmove(void *dest, const void *src, size_t n);
