#pragma once

#ifndef __KERNEL__
#error "This is a kernel header, and should not be included in userspace"
#endif

#include <termcolors.h>

#define VGA_WIDTH 80
#define VGA_HEIGHT 25

void vga_buffer_init();

#ifndef PIXEL_RENDERING
void putchar(char c);
#endif
