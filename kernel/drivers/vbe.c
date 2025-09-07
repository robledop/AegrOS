#include <memory.h>
#include <printf.h>
#include <termcolors.h>
#include <vbe.h>

static struct vbe_mode_info_structure __vbe_info;
struct vbe_mode_info_structure *vbe_info = &__vbe_info;

void clear_screen(uint32_t color)
{
    if (vbe_info->framebuffer == 0) {
        return;
    }

    uint8_t *framebuffer     = (uint8_t *)vbe_info->framebuffer;
    uint32_t bytes_per_pixel = vbe_info->bpp / 8;

    for (uint32_t y = 0; y < vbe_info->height; y++) {
        for (uint32_t x = 0; x < vbe_info->width; x++) {
            uint8_t *pixel = framebuffer + (y * vbe_info->pitch) + (x * bytes_per_pixel);

            if (vbe_info->bpp == 32) {
                *((uint32_t *)pixel) = color;
            } else if (vbe_info->bpp == 24) {
                pixel[0] = color & 0xFF;         // Blue
                pixel[1] = (color >> 8) & 0xFF;  // Green
                pixel[2] = (color >> 16) & 0xFF; // Red
            } else if (vbe_info->bpp == 16) {
                // Convert 32-bit color to 16-bit (RGB565)
                uint16_t r           = ((color >> 16) & 0xFF) >> 3;
                uint16_t g           = ((color >> 8) & 0xFF) >> 2;
                uint16_t b           = (color & 0xFF) >> 3;
                *((uint16_t *)pixel) = (r << 11) | (g << 5) | b;
            }
        }
    }
}

void putpixel_rgb(int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
    if (x < 0 || x >= vbe_info->width || y < 0 || y >= vbe_info->height) {
        return; // Out of bounds
    }

    uint8_t *framebuffer     = (uint8_t *)vbe_info->framebuffer;
    uint32_t bytes_per_pixel = vbe_info->bpp / 8;
    uint8_t *pixel           = framebuffer + (y * vbe_info->pitch) + (x * bytes_per_pixel);

    if (vbe_info->bpp == 32) {
        *((uint32_t *)pixel) = (r << 16) | (g << 8) | b;
    } else if (vbe_info->bpp == 24) {
        pixel[0] = b; // Blue
        pixel[1] = g; // Green
        pixel[2] = r; // Red
    } else if (vbe_info->bpp == 16) {
        // Convert to RGB565
        uint16_t color       = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
        *((uint16_t *)pixel) = color;
    }
}

inline void putpixel(uint8_t *buffer, int x, int y, unsigned char color, int pitch)
{
    uint8_t *pixel_offset = (uint8_t *)(y * pitch + (x * (vbe_info->bpp / 8)) + buffer);
    *pixel_offset         = color;
}


void simple_test_screen(void)
{
    if (vbe_info->framebuffer == 0)
        return;

    // Fill with red for first quarter, blue for rest
    uint8_t *fb          = (uint8_t *)vbe_info->framebuffer;
    uint32_t total_bytes = vbe_info->height * vbe_info->pitch;

    // Fill first quarter with red pattern
    for (uint32_t i = 0; i < total_bytes / 4; i += 4) {
        fb[i]     = 0x00; // Blue
        fb[i + 1] = 0x00; // Green
        fb[i + 2] = 0xFF; // Red
        fb[i + 3] = 0x00; // Alpha/padding
    }

    // Fill rest with blue
    for (uint32_t i = total_bytes / 4; i < total_bytes; i += 4) {
        fb[i]     = 0xFF; // Blue
        fb[i + 1] = 0x00; // Green
        fb[i + 2] = 0x00; // Red
        fb[i + 3] = 0x00; // Alpha/padding
    }
}
