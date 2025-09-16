#pragma once

#include <attributes.h>
#ifndef __KERNEL__
#error "This is a kernel header, and should not be included in userspace"
#endif

#include <stddef.h>

void kernel_heap_init(int heap_size);
void *kmalloc(size_t size);
NON_NULL void kfree(void *ptr);
void *kzalloc(size_t size);
NON_NULL void *krealloc(void *ptr, size_t size);
void kernel_heap_print_stats();
