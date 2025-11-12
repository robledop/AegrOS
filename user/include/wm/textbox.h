#pragma once
#include <wm/window.h>
#include <types.h>

typedef struct textbox {
    window_t window;
} textbox_t;

textbox_t *textbox_new(i16 x, i16 y, int width, int height);
void textbox_paint(window_t *text_box_window);
