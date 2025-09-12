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

extern struct vbe_mode_info *vbe_info;


void clear_screen(uint32_t color);
void putpixel_rgb(int x, int y, uint8_t r, uint8_t g, uint8_t b);
void vesa_put_char16(unsigned char c, int x, int y, uint8_t r, uint8_t g, uint8_t b);
void vesa_put_char8(unsigned char c, int x, int y, uint8_t r, uint8_t g, uint8_t b);
void vesa_fillrect(int x, int y, int w, int h, uint8_t r, uint8_t g, uint8_t b);
void vesa_draw_window(int x, int y, int w, int h);
void vesa_puticon32(int x, int y, const unsigned char *icon);

#ifdef PIXEL_RENDERING
void putchar(char c);
#endif


void text_mode_hello_world(void);
