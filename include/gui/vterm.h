#pragma once
#include <gui/button.h>
#include <gui/textbox.h>

struct vterm;

typedef void (*vterm_putchar_func)(char);

typedef struct vterm {
    window_t window; //'inherit' Window
    vterm_putchar_func putchar;
} vterm_t;

vterm_t *vterm_new(void);
