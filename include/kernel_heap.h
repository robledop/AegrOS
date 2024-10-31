#pragma once

#ifndef __KERNEL__
#error "This is a kernel header, and should not be included in userspace"
#endif

#include "types.h"
void kernel_heap_init();
void *kmalloc(size_t size);
void kfree(void *ptr);
void *kzalloc(size_t size);
void *krealloc(void *ptr, const size_t size);