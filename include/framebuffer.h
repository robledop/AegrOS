#pragma once

#include "types.h"
#include <stdbool.h>
#include "multiboot.h"

#define VESA_CHAR_WIDTH 8
#define VESA_CHAR_HEIGHT 12
#define VESA_LINE_HEIGHT 14

#define FRAMEBUFFER 2

struct vbe_mode_info
{
    u16 window_size;
    u16 segment_a;
    u16 segment_b;
    u16 pitch; // number of bytes per horizontal line
    u16 width; // width in pixels
    u16 height; // height in pixels
    u8 planes;
    u8 bpp; // bits per pixel in this mode
    u8 memory_model;
    u8 image_pages;

    u8 red_mask;
    u8 red_position;
    u8 green_mask;
    u8 green_position;
    u8 blue_mask;
    u8 blue_position;
    u8 reserved_mask;
    u8 reserved_position;
    u8 direct_color_attributes;

    u32 framebuffer; // physical address of the linear frame buffer; write here to draw to the screen
    u32 framebuffer_virtual; // kernel virtual address mapped to the framebuffer (0 if unmapped)
    u32 off_screen_mem_off;
    u16 off_screen_mem_size; // size of memory in the framebuffer but not being displayed on the screen
};

extern struct vbe_mode_info* vbe_info;

struct cpu;
void framebuffer_set_vbe_info(const multiboot_info_t *mbd);
bool framebuffer_map_boot_framebuffer(struct cpu *bsp);
void framebuffer_prepare_cpu(struct cpu *cpu);

static inline u8 *framebuffer_kernel_bytes(void)
{
    if (vbe_info == nullptr) {
        return nullptr;
    }
    u32 addr = vbe_info->framebuffer_virtual;
    if (addr == 0) {
        return nullptr;
    }
    return (u8 *)(uptr)addr;
}

void framebuffer_init();
void framebuffer_clear_screen(u32 color);
void framebuffer_putpixel(int x, int y, u32 rbg);
void framebuffer_put_char16(unsigned char c, int x, int y, u32 color);
void framebuffer_put_char8(unsigned char c, int x, int y, u32 color, u32 bg);
void framebuffer_fill_rect32(int x, int y, int width, int height, u32 color);
void framebuffer_blit_span32(int x, int y, const u32* src, u32 pixel_count);
void framebuffer_puticon32(int x, int y, const unsigned char* icon);
void framebuffer_put_bitmap_32(int x, int y, const unsigned int* icon);
void framebuffer_put_black_and_white_icon16(int x, int y, const unsigned char* icon);
void framebuffer_print_string(const char* str, int len, int x, int y, u32 color, u32 bg);
void framebuffer_draw_cursor(int x, int y);
void framebuffer_erase_cursor(int x, int y);
void framebuffer_scroll_up();
void framebuffer_enable_write_combining(void);
bool framebuffer_write_combining_enabled(void);
