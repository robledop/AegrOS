#pragma once

#include <gui/rect.h>
#include <list.h>
#include <stdint.h>

typedef struct video_context {
    // uint32_t *buffer; // A pointer to our framebuffer
    uint16_t width; // The dimensions of the framebuffer
    uint16_t height;
    int translate_x; // Our new translation values
    int translate_y;
    list_t *clip_rects;
    uint8_t clipping_on;
} video_context_t;

video_context_t *context_new(uint16_t width, uint16_t height);
void context_fill_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height, uint32_t color);
void context_draw_bitmap(video_context_t *context, int x, int y, unsigned int width, unsigned int height,
                         uint32_t *pixels);
void context_horizontal_line(video_context_t *context, int x, int y, unsigned int length, uint32_t color);
void context_vertical_line(video_context_t *context, int x, int y, unsigned int length, uint32_t color);
void context_draw_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height, uint32_t color);
void context_intersect_clip_rect(video_context_t *context, rect_t *rect);
void context_subtract_clip_rect(video_context_t *context, rect_t *subtracted_rect);
void context_add_clip_rect(video_context_t *context, rect_t *rect);
void context_clear_clip_rects(video_context_t *context);
void context_draw_char(video_context_t *context, char character, int x, int y, uint32_t color);
void context_draw_text(video_context_t *context, char *string, int x, int y, uint32_t color);
