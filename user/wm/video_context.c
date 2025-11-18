#include <font.h>
#include <wm/rect.h>
#include <wm/video_context.h>
#include <user.h>
#include <types.h>
#include <wm/desktop.h>

static u32 *framebuffer;

static int framebuffer_simd_initialized;
static int framebuffer_has_sse2;
static int framebuffer_has_avx;

static inline u8 reverse_bits8(u8 v)
{
    v = (u8)(((v & 0xF0u) >> 4) | ((v & 0x0Fu) << 4));
    v = (u8)(((v & 0xCCu) >> 2) | ((v & 0x33u) << 2));
    v = (u8)(((v & 0xAAu) >> 1) | ((v & 0x55u) << 1));
    return v;
}

static inline void framebuffer_init_simd_caps(void)
{
    if (framebuffer_simd_initialized) {
        return;
    }
    framebuffer_has_sse2       = simd_has_sse2();
    framebuffer_has_avx        = simd_has_avx();
    framebuffer_simd_initialized = 1;
}

static inline void framebuffer_fill_span32(u8 *dst, u32 pixel_count, u32 color)
{
    if (pixel_count == 0) {
        return;
    }

    framebuffer_init_simd_caps();

    u32 remaining = pixel_count;

    u32 pattern[8];
    int pattern_initialized = 0;

    int used_avx = 0;
    if (framebuffer_has_avx && remaining >= 8) {
        if (!pattern_initialized) {
            for (int i = 0; i < 8; i++) {
                pattern[i] = color;
            }
            pattern_initialized = 1;
        }

        __asm__ volatile("vmovdqu (%0), %%ymm0" : : "r"(pattern));
        while (remaining >= 8) {
            __asm__ volatile("vmovdqu %%ymm0, (%0)" : : "r"(dst) : "memory");
            dst += 32;
            remaining -= 8;
        }
        used_avx = 1;
    }

    if (framebuffer_has_sse2 && remaining >= 4) {
        if (!pattern_initialized) {
            for (int i = 0; i < 8; i++) {
                pattern[i] = color;
            }
            pattern_initialized = 1;
        }

        __asm__ volatile("movdqu (%0), %%xmm0" : : "r"(pattern));
        while (remaining >= 4) {
            __asm__ volatile("movdqu %%xmm0, (%0)" : : "r"(dst) : "memory");
            dst += 16;
            remaining -= 4;
        }
    }

    if (used_avx) {
        __asm__ volatile("vzeroupper" ::: "memory");
    }

    while (remaining--) {
        *((u32 *)dst) = color;
        dst += 4;
    }
}

static inline void framebuffer_copy_span32(u8 *dst, const u32 *src, u32 pixel_count)
{
    if (pixel_count == 0) {
        return;
    }

    framebuffer_init_simd_caps();

    u32 remaining       = pixel_count;
    const u8 *src_bytes = (const u8 *)src;
    u8 *dst_bytes       = dst;
    int used_avx        = 0;

    if (framebuffer_has_avx && remaining >= 8) {
        while (remaining >= 8) {
            __asm__ volatile("vmovdqu (%0), %%ymm0\n\t"
                             "vmovdqu %%ymm0, (%1)"
                             :
                             : "r"(src_bytes), "r"(dst_bytes)
                             : "memory");
            src_bytes += 32;
            dst_bytes += 32;
            remaining -= 8;
        }
        used_avx = 1;
    }

    if (framebuffer_has_sse2 && remaining >= 4) {
        while (remaining >= 4) {
            __asm__ volatile("movdqu (%0), %%xmm0\n\t"
                             "movdqu %%xmm0, (%1)"
                             :
                             : "r"(src_bytes), "r"(dst_bytes)
                             : "memory");
            src_bytes += 16;
            dst_bytes += 16;
            remaining -= 4;
        }
    }

    if (used_avx) {
        __asm__ volatile("vzeroupper" ::: "memory");
    }

    while (remaining--) {
        *((u32 *)dst_bytes) = *((const u32 *)src_bytes);
        dst_bytes += 4;
        src_bytes += 4;
    }
}

/**
 * @brief Fill a rectangle in the framebuffer using 32-bit pixels when possible.
 */
