#pragma once
#include <stdint.h>

#define TITLE_BAR_HEIGHT 18
#define TITLE_BAR_COLOR 0x00CCCC
#define TITLE_TEXT_COLOR 0xFFFFFF
#define BORDER_COLOR 0xFFFFFF
#define BACKGROUND_COLOR 0xFFCCCC
#define MAX_CHILDREN 10

typedef struct window window_t;

typedef void (*window_paint_handler)(window_t *);
typedef void (*window_mouse_down_handler)(window_t *, int, int);

typedef struct window {
    window_t *parent;
    char name[20];
    int x;
    int y;
    int width;
    int height;

    window_t *children[MAX_CHILDREN];
    int children_count;
    uint8_t last_button_state;
    uint16_t mouse_x;
    uint16_t mouse_y;
    struct window *drag_child;
    uint16_t drag_x;
    uint16_t drag_y;
    window_paint_handler paint_handler;
    window_mouse_down_handler mouse_down_handler;
} window_t;


window_t *window_create(char *name, int x, int y, int width, int height);
void window_draw(window_t *window);
void window_print_message(window_t *window, char *message);
bool window_was_clicked(window_t *window, int x, int y);
