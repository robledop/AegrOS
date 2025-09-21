#include <config.h>
#include <font.h>
#include <memory.h>
#include <sse.h>
#include <stdint.h>
#include <vesa.h>

static struct vbe_mode_info vbe_info_;
struct vbe_mode_info *vbe_info = &vbe_info_;

static inline int vesa_supports_32bpp(void)
{
    return vbe_info->bpp == 32;
}

static inline void vesa_fill_span32(uint8_t *dst, uint32_t pixel_count, uint32_t color)
{
#ifdef __SSE2__
    if (pixel_count >= 4U) {
        vec128 fill = splat32(color);
        while (((uintptr_t)dst & 15U) && pixel_count) {
            *((uint32_t *)dst) = color;
            dst += 4;
            pixel_count--;
        }
        while (pixel_count >= 4U) {
            storeu128(dst, fill);
            dst += 16;
            pixel_count -= 4U;
        }
    }
#endif
    while (pixel_count--) {
        *((uint32_t *)dst) = color;
        dst += 4;
    }
}

static inline void vesa_copy_span32(uint8_t *dst, const uint32_t *src, uint32_t pixel_count)
{
#ifdef __SSE2__
    if (pixel_count >= 4U) {
        while (((uintptr_t)dst & 15U) && pixel_count) {
            *((uint32_t *)dst) = *src++;
            dst += 4;
            pixel_count--;
        }
        while (pixel_count >= 4U) {
            vec128 value = loadu128(src);
            storeu128(dst, value);
            dst += 16;
            src += 4;
            pixel_count -= 4U;
        }
    }
#endif
    while (pixel_count--) {
        *((uint32_t *)dst) = *src++;
        dst += 4;
    }
}

void vesa_fill_rect32(int x, int y, int width, int height, uint32_t color)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    if (!vesa_supports_32bpp()) {
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                vesa_putpixel(x + i, y + j, color);
            }
        }
        return;
    }

    const int screen_w = (int)vbe_info->width;
    const int screen_h = (int)vbe_info->height;

    int x0 = x;
    int y0 = y;
    int x1 = x + width;
    int y1 = y + height;

    if (x1 <= 0 || y1 <= 0 || x0 >= screen_w || y0 >= screen_h) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > screen_w) {
        x1 = screen_w;
    }
    if (y1 > screen_h) {
        y1 = screen_h;
    }

    uint32_t span_pixels = (uint32_t)(x1 - x0);
    uint8_t *row         = (uint8_t *)vbe_info->framebuffer + (uint32_t)y0 * vbe_info->pitch + (uint32_t)x0 * 4U;

    for (int j = y0; j < y1; j++) {
        vesa_fill_span32(row, span_pixels, color);
        row += vbe_info->pitch;
    }
}

void vesa_blit_span32(int x, int y, const uint32_t *src, uint32_t pixel_count)
{
    if (!src || pixel_count == 0) {
        return;
    }

    if (!vesa_supports_32bpp()) {
        for (uint32_t i = 0; i < pixel_count; i++) {
            vesa_putpixel(x + (int)i, y, src[i]);
        }
        return;
    }

    const int screen_w = (int)vbe_info->width;
    const int screen_h = (int)vbe_info->height;

    if (y < 0 || y >= screen_h) {
        return;
    }

    int x0          = x;
    uint32_t offset = 0;
    if (x0 < 0) {
        offset = (uint32_t)(-x0);
        if (offset >= pixel_count) {
            return;
        }
        pixel_count -= offset;
        x0 = 0;
    }

    if (x0 >= screen_w) {
        return;
    }

    if (x0 + (int)pixel_count > screen_w) {
        pixel_count = (uint32_t)(screen_w - x0);
    }

    uint8_t *row = (uint8_t *)vbe_info->framebuffer + (uint32_t)y * vbe_info->pitch + (uint32_t)x0 * 4U;
    vesa_copy_span32(row, src + offset, pixel_count);
}

void vesa_putpixel(int x, int y, uint32_t rgb)
{
    if (x < 0 || x >= vbe_info->width || y < 0 || y >= vbe_info->height) {
        return; // Out of bounds
    }

    auto framebuffer         = (uint8_t *)vbe_info->framebuffer;
    uint32_t bytes_per_pixel = vbe_info->bpp / 8;
    uint8_t *pixel           = framebuffer + (y * vbe_info->pitch) + (x * bytes_per_pixel);
    *((uint32_t *)pixel)     = rgb;
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
    if (!icon) {
        return;
    }

    const int icon_w = 32;
    const int icon_h = 32;

    if (vesa_supports_32bpp() && x >= 0 && y >= 0 && x + icon_w <= (int)vbe_info->width &&
        y + icon_h <= (int)vbe_info->height) {
        uint8_t *row = (uint8_t *)vbe_info->framebuffer + (uint32_t)y * vbe_info->pitch + (uint32_t)x * 4U;
        for (int j = 0; j < icon_h; j++) {
            const unsigned int *src_row = icon + (uint32_t)j * (uint32_t)icon_w;
            vesa_copy_span32(row, (const uint32_t *)src_row, (uint32_t)icon_w);
            row += vbe_info->pitch;
        }
        return;
    }

    for (int j = 0; j < icon_h; j++) {
        for (int i = 0; i < icon_w; i++) {
            uint32_t color = icon[j * icon_w + i];
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

    vesa_fill_rect32(0, 0, (int)vbe_info->width, (int)vbe_info->height, color);
}

void vesa_put_char8(unsigned char c, int x, int y, uint32_t color, uint32_t bg)
{
    if (c > 128) {
        return;
    }

    vesa_fill_rect32(x, y, VESA_CHAR_WIDTH, VESA_LINE_HEIGHT, bg);

    if (!vesa_supports_32bpp()) {
        for (int l = 0; l < 8; l++) {
            for (int i = 0; i < 8; i++) {
                if (font8x8_basic[c][l] & (1 << i)) {
                    vesa_putpixel(x + i, y + l, color);
                }
            }
        }
        return;
    }

    uint8_t *row = (uint8_t *)vbe_info->framebuffer + (uint32_t)y * vbe_info->pitch + (uint32_t)x * 4U;

    for (int l = 0; l < 8; l++) {
        uint8_t mask    = font8x8_basic[c][l];
        uint32_t *dst32 = (uint32_t *)row;
        for (int i = 0; i < 8; i++) {
            if (mask & (1U << i)) {
                dst32[i] = color;
            }
        }
        row += vbe_info->pitch;
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
