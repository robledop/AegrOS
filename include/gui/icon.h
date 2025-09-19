#pragma once
#include <gui/window.h>

struct icon;

typedef void (*IconMousedownHandler)(struct icon *, int, int);

typedef struct icon {
    window_t window;
    unsigned int *image;
    char *text;
    IconMousedownHandler onmousedown;
} icon_t;

icon_t *icon_new(unsigned int *image, int16_t x, int16_t y);
void icon_mousedown_handler(window_t *button_window, int16_t x, int16_t y);
void icon_paint(window_t *button_window);
