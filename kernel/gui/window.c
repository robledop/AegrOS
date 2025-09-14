#include <icons.h>
#include <kernel_heap.h>
#include <printf.h>
#include <string.h>
#include <vesa.h>
#include <window.h>

void window_draw(window_t *window)
{

    int x = window->x;
    int y = window->y;
    int w = window->width;
    int h = window->height;

    // Border
    vesa_draw_rect(x, y, w, h, BORDER_COLOR);

    // Title bar
    vesa_fill_rect(x + 1, y + 1, w - 2, TITLE_BAR_HEIGHT, TITLE_BAR_COLOR);
    vesa_print_string(window->name, (int)strlen(window->name), x + 10, y + 5, TITLE_TEXT_COLOR, TITLE_BAR_COLOR);

    // Draw the window contents
    vesa_fill_rect(x + 1, y + TITLE_BAR_HEIGHT + 1, w - 2, h - TITLE_BAR_HEIGHT - 2, BACKGROUND_COLOR);
    vesa_puticon32(x + 2, y + 20, computer_icon_1);
}

bool window_was_clicked(window_t *window, int x, int y)
{
    if (window == nullptr) {
        return false;
    }

    if (x >= window->x && x <= window->x + window->width && y >= window->y && y <= window->y + window->height) {
        window_print_message(window, "You clicked me!");
        return true;
    }

    return false;
}

void window_print_message(window_t *window, char *message)
{
    auto message_length = strlen(message);
    auto center_x       = window->x + window->width / 2 - (message_length * 8) / 2;
    auto center_y       = window->y + window->height / 2;
    vesa_print_string(message, (int)strlen(message), (int)center_x, center_y, 0x000000, 0xFFCCCC);
}
