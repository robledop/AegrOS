#pragma once
#include <gui/button.h>
#include <gui/textbox.h>

struct vterm;

typedef void (*vterm_putchar_func)(struct vterm *, char);

typedef struct vterm {
    window_t window; //'inherit' Window
    vterm_putchar_func putchar;
    int cursor_x;
    int cursor_y;
    char *buffer;
    char *dirty_buffer;  // Track which characters need redrawing
    int last_painted_x;  // Last painted cursor position
    int last_painted_y;
} vterm_t;

vterm_t *vterm_new(void);
