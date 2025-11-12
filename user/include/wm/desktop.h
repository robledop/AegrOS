#pragma once

#include <wm/video_context.h>
#include <wm/window.h>
#include <types.h>

#define MOUSE_WIDTH 11
#define MOUSE_HEIGHT 18
#define MOUSE_BUFSZ (MOUSE_WIDTH * MOUSE_HEIGHT)

#define DESKTOP_BACKGROUND_COLOR 0x113399

#define VESA_CHAR_WIDTH 8
#define VESA_CHAR_HEIGHT 12
#define VESA_LINE_HEIGHT 14

typedef struct desktop
{
    window_t window; // "Inherits" window class
    i16 mouse_x;
    i16 mouse_y;
    u32 *wallpaper;
} desktop_t;

desktop_t *desktop_new(video_context_t *context, u32 *wallpaper);
void desktop_paint_handler(window_t *desktop_window);
void desktop_process_mouse(desktop_t *desktop, u16 mouse_x, u16 mouse_y, u16 mouse_buttons);