void framebuffer_fill_rect32(int x, int y, int width, int height, u32 color)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    // if (!framebuffer_supports_32bpp()) {
    //     for (int j = 0; j < height; j++) {
    //         for (int i = 0; i < width; i++) {
    //             framebuffer_putpixel(x + i, y + j, color);
    //         }
    //     }
    //     return;
    // }

    // const int screen_w = (int)vbe_info->width;
    // const int screen_h = (int)vbe_info->height;

    const int screen_w = 1024;
    const int screen_h = 768;
    const int pitch    = 4096;

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

    u32 span_pixels = (u32)(x1 - x0);
    u8 *row         = (u8 *)framebuffer + (u32)y0 * pitch + (u32)x0 * 4U;

    for (int j = y0; j < y1; j++) {
        framebuffer_fill_span32(row, span_pixels, color);
        row += pitch;
    }
}

/**
 * @brief Blit a horizontal span of 32-bit pixels to the framebuffer.
 */
void framebuffer_blit_span32(int x, int y, const u32 *src, u32 pixel_count)
{
    if (!src || pixel_count == 0) {
        return;
    }

    // if (!framebuffer_supports_32bpp()) {
    //     for (u32 i = 0; i < pixel_count; i++) {
    //         framebuffer_putpixel(x + (int)i, y, src[i]);
    //     }
    //     return;
    // }

    // const int screen_w = (int)vbe_info->width;
    // const int screen_h = (int)vbe_info->height;

    const int screen_w = 1024;
    const int screen_h = 768;

    if (y < 0 || y >= screen_h) {
        return;
    }

    int x0     = x;
    u32 offset = 0;
    if (x0 < 0) {
        offset = (u32)(-x0);
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
        pixel_count = (u32)(screen_w - x0);
    }

    const int pitch = 4096;
    u8 *row         = (u8 *)framebuffer + (u32)y * pitch + (u32)x0 * 4U;
    framebuffer_copy_span32(row, src + offset, pixel_count);
}

/**
 * @brief Write a single pixel to the framebuffer.
 */
void framebuffer_putpixel(int x, int y, u32 rgb)
{
    if (x < 0 || x >= 1024 || y < 0 || y >= 768) {
        return; // Out of bounds
    }

    const int pitch     = 4096;
    auto fb             = (u8 *)framebuffer;
    u32 bytes_per_pixel = 32 / 8;
    u8 *pixel           = fb + (y * pitch) + (x * bytes_per_pixel);
    *((u32 *)pixel)     = rgb;
}

/**
 * @brief Read a single pixel from the framebuffer.
 */
u32 framebuffer_getpixel(int x, int y)
{
    if (x < 0 || x >= 1024 || y < 0 || y >= 768) {
        return 0; // Out of bounds
    }
    const int pitch     = 4096;
    auto fb             = (u8 *)framebuffer;
    u32 bytes_per_pixel = 32 / 8;
    u8 *pixel           = fb + (y * pitch) + (x * bytes_per_pixel);
    return *((u32 *)pixel);
}

/**
 * @brief Allocate a drawing context with the given dimensions.
 *
 * @param width Context width in pixels.
 * @param height Context height in pixels.
 * @return Newly allocated context or nullptr on failure.
 */
video_context_t *context_new(u32 *fb, u16 width, u16 height)
{
    framebuffer  = fb;
    auto context = (video_context_t *)malloc(sizeof(video_context_t));
    if (!context) {
        return nullptr;
    }

    context->clip_rects = list_new();
    if (!context->clip_rects) {
        free(context);
        return nullptr;
    }

    context->width       = width;
    context->height      = height;
    context->clipping_on = 0;

    return context;
}

/**
 * @brief Draw a bitmap clipped against a single rectangle.
 *
 * @param context Target video context.
 * @param x Destination X coordinate.
 * @param y Destination Y coordinate.
 * @param draw_width Width of the region to draw.
 * @param draw_height Height of the region to draw.
 * @param clip_area Clipping rectangle in destination coordinates.
 * @param pixels Source ARGB pixel buffer.
 * @param stride Source stride in pixels.
 * @param src_origin_x X offset in the source image.
 * @param src_origin_y Y offset in the source image.
 */
