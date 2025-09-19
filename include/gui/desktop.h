#pragma once

#include <gui/video_context.h>
#include <gui/window.h>

#define MOUSE_WIDTH 11
#define MOUSE_HEIGHT 18
#define MOUSE_BUFSZ (MOUSE_WIDTH * MOUSE_HEIGHT)

typedef struct desktop {
    window_t window; // "Inherits" window class
    uint16_t mouse_x;
    uint16_t mouse_y;
    uint32_t *wallpaper;
} desktop_t;

desktop_t *desktop_new(video_context_t *context, uint32_t *wallpaper);
void desktop_paint_handler(window_t *desktop_window);
void desktop_process_mouse(desktop_t *desktop, uint16_t mouse_x, uint16_t mouse_y, uint8_t mouse_buttons);
