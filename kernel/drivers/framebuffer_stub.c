#ifndef GRAPHICS
#include "framebuffer.h"

struct vbe_mode_info *vbe_info = nullptr;

void framebuffer_set_vbe_info(const multiboot_info_t *mbd)
{
    (void)mbd;
}

bool framebuffer_map_boot_framebuffer(struct cpu *bsp)
{
    (void)bsp;
    return false;
}

void framebuffer_prepare_cpu(struct cpu *cpu)
{
    (void)cpu;
}

void framebuffer_init(void)
{
}

void framebuffer_clear_screen(u32 color)
{
    (void)color;
}

void framebuffer_putpixel(int x, int y, u32 rbg)
{
    (void)x;
    (void)y;
    (void)rbg;
}

void framebuffer_put_char16(unsigned char c, int x, int y, u32 color)
{
    (void)c;
    (void)x;
    (void)y;
    (void)color;
}

void framebuffer_put_char8(unsigned char c, int x, int y, u32 color, u32 bg)
{
    (void)c;
    (void)x;
    (void)y;
    (void)color;
    (void)bg;
}

void framebuffer_fill_rect32(int x, int y, int width, int height, u32 color)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    (void)color;
}

void framebuffer_blit_span32(int x, int y, const u32 *src, u32 pixel_count)
{
    (void)x;
    (void)y;
    (void)src;
    (void)pixel_count;
}

void framebuffer_puticon32(int x, int y, const unsigned char *icon)
{
    (void)x;
    (void)y;
    (void)icon;
}

void framebuffer_put_bitmap_32(int x, int y, const unsigned int *icon)
{
    (void)x;
    (void)y;
    (void)icon;
}

void framebuffer_put_black_and_white_icon16(int x, int y, const unsigned char *icon)
{
    (void)x;
    (void)y;
    (void)icon;
}

void framebuffer_print_string(const char *str, int len, int x, int y, u32 color, u32 bg)
{
    (void)str;
    (void)len;
    (void)x;
    (void)y;
    (void)color;
    (void)bg;
}

void framebuffer_draw_cursor(int x, int y)
{
    (void)x;
    (void)y;
}

void framebuffer_erase_cursor(int x, int y)
{
    (void)x;
    (void)y;
}

void framebuffer_scroll_up(void)
{
}

void framebuffer_enable_write_combining(void)
{
}

bool framebuffer_write_combining_enabled(void)
{
    return false;
}

#endif /* GRAPHICS */
