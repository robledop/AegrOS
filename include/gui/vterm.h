#pragma once
#include <gui/window.h>

struct vterm;

typedef void (*vterm_putchar_func)(struct vterm *, char);
typedef void (*vterm_clear_screen_func)(struct vterm *);

typedef struct vterm {
    window_t window; //'inherit' Window
    vterm_putchar_func putchar;
    vterm_clear_screen_func clear_screen;
    int cursor_x;
    int cursor_y;
    char *buffer;
    int buffer_width;
    int buffer_height;
} vterm_t;

vterm_t *vterm_new();