void context_clipped_rect_bitmap(video_context_t *context, int x, int y, unsigned int draw_width,
                                 unsigned int draw_height, rect_t *clip_area, const u32 *pixels,
                                 unsigned int stride, int src_origin_x, int src_origin_y)
{
    int max_x = x + (int)draw_width;
    int max_y = y + (int)draw_height;

    // Translate the rectangle coordinates by the context translation values
    const int draw_origin_x = x + context->translate_x;
    const int draw_origin_y = y + context->translate_y;
    x                       = draw_origin_x;
    y                       = draw_origin_y;
    max_x += context->translate_x;
    max_y += context->translate_y;


    // Make sure we don't go outside of the clip region:
    if (x < clip_area->left) {
        x = clip_area->left;
    }

    if (y < clip_area->top) {
        y = clip_area->top;
    }

    if (max_x > clip_area->right + 1) {
        max_x = clip_area->right + 1;
    }

    if (max_y > clip_area->bottom + 1) {
        max_y = clip_area->bottom + 1;
    }

    if (x >= max_x || y >= max_y) {
        return;
    }

    const int src_base_x = src_origin_x + (x - draw_origin_x);
    const int src_base_y = src_origin_y + (y - draw_origin_y);

    const int span = max_x - x;
    if (span <= 0) {
        return;
    }

    for (int draw_y = y; draw_y < max_y; draw_y++) {
        const int src_y    = src_base_y + (draw_y - y);
        const u32 *src_row = pixels + (u32)src_y * stride + (u32)src_base_x;
        framebuffer_blit_span32(x, draw_y, src_row, (u32)span);
    }
}

/**
 * @brief Fill a rectangle while respecting a clipping region.
 *
 * @param context Target video context.
 * @param x Destination X coordinate.
 * @param y Destination Y coordinate.
 * @param width Width of the rectangle.
 * @param height Height of the rectangle.
 * @param clip_area Clipping rectangle to limit drawing.
 * @param color Fill colour.
 */
void context_clipped_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height,
                          rect_t *clip_area, u32 color)
{
    int max_x = x + (int)width;
    int max_y = y + (int)height;

    // Translate the rectangle coordinates by the context translation values
    x += context->translate_x;
    y += context->translate_y;
    max_x += context->translate_x;
    max_y += context->translate_y;

    // Make sure we don't go outside of the clip region:
    if (x < clip_area->left) {
        x = clip_area->left;
    }

    if (y < clip_area->top) {
        y = clip_area->top;
    }

    if (max_x > clip_area->right + 1) {
        max_x = clip_area->right + 1;
    }

    if (max_y > clip_area->bottom + 1) {
        max_y = clip_area->bottom + 1;
    }

    // Draw the rectangle into the framebuffer line-by line
    //(bonus points if you write an assembly routine to do it faster)
    if (x >= max_x || y >= max_y) {
        return;
    }

    const int width_span  = max_x - x;
    const int height_span = max_y - y;
    framebuffer_fill_rect32(x, y, width_span, height_span, color);
}


/**
 * @brief Draw a bitmap using the context's clipping configuration.
 *
 * @param context Target video context.
 * @param x Destination X coordinate.
 * @param y Destination Y coordinate.
 * @param width Bitmap width in pixels.
 * @param height Bitmap height in pixels.
 * @param pixels Source ARGB bitmap data.
 */
