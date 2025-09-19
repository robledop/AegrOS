#include <config.h>
#include <font.h>
#include <memory.h>
#include <stdint.h>
#include <vesa.h>

static struct vbe_mode_info vbe_info_;
struct vbe_mode_info *vbe_info = &vbe_info_;

void vesa_putpixel(int x, int y, uint32_t rgb)
{
    if (x < 0 || x >= vbe_info->width || y < 0 || y >= vbe_info->height) {
        return; // Out of bounds
    }

    auto framebuffer         = (uint8_t *)vbe_info->framebuffer;
    uint32_t bytes_per_pixel = vbe_info->bpp / 8;
    uint8_t *pixel           = framebuffer + (y * vbe_info->pitch) + (x * bytes_per_pixel);

    *((uint32_t *)pixel) = rgb;
}

uint32_t vesa_getpixel(int x, int y)
{
    if (x < 0 || x >= vbe_info->width || y < 0 || y >= vbe_info->height) {
        return 0; // Out of bounds
    }
    auto framebuffer         = (uint8_t *)vbe_info->framebuffer;
    uint32_t bytes_per_pixel = vbe_info->bpp / 8;
    uint8_t *pixel           = framebuffer + (y * vbe_info->pitch) + (x * bytes_per_pixel);
    return *((uint32_t *)pixel);
}


void vesa_puticon32(int x, int y, const unsigned char *icon)
{
    for (int j = 0; j < 32; j++) {
        for (int i = 0; i < 32; i++) {
            uint8_t color_index = icon[j * 32 + i];
            if (color_index != 0xfa) { // Assuming 0xfa is the transparent color
                uint8_t r    = color_index;
                uint8_t g    = color_index;
                uint8_t b    = color_index;
                uint32_t rgb = (r << 16) | (g << 8) | b;
                vesa_putpixel(x + i, y + j, rgb);
            }
        }
    }
}

void vesa_put_bitmap_32(int x, int y, const unsigned int *icon)
{
    for (int j = 0; j < 32; j++) {
        for (int i = 0; i < 32; i++) {
            uint32_t color = icon[j * 32 + i];
            vesa_putpixel(x + i, y + j, color);
        }
    }
}

void vesa_put_black_and_white_icon16(int x, int y, const unsigned char *icon)
{
    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 16; i++) {
            uint8_t pixel = icon[j * 16 + i];
            if (pixel == 1) {
                vesa_putpixel(x + i, y + j, 0xFFFFFF);
            }
        }
    }
}

// TODO: Implement double buffering
void vesa_scroll_up()
{
    uint8_t *framebuffer = (uint8_t *)vbe_info->framebuffer;
    // uint32_t bytes_per_pixel = vbe_info->bpp / 8;
    uint32_t pitch  = vbe_info->pitch;
    uint32_t height = vbe_info->height;

    memmove(framebuffer, framebuffer + pitch * VESA_LINE_HEIGHT, pitch * (height - VESA_LINE_HEIGHT));

    for (uint32_t i = pitch * (height - VESA_LINE_HEIGHT); i < pitch * height; i++) {
        framebuffer[i] = 0;
    }
}

void vesa_clear_screen(uint32_t color)
{
    if (vbe_info->framebuffer == 0) {
        return;
    }

    uint8_t *framebuffer     = (uint8_t *)vbe_info->framebuffer;
    uint32_t bytes_per_pixel = vbe_info->bpp / 8;

    for (uint32_t y = 0; y < vbe_info->height; y++) {
        for (uint32_t x = 0; x < vbe_info->width; x++) {
            uint8_t *pixel = framebuffer + (y * vbe_info->pitch) + (x * bytes_per_pixel);

            *((uint32_t *)pixel) = color;
        }
    }
}

void vesa_put_char8(unsigned char c, int x, int y, uint32_t color, uint32_t bg)
{
    if (c > 128) {
        return;
    }

    // Clear background
    for (int i = 0; i < VESA_CHAR_WIDTH; i++) {
        for (int j = 0; j < VESA_LINE_HEIGHT; j++) {
            vesa_putpixel(x + i, y + j, bg);
        }
    }

    for (int l = 0; l < 8; l++) {
        for (int i = 8; i >= 0; i--) {
            if (font8x8_basic[c][l] & (1 << i)) {
                vesa_putpixel((x) + i, (y) + l, color);
            }
        }
    }
}

void vesa_print_string(const char *str, int len, int x, int y, uint32_t color, uint32_t bg)
{
    for (int i = 0; i < len; i++) {
        vesa_put_char8(str[i], x, y, color, bg);
        x += VESA_CHAR_WIDTH;
    }
}

void vesa_put_char16(unsigned char c, int x, int y, uint32_t color)
{
    for (int l = 0; l < 16; l++) {
        for (int i = 15; i >= 0; i--) {
            if (font8x8_basic[c][l / 2] & (1 << (i / 2))) {
                vesa_putpixel((x) + i, (y) + l, color);
            }
        }
    }
}

void vesa_draw_line(int x1, int y1, int x2, int y2, uint32_t color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int d  = 2 * dy - dx;
    int y  = y1;
    int x  = x1;
    vesa_putpixel(x, y, color);
    while (x < x2) {
        if (d >= 0) {
            y++;
            d -= 2 * dx;
        }
        x++;
        d += 2 * dy;
        vesa_putpixel(x, y, color);
    }

    d = 2 * dx - dy;
    x = x2;
    while (y < y2) {
        if (d >= 0) {
            x--;
            d -= 2 * dy;
        }

        y++;
        d += 2 * dx;
        vesa_putpixel(x, y, color);
    }
}

void vesa_draw_cursor(int x, int y)
{
    // vesa_fill_rect(x, y + VESA_CHAR_WIDTH - 3, VESA_CHAR_WIDTH, 3, 0xFFFFFF);
    return;
}

void vesa_erase_cursor(int x, int y)
{
    // vesa_fill_rect(x, y, VESA_CHAR_WIDTH, VESA_CHAR_WIDTH, DESKTOP_BACKGROUND_COLOR);
    return;
}