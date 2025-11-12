#pragma once
#include <wm/window.h>
#include <stddef.h>
#include <types.h>

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
    u32 *color_buffer;
    int buffer_width;
    int buffer_height;
    bool needs_full_repaint;
    bool has_dirty_region;
    int dirty_min_col;
    int dirty_min_row;
    int dirty_max_col;
    int dirty_max_row;
} vterm_t;

vterm_t *vterm_new();
void vterm_set_active(vterm_t *vterm);
vterm_t *vterm_active(void);
void vterm_write(vterm_t *vterm, const char *data, size_t length);
void vterm_flush(vterm_t *vterm);