void context_draw_bitmap(video_context_t *context, int x, int y, unsigned int width, unsigned int height,
                         u32 *pixels)
{
    if (!pixels || width == 0 || height == 0) {
        return;
    }

    const unsigned int stride = width;
    int draw_x                = x;
    int draw_y                = y;
    unsigned int draw_width   = width;
    unsigned int draw_height  = height;
    int src_origin_x          = 0;
    int src_origin_y          = 0;

    if (draw_x < 0) {
        src_origin_x = -draw_x;
        if (src_origin_x >= (int)draw_width) {
            return;
        }
        draw_width -= (unsigned int)src_origin_x;
        draw_x = 0;
    }

    if (draw_y < 0) {
        src_origin_y = -draw_y;
        if (src_origin_y >= (int)draw_height) {
            return;
        }
        draw_height -= (unsigned int)src_origin_y;
        draw_y = 0;
    }

    if (draw_x + (int)draw_width > context->width) {
        const int overflow = draw_x + (int)draw_width - context->width;
        if (overflow >= (int)draw_width) {
            return;
        }
        draw_width -= (unsigned int)overflow;
    }

    if (draw_y + (int)draw_height > context->height) {
        const int overflow = draw_y + (int)draw_height - context->height;
        if (overflow >= (int)draw_height) {
            return;
        }
        draw_height -= (unsigned int)overflow;
    }

    if (draw_width == 0 || draw_height == 0) {
        return;
    }

    rect_t screen_area;

    // If there are clipping rects, draw the rect clipped to
    // each of them. Otherwise, draw unclipped (clipped to the screen)
    if (context->clip_rects->count) {
        for (unsigned int i = 0; i < context->clip_rects->count; i++) {
            auto clip_area = (rect_t *)list_get_at(context->clip_rects, i);
            context_clipped_rect_bitmap(context,
                                        draw_x,
                                        draw_y,
                                        draw_width,
                                        draw_height,
                                        clip_area,
                                        pixels,
                                        stride,
                                        src_origin_x,
                                        src_origin_y);
        }
    } else {

        if (!context->clipping_on) {

            screen_area.top    = 0;
            screen_area.left   = 0;
            screen_area.bottom = context->height - 1;
            screen_area.right  = context->width - 1;
            context_clipped_rect_bitmap(context,
                                        draw_x,
                                        draw_y,
                                        draw_width,
                                        draw_height,
                                        &screen_area,
                                        pixels,
                                        stride,
                                        src_origin_x,
                                        src_origin_y);
        }
    }
}

// Simple for-loop rectangle into a context
/**
 * @brief Fill a rectangular region, optionally respecting clip rectangles.
 *
 * @param context Target video context.
 * @param x Destination X coordinate.
 * @param y Destination Y coordinate.
 * @param width Rectangle width.
 * @param height Rectangle height.
 * @param color Fill colour.
 */
void context_fill_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height, u32 color)
{
    int max_x = x + (int)width;
    int max_y = y + (int)height;
    rect_t screen_area;

    // Make sure we don't try to draw offscreen
    if (max_x > context->width) {
        max_x = context->width;
    }

    if (max_y > context->height) {
        max_y = context->height;
    }

    if (x < 0) {
        x = 0;
    }

    if (y < 0) {
        y = 0;
    }

    width  = max_x - x;
    height = max_y - y;

    // If there are clipping rects, draw the rect clipped to
    // each of them. Otherwise, draw unclipped (clipped to the screen)
    if (context->clip_rects->count) {
        for (unsigned int i = 0; i < context->clip_rects->count; i++) {
            auto clip_area = (rect_t *)list_get_at(context->clip_rects, i);
            context_clipped_rect(context, x, y, width, height, clip_area, color);
        }
    } else {

        if (!context->clipping_on) {

            screen_area.top    = 0;
            screen_area.left   = 0;
            screen_area.bottom = context->height - 1;
            screen_area.right  = context->width - 1;
            context_clipped_rect(context, x, y, width, height, &screen_area, color);
        }
    }
}

// A horizontal line as a filled rect of height 1
/**
 * @brief Draw a horizontal line using the fill-rect helper.
 *
 * @param context Target video context.
 * @param x Starting X coordinate.
 * @param y Y coordinate of the line.
 * @param length Length in pixels.
 * @param color Line colour.
 */
void context_horizontal_line(video_context_t *context, int x, int y, unsigned int length, u32 color)
{
    context_fill_rect(context, x, y, length, 1, color);
}

// A vertical line as a filled rect of width 1
/**
 * @brief Draw a vertical line using the fill-rect helper.
 *
 * @param context Target video context.
 * @param x X coordinate of the line.
 * @param y Starting Y coordinate.
 * @param length Length in pixels.
 * @param color Line colour.
 */
void context_vertical_line(video_context_t *context, int x, int y, unsigned int length, u32 color)
{
    context_fill_rect(context, x, y, 1, length, color);
}

