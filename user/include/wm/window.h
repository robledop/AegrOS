#pragma once

#include <wm/video_context.h>
#include <types.h>

#define WIN_BGCOLOR 0xFFDDDDDD
#define WIN_TITLE_COLOR 0xFF009070
#define WIN_TITLE_COLOR_INACTIVE 0xFF004040
#define WIN_TITLE_TEXT_COLOR 0xFFFFE0E0
#define WIN_TITLE_TEXT_COLOR_INACTIVE 0xFFBBBBBB
#define WIN_BORDER_COLOR 0xFF555599
#define WIN_TITLE_HEIGHT 25 // Includes the border
#define WIN_BORDER_WIDTH 2
#define WIN_TEXT_COLOR 0xFF111111
#define WIN_TITLE_MARGIN ((WIN_TITLE_HEIGHT - VESA_CHAR_HEIGHT) / 2)

// Some flags to define our window behavior
#define WIN_NODECORATION 0x1

// Forward struct declaration for function type declarations
struct window;

// Callback function type declarations
typedef void (*WindowPaintHandler)(struct window *);
typedef void (*WindowMousedownHandler)(struct window *, i16, i16);

typedef struct window {
    struct window *parent;
    i16 x;
    i16 y;
    u16 width;
    u16 height;
    u16 flags;
    video_context_t *context;
    struct window *drag_child;
    struct window *active_child;
    list_t *children;
    u16 drag_off_x;
    u16 drag_off_y;
    u8 last_button_state;
    WindowPaintHandler paint_function;
    WindowMousedownHandler mousedown_function;
    char *title;
} window_t;

window_t *window_new(i16 x, i16 y, u16 width, u16 height, u16 flags, video_context_t *context);
int window_init(window_t *window, i16 x, i16 y, u16 width, u16 height, u16 flags,
                video_context_t *context);
int window_screen_x(window_t *window);
int window_screen_y(window_t *window);
void window_paint(window_t *window, list_t *dirty_regions, u8 paint_children);
void window_process_mouse(window_t *window, u16 mouse_x, u16 mouse_y, u8 mouse_buttons);
void window_paint_handler(window_t *window);
void window_mousedown_handler(window_t *window, i16 x, i16 y);
list_t *window_get_windows_above(window_t *parent, window_t *child);
list_t *window_get_windows_below(window_t *parent, window_t *child);
void window_raise(window_t *window, u8 do_draw);
void window_move(window_t *window, int new_x, int new_y);
window_t *window_create_window(window_t *window, i16 x, i16 y, u16 width, i16 height, u16 flags);
void window_insert_child(window_t *window, window_t *child);
void window_invalidate(window_t *window, int top, int left, int bottom, int right);
void window_set_title(window_t *window, char *new_title);
void window_append_title(window_t *window, char *additional_chars);
