#pragma once

#include <stdint.h>


struct vbe_mode_info {
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint16_t pitch;  // number of bytes per horizontal line
    uint16_t width;  // width in pixels
    uint16_t height; // height in pixels
    uint8_t planes;
    uint8_t bpp; // bits per pixel in this mode
    uint8_t memory_model;
    uint8_t image_pages;

    uint8_t red_mask;
    uint8_t red_position;
    uint8_t green_mask;
    uint8_t green_position;
    uint8_t blue_mask;
    uint8_t blue_position;
    uint8_t reserved_mask;
    uint8_t reserved_position;
    uint8_t direct_color_attributes;

    uint32_t framebuffer; // physical address of the linear frame buffer; write here to draw to the screen
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size; // size of memory in the framebuffer but not being displayed on the screen
};

// extern struct vbe_mode_info *vbe_info;

// void vesa_init();
// void vesa_add_clip_rect(rect_t *added_rect);
// void vesa_clear_clip_rects();
void vesa_clear_screen(uint32_t color);
void vesa_putpixel(int x, int y, uint32_t rbg);
void vesa_put_char16(unsigned char c, int x, int y, uint32_t color);
void vesa_put_char8(unsigned char c, int x, int y, uint32_t color, uint32_t bg);
// void vesa_fill_rect(int x, int y, int w, int h, uint32_t color);
// void vesa_draw_rect(int x, int y, unsigned int width, unsigned int height, uint32_t color);
void vesa_puticon32(int x, int y, const unsigned char *icon);
void vesa_put_black_and_white_icon16(int x, int y, const unsigned char *icon);
void vesa_print_string(const char *str, int len, int x, int y, uint32_t color, uint32_t bg);
// void vesa_draw_mouse_cursor(int x, int y);
// void vesa_restore_mouse_cursor();
// void vesa_subtract_clip_rect(rect_t *subtracted_rect);
void vesa_draw_cursor(int x, int y);
void vesa_erase_cursor(int x, int y);
void vesa_scroll_up();
