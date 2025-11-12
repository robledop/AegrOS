#pragma once

#include <wm/rect.h>
#include <list.h>
#include <types.h>

typedef struct video_context {
    // u32 *buffer; // A pointer to our framebuffer
    u16 width; // The dimensions of the framebuffer
    u16 height;
    int translate_x; // Our new translation values
    int translate_y;
    list_t *clip_rects;
    u8 clipping_on;
} video_context_t;

video_context_t *context_new(u16 width, u16 height);
void context_fill_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height, u32 color);
void context_draw_bitmap(video_context_t *context, int x, int y, unsigned int width, unsigned int height,
                         u32 *pixels);
void context_horizontal_line(video_context_t *context, int x, int y, unsigned int length, u32 color);
void context_vertical_line(video_context_t *context, int x, int y, unsigned int length, u32 color);
void context_draw_rect(video_context_t *context, int x, int y, unsigned int width, unsigned int height, u32 color);
void context_intersect_clip_rect(video_context_t *context, rect_t *rect);
void context_subtract_clip_rect(video_context_t *context, rect_t *subtracted_rect);
void context_add_clip_rect(video_context_t *context, rect_t *rect);
void context_clear_clip_rects(video_context_t *context);
void context_draw_char(video_context_t *context, char character, int x, int y, u32 color);
void context_draw_text(video_context_t *context, char *string, int x, int y, u32 color);
