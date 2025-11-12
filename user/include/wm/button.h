#pragma once
#include <wm/window.h>
#include <types.h>

struct button;

typedef void (*ButtonMousedownHandler)(struct button *, int, int);

typedef struct button {
    window_t window;
    u8 color_toggle;
    ButtonMousedownHandler onmousedown;
} button_t;

button_t *button_new(i16 x, i16 y, i16 w, i16 h);
void button_mousedown_handler(window_t *button_window, i16 x, i16 y);
void button_paint(window_t *button_window);