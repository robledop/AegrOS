#pragma once

#define MAX_WINDOWS 10
typedef struct window {
    char name[20];

    int x;
    int y;

    int width;
    int height;
} window_t;

window_t *window_create(char *name, int x, int y, int width, int height);
void window_draw(window_t *window);
void draw_windows();
bool window_was_clicked(window_t *window, int x, int y);
void window_print_message(window_t *window, char *message);
