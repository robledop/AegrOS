#pragma once

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VIDEO_MEMORY P2V(0xB8000)
#define DEFAULT_ATTRIBUTE 0x07 // Light grey on black background
#define BYTES_PER_CHAR 2       // 1 byte for character, 1 byte for attribute (color)
#define SCREEN_SIZE (VGA_WIDTH * VGA_HEIGHT * BYTES_PER_CHAR)
#define ROW_SIZE (VGA_WIDTH * BYTES_PER_CHAR)
#define CRTPORT 0x3d4

// Special keycodes
#define KEY_HOME        0xE0
#define KEY_END         0xE1
#define KEY_UP          0xE2
#define KEY_DN          0xE3
#define KEY_LF          0xE4
#define KEY_RT          0xE5
#define KEY_PGUP        0xE6
#define KEY_PGDN        0xE7
#define KEY_INS         0xE8
#define KEY_DEL         0xE9
#define BACKSPACE 0x100

void vga_terminal_init(void);
void vga_enter_text_mode(void);
void vga_putc(int c);
