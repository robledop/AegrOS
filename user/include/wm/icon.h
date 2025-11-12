#pragma once
#include <wm/window.h>
#include <types.h>

struct icon;

typedef void (*IconMousedownHandler)(struct icon *, int, int);

typedef struct icon {
    window_t window;
    unsigned int *image;
    char *text;
    IconMousedownHandler onmousedown;
} icon_t;

icon_t *icon_new(unsigned int *image, i16 x, i16 y);
void icon_mousedown_handler(window_t *button_window, i16 x, i16 y);
void icon_paint(window_t *button_window);
