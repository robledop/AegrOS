#include <config.h>
#include <font.h>
#include <gui/rect.h>
#include <gui/video_context.h>
#include <kernel_heap.h>
#include <stdint.h>
#include <vesa.h>

video_context_t *context_new(uint16_t width, uint16_t height)
{
    auto context = (video_context_t *)kzalloc(sizeof(video_context_t));
    if (!context) {
        return nullptr;
    }

    context->clip_rects = list_new();
    if (!context->clip_rects) {
        kfree(context);
        return nullptr;
    }

    context->width       = width;
    context->height      = height;
    context->clipping_on = 0;

    return context;
}


void context_clipped_rect_bitmap(video_context_t *context,
                                 int x, int y, unsigned int draw_width, unsigned int draw_height,
                                 rect_t *clip_area, uint32_t *pixels, unsigned int stride,
                                 int src_origin_x, int src_origin_y)
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

    // Draw the rectangle into the framebuffer line-by line
    //(bonus points if you write an assembly routine to do it faster)
    for (int draw_y = y; draw_y < max_y; draw_y++) {
        const int src_y = src_base_y + (draw_y - y);
        for (int draw_x = x; draw_x < max_x; draw_x++) {
            const int src_x = src_base_x + (draw_x - x);
            vesa_putpixel(draw_x, draw_y, pixels[src_y * stride + src_x]);
        }
    }
}

void context_clipped_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height,
                          rect_t *clip_area, uint32_t color)
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
    for (; y < max_y; y++)
        for (int cur_x = x; cur_x < max_x; cur_x++)
            vesa_putpixel(cur_x, y, color);
}



void context_draw_bitmap(video_context_t *context, int x, int y, unsigned int width, unsigned int height,
                         uint32_t *pixels)
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
        draw_x      = 0;
    }

    if (draw_y < 0) {
        src_origin_y = -draw_y;
        if (src_origin_y >= (int)draw_height) {
            return;
        }
        draw_height -= (unsigned int)src_origin_y;
        draw_y       = 0;
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
void context_fill_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height, uint32_t color)
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
void context_horizontal_line(video_context_t *context, int x, int y, unsigned int length, uint32_t color)
{
    context_fill_rect(context, x, y, length, 1, color);
}

// A vertical line as a filled rect of width 1
void context_vertical_line(video_context_t *context, int x, int y, unsigned int length, uint32_t color)
{
    context_fill_rect(context, x, y, 1, length, color);
}

// Rectangle drawing using our horizontal and vertical lines
void context_draw_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height, uint32_t color)
{
    context_horizontal_line(context, x, y, width, color);                         // top
    context_vertical_line(context, x, y + 1, height - 2, color);                  // left
    context_horizontal_line(context, x, y + (int)height - 1, width, color);       // bottom
    context_vertical_line(context, x + (int)width - 1, y + 1, height - 2, color); // right
}

// Update the clipping rectangles to only include those areas within both the
// existing clipping region AND the passed rect_t
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
    while (context->clip_rects->count)
        kfree(list_remove_at(context->clip_rects, 0));
    kfree(context->clip_rects);

    // And re-point it to the new one we built above
    context->clip_rects = output_rects;

    // Free the input rect
    kfree(rect);
}

// split all existing clip rectangles against the passed rect
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
        kfree(cur_rect);                                             // We can throw this away now, we're done with it

        // Copy the split, non-overlapping result rectangles into the list
        while (split_rects->count) {
            cur_rect = (rect_t *)list_remove_at(split_rects, 0);
            list_add(context->clip_rects, cur_rect);
        }

        // Free the empty split_rect list
        kfree(split_rects);

        // Since we removed an item from the list, we need to start counting over again
        // In this way, we'll only exit this loop once nothing in the list overlaps
        i = 0;
    }
}

void context_add_clip_rect(video_context_t *context, rect_t *added_rect)
{
    context_subtract_clip_rect(context, added_rect);

    // Now that we have made sure none of the existing rectangles overlap
    // with the new rectangle, we can finally insert it
    list_add(context->clip_rects, added_rect);
}

// Remove all of the clipping rects from the passed context object
void context_clear_clip_rects(video_context_t *context)
{
    context->clipping_on = 0;

    // Remove and free until the list is empty
    while (context->clip_rects->count) {

        auto cur_rect = (rect_t *)list_remove_at(context->clip_rects, 0);
        kfree(cur_rect);
    }
}

// Draw a single character with the specified font color at the specified coordinates
void context_draw_char_clipped(video_context_t *context, char character, int x, int y, uint32_t color,
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

    // Now we do the actual pixel plotting loop
    for (int font_y = off_y; font_y < count_y; font_y++) {

        // Capture the current line of the specified char
        // Just a normal bmp[y * width + x], but in this
        // case we're dealing with an array of 1bpp
        // 8-bit-wide character lines
        uint8_t shift_line = font_array[font_y * 128 + character];

        // Pre-shift the line by the x-offset
        shift_line <<= off_x;

        for (int font_x = off_x; font_x < count_x; font_x++) {

            // Get the current leftmost bit of the current
            // line of the character and, if it's set, plot a pixel
            if (shift_line & 0x80) {
                vesa_putpixel(font_x + x, font_y + y, color);
            }
            // context->buffer[(font_y + y) * context->width + (font_x + x)] = color;

            // Shift in the next bit
            shift_line <<= 1;
        }
    }
}

// This will be a lot like context_fill_rect, but on a bitmap font character
void context_draw_char(video_context_t *context, char character, int x, int y, uint32_t color)
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
void context_draw_text(video_context_t *context, char *string, int x, int y, uint32_t color)
{
    for (; *string; x += VESA_CHAR_WIDTH)
        context_draw_char(context, *(string++), x, y, color);
}