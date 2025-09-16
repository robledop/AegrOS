#include <config.h>
#include <font.h>
#include <memory.h>
#include <stdint.h>
#include <vesa.h>

static struct vbe_mode_info vbe_info_;
struct vbe_mode_info *vbe_info = &vbe_info_;

// struct mouse_cursor {
//     int x;
//     int y;
//     uint32_t pixels[8][8];
//     bool valid;
// };
//
// struct mouse_cursor mouse_cursor_backup = {};


// void vesa_init()
// {
//     clip_rects = list_new();
//     if (!clip_rects) {
//         panic("Failed to allocate memory for clip rects");
//     }
// }

// Insert the passed rectangle into the clip list, splitting all
// existing clip rectangles against it to prevent overlap
// void vesa_add_clip_rect(rect_t *added_rect)
// {
//     vesa_subtract_clip_rect(added_rect);
//
//     // Now that we have made sure none of the existing rectangles overlap
//     // with the new rectangle, we can finally insert it
//     list_add(clip_rects, added_rect);
// }
//
// void vesa_subtract_clip_rect(rect_t *subtracted_rect)
// {
//     // Check each item already in the list to see if it overlaps with
//     // the new rectangle
//
//     for (uint32_t i = 0; i < clip_rects->count;) {
//
//         rect_t *cur_rect = list_get_at(clip_rects, i);
//
//         // Standard rect intersect test (if no intersect, skip to next)
//         // see here for an example of why this works:
//         // http://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other#tab-top
//         if (!(cur_rect->left <= subtracted_rect->right && cur_rect->right >= subtracted_rect->left &&
//               cur_rect->top <= subtracted_rect->bottom && cur_rect->bottom >= subtracted_rect->top)) {
//
//             i++;
//             continue;
//         }
//
//         // If this rectangle does intersect with the new rectangle,
//         // we need to split it
//         list_remove_at(clip_rects, i);                               // Original will be replaced w/splits
//         list_t *split_rects = rect_split(cur_rect, subtracted_rect); // Do the split
//         kfree(cur_rect);                                             // We can throw this away now, we're done with
//         it
//
//         // Copy the split, non-overlapping result rectangles into the list
//         while (split_rects->count) {
//
//             cur_rect = (rect_t *)list_remove_at(split_rects, 0);
//             list_add(clip_rects, cur_rect);
//         }
//
//         // Free the empty split_rect list
//         kfree(split_rects);
//
//         // Since we removed an item from the list, we need to start counting over again
//         // In this way, we'll only exit this loop once nothing in the list overlaps
//         i = 0;
//     }
// }
//
// // Remove all of the clipping rects from the passed context object
// void vesa_clear_clip_rects()
// {
//     // Remove and free until the list is empty
//     while (clip_rects->count) {
//         auto cur_rect = (rect_t *)list_remove_at(clip_rects, 0);
//         kfree(cur_rect);
//     }
// }

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

    // desktop_draw_windows((window_t *)desktop);
}

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

    // desktop_draw_windows((window_t *)desktop);
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

// void vesa_horizontal_line(int x, int y, uint32_t length, uint32_t color)
// {
//     vesa_fill_rect(x, y, length, 1, color);
// }

// void vesa_vertical_line(int x, int y, uint32_t length, uint32_t color)
// {
//     vesa_fill_rect(x, y, 1, length, color);
// }

// void vesa_draw_rect(int x, int y, unsigned int width, unsigned int height, uint32_t color)
// {
//     vesa_horizontal_line(x, y, width, color);                    // top
//     vesa_vertical_line(x, y + 1, height - 2, color);             // left
//     vesa_horizontal_line(x, y + height - 1, width, color);       // bottom
//     vesa_vertical_line(x + width - 1, y + 1, height - 2, color); // right
// }

// This is our new function. It basically acts exactly like the original
// context_fill_rect() except that it limits its drawing to the bounds
// of a rect_t instead of to the bounds of the screen.
// void vesa_clipped_rect(int x, int y, unsigned int width, unsigned int height, rect_t *clip_area, uint32_t color)
// {
//     int max_x = x + width;
//     int max_y = y + height;
//
//     // Make sure we don't go outside of the clip region:
//     if (x < clip_area->left) {
//         x = clip_area->left;
//     }
//
//     if (y < clip_area->top) {
//         y = clip_area->top;
//     }
//
//     if (max_x > clip_area->right + 1) {
//         max_x = clip_area->right + 1;
//     }
//
//     if (max_y > clip_area->bottom + 1) {
//         max_y = clip_area->bottom + 1;
//     }
//
//     // Draw the rectangle into the framebuffer line-by line
//     // just as we've always done
//     for (; y < max_y; y++)
//         for (int cur_x = x; cur_x < max_x; cur_x++)
//             vesa_putpixel(cur_x, y, color);
//     // buffer[y * width + cur_x] = color;
// }

