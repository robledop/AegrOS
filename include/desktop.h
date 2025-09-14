#pragma once

#include <window.h>

typedef struct desktop {
    window_t window; // desktop_t "inherits" window_t and can be cast into one
    uint16_t mouse_x;
    uint16_t mouse_y;
} desktop_t;

void desktop_draw_windows();
void desktop_process_mouse_event();
