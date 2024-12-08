#pragma once

#ifndef __KERNEL__
#error "This is a kernel header, and should not be included in userspace"
#endif

#include <termcolors.h>


void vga_buffer_init();
void vga_putchar(char c);