// void vesa_fill_rect(int x, int y, int w, int h, uint32_t color);
// And here is the heavily updated context_fill_rect that calls on the new
// context_clipped_rect() above for each rect_t in the context->clip_rects
// void vesa_fill_rect(int x, int y, int width, int height, uint32_t color)
// {
//     if (clip_rects == nullptr) {
//         return;
//     }
//
//     rect_t screen_area;
//
//     // If there are clipping rects, draw the rect clipped to
//     // each of them. Otherwise, draw unclipped (clipped to the screen)
//     if (clip_rects->count) {
//
//         for (uint32_t i = 0; i < clip_rects->count; i++) {
//
//             rect_t *clip_area = (rect_t *)list_get_at(clip_rects, i);
//             vesa_clipped_rect(x, y, width, height, clip_area, color);
//         }
//     } else {
//
//         // Since we have no rects, pass a fake 'screen' one
//         screen_area.top    = 0;
//         screen_area.left   = 0;
//         screen_area.bottom = vbe_info->height - 1;
//         screen_area.right  = vbe_info->width - 1;
//         vesa_clipped_rect(x, y, width, height, &screen_area, color);
//     }
// }

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

// void vesa_backup_mouse_cursor(int x, int y)
// {
//     mouse_cursor_backup.x = x;
//     mouse_cursor_backup.y = y;
//
//     for (int j = 0; j < 8; j++) {
//         for (int i = 0; i < 8; i++) {
//             uint32_t pixel                   = vesa_getpixel(x + i, y + j);
//             mouse_cursor_backup.pixels[j][i] = pixel;
//         }
//     }
//     mouse_cursor_backup.valid = true;
// }

// void vesa_restore_mouse_cursor()
// {
//     if (!mouse_cursor_backup.valid) {
//         return;
//     }
//     int x = mouse_cursor_backup.x;
//     int y = mouse_cursor_backup.y;
//
//     for (int j = 0; j < 8; j++) {
//         for (int i = 0; i < 8; i++) {
//             uint32_t pixel = mouse_cursor_backup.pixels[j][i];
//             vesa_putpixel(x + i, y + j, pixel);
//         }
//     }
// }
//
// void vesa_draw_mouse_cursor(int x, int y)
// {
//     vesa_restore_mouse_cursor();
//     vesa_backup_mouse_cursor(x, y);
//
//     for (int j = 0; j < 8; j++) {
//         for (int i = 0; i < 8; i++) {
//             uint8_t pixel = mouse_cursor[j][i];
//             if (pixel != 0) {
//                 vesa_putpixel(x + i, y + j, 0xffffff);
//             }
//         }
//     }
// }

// // Draw a single character with the specified font color at the specified coordinates
// void vesa_draw_char_clipped(char character, int x, int y, uint32_t color, rect_t *bound_rect)
// {
//
//     int font_x, font_y;
//     int off_x   = 0;
//     int off_y   = 0;
//     int count_x = 8; // Font is 8x8
//     int count_y = 8;
//     uint8_t shift_line;
//
//     // Make sure to take context translation into account
//     x += context->translate_x;
//     y += context->translate_y;
//
//     // Our font only handles the core set of 128 ASCII chars
//     character &= 0x7F;
//
//     // Check to see if the character is even inside of this rectangle
//     if (x > bound_rect->right || (x + 8) <= bound_rect->left || y > bound_rect->bottom || (y + 12) <=
//     bound_rect->top)
//         return;
//
//     // Limit the drawn portion of the character to the interior of the rect
//     if (x < bound_rect->left)
//         off_x = bound_rect->left - x;
//
//     if ((x + 8) > bound_rect->right)
//         count_x = bound_rect->right - x + 1;
//
//     if (y < bound_rect->top)
//         off_y = bound_rect->top - y;
//
//     if ((y + 12) > bound_rect->bottom)
//         count_y = bound_rect->bottom - y + 1;
//
//     // Now we do the actual pixel plotting loop
//     for (font_y = off_y; font_y < count_y; font_y++) {
//
//         // Capture the current line of the specified char
//         // Just a normal bmp[y * width + x], but in this
//         // case we're dealing with an array of 1bpp
//         // 8-bit-wide character lines
//         shift_line = font_array[font_y * 128 + character];
//
//         // Pre-shift the line by the x-offset
//         shift_line <<= off_x;
//
//         for (font_x = off_x; font_x < count_x; font_x++) {
//
//             // Get the current leftmost bit of the current
//             // line of the character and, if it's set, plot a pixel
//             if (shift_line & 0x80)
//                 context->buffer[(font_y + y) * context->width + (font_x + x)] = color;
//
//             // Shift in the next bit
//             shift_line <<= 1;
//         }
//     }
// }