// Rectangle drawing using our horizontal and vertical lines
/**
 * @brief Draw an axis-aligned rectangle outline.
 *
 * @param context Target video context.
 * @param x Left coordinate.
 * @param y Top coordinate.
 * @param width Rectangle width.
 * @param height Rectangle height.
 * @param color Outline colour.
 */
void context_draw_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height, u32 color)
{
    context_horizontal_line(context, x, y, width, color);                         // top
    context_vertical_line(context, x, y + 1, height - 2, color);                  // left
    context_horizontal_line(context, x, y + (int)height - 1, width, color);       // bottom
    context_vertical_line(context, x + (int)width - 1, y + 1, height - 2, color); // right
}

// Update the clipping rectangles to only include those areas within both the
// existing clipping region AND the passed rect_t
/**
 * @brief Intersect the current clipping region with the provided rectangle.
 *
 * @param context Video context whose clipping set is updated.
 * @param rect Rectangle to intersect (freed by the function).
 */
void context_intersect_clip_rect(video_context_t *context, rect_t *rect)
{
    context->clipping_on = 1;

    list_t *output_rects = list_new();
    if (!output_rects) {
        return;
    }

    for (unsigned int i = 0; i < context->clip_rects->count; i++) {
        auto current_rect      = (rect_t *)list_get_at(context->clip_rects, i);
        rect_t *intersect_rect = rect_intersect(current_rect, rect);

        if (intersect_rect) {
            list_add(output_rects, intersect_rect);
        }
    }

    // Delete the original rectangle list
    while (context->clip_rects->count) {
        free(list_remove_at(context->clip_rects, 0));
    }
    free(context->clip_rects);

    // And re-point it to the new one we built above
    context->clip_rects = output_rects;

    // Free the input rect
    free(rect);
}

// split all existing clip rectangles against the passed rect
/**
 * @brief Remove the area of @p subtracted_rect from the clipping region.
 *
 * @param context Video context whose clips are modified.
 * @param subtracted_rect Rectangle to subtract.
 */
void context_subtract_clip_rect(video_context_t *context, rect_t *subtracted_rect)
{
    // Check each item already in the list to see if it overlaps with
    // the new rectangle
    context->clipping_on = 1;

    for (unsigned int i = 0; i < context->clip_rects->count;) {

        rect_t *cur_rect = list_get_at(context->clip_rects, i);

        // Standard rect intersect test (if no intersect, skip to next)
        // see here for an example of why this works:
        // http://stackoverflow.com/questions/306316/determine-if-two-rectangles-overlap-each-other#tab-top
        if (!(cur_rect->left <= subtracted_rect->right && cur_rect->right >= subtracted_rect->left &&
            cur_rect->top <= subtracted_rect->bottom && cur_rect->bottom >= subtracted_rect->top)) {

            i++;
            continue;
        }

        // If this rectangle does intersect with the new rectangle,
        // we need to split it
        list_remove_at(context->clip_rects, i);                      // Original will be replaced w/splits
        list_t *split_rects = rect_split(cur_rect, subtracted_rect); // Do the split
        free(cur_rect);                                              // We can throw this away now, we're done with it

        // Copy the split, non-overlapping result rectangles into the list
        while (split_rects->count) {
            cur_rect = (rect_t *)list_remove_at(split_rects, 0);
            list_add(context->clip_rects, cur_rect);
        }

        // Free the empty split_rect list
        free(split_rects);

        // Since we removed an item from the list, we need to start counting over again
        // In this way, we'll only exit this loop once nothing in the list overlaps
        i = 0;
    }
}

/**
 * @brief Add a non-overlapping clipping rectangle to the context.
 *
 * @param context Video context that receives the rectangle.
 * @param added_rect Rectangle to add; ownership passes to the context.
 */
void context_add_clip_rect(video_context_t *context, rect_t *added_rect)
{
    context_subtract_clip_rect(context, added_rect);

    // Now that we have made sure none of the existing rectangles overlap
    // with the new rectangle, we can finally insert it
    list_add(context->clip_rects, added_rect);
}

// Remove all of the clipping rects from the passed context object
/**
 * @brief Remove all clipping rectangles from the context.
 *
 * @param context Video context to clear.
 */
void context_clear_clip_rects(video_context_t *context)
{
    context->clipping_on = 0;

    // Remove and free until the list is empty
    while (context->clip_rects->count) {

        auto cur_rect = (rect_t *)list_remove_at(context->clip_rects, 0);
        free(cur_rect);
    }
}

// Draw a single character with the specified font color at the specified coordinates
/**
 * @brief Render a single character clipped to the supplied rectangle.
 *
 * @param context Target video context.
 * @param character Character to draw (ASCII subset).
 * @param x Left coordinate.
 * @param y Top coordinate.
 * @param color Colour to use for glyph pixels.
 * @param bound_rect Clipping rectangle bounding the glyph.
 */
void context_draw_char_clipped(video_context_t *context, char character, int x, int y, u32 color,
                               rect_t *bound_rect)
{
    int off_x   = 0;
    int off_y   = 0;
    int count_x = VESA_CHAR_WIDTH; // Font is 8x12
    int count_y = VESA_CHAR_HEIGHT;

    // Make sure to take context translation into account
    x += context->translate_x;
    y += context->translate_y;

    // Our font only handles the core set of 128 ASCII chars
    character &= 0x7F;

    // Check to see if the character is even inside of this rectangle
    if (x > bound_rect->right || (x + 8) <= bound_rect->left || y > bound_rect->bottom || (y + 12) <= bound_rect->top) {
        return;
    }

    // Limit the drawn portion of the character to the interior of the rect
    if (x < bound_rect->left) {
        off_x = bound_rect->left - x;
    }

    if ((x + 8) > bound_rect->right) {
        count_x = bound_rect->right - x + 1;
    }

    if (y < bound_rect->top) {
        off_y = bound_rect->top - y;
    }

    if ((y + 12) > bound_rect->bottom) {
        count_y = bound_rect->bottom - y + 1;
    }

    const int pitch_bytes = 4096;

    for (int font_y = off_y; font_y < count_y; font_y++) {
        u8 row_bits  = reverse_bits8(font8x12[font_y * 128 + character]);
        u32 row_mask = 0xFFu;
        if (off_x > 0) {
            row_mask &= ~((1u << off_x) - 1u);
        }
        if (count_x < VESA_CHAR_WIDTH) {
            row_mask &= (count_x == 0 ? 0u : ((1u << count_x) - 1u));
        }
        u8 active = (u8)(row_bits & row_mask);
        if (active == 0) {
            continue;
        }
        u32 *dst = (u32 *)((u8 *)framebuffer + (u32)(y + font_y) * pitch_bytes + (u32)x * 4U);
        while (active) {
            int bit = __builtin_ctz(active);
            dst[bit] = color;
            active &= active - 1;
        }
    }
}

// This will be a lot like context_fill_rect, but on a bitmap font character
/**
 * @brief Render a character, honouring the context's clip rectangles.
 *
 * @param context Target video context.
 * @param character Character to draw (ASCII subset).
 * @param x Left coordinate.
 * @param y Top coordinate.
 * @param color Colour to use for glyph pixels.
 */
void context_draw_char(video_context_t *context, char character, int x, int y, u32 color)
{
    // If there are clipping rects, draw the character clipped to
    // each of them. Otherwise, draw unclipped (clipped to the screen)
    if (context->clip_rects->count) {
        for (unsigned int i = 0; i < context->clip_rects->count; i++) {
            auto clip_area = (rect_t *)list_get_at(context->clip_rects, i);
            context_draw_char_clipped(context, character, x, y, color, clip_area);
        }
    } else {
        if (!context->clipping_on) {
            rect_t screen_area;

            screen_area.top    = 0;
            screen_area.left   = 0;
            screen_area.bottom = context->height - 1;
            screen_area.right  = context->width - 1;
            context_draw_char_clipped(context, character, x, y, color, &screen_area);
        }
    }
}

// Draw a line of text with the specified font color at the specified coordinates
/**
 * @brief Draw a null-terminated string starting at the given position.
 *
 * @param context Target video context.
 * @param string Text to render.
 * @param x Left coordinate.
 * @param y Top coordinate.
 * @param color Colour of the text.
 */
void context_draw_text(video_context_t *context, char *string, int x, int y, u32 color)
{
    for (; *string; x += VESA_CHAR_WIDTH)
        context_draw_char(context, *(string++), x, y, color);
}
