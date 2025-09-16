#include <config.h>
#include <gui/button.h>
#include <kernel_heap.h>
#include <string.h>

button_t *button_new(int16_t x, int16_t y, int16_t w, int16_t h)
{
    // Like a Desktop, this is just a special kind of window
    auto button = (button_t *)kzalloc(sizeof(button_t));
    if (!button) {
        return button;
    }

    if (!window_init((window_t *)button, x, y, w, h, WIN_NODECORATION, nullptr)) {

        kfree(button);
        return nullptr;
    }

    // Override default window callbacks
    button->window.paint_function     = button_paint;
    button->window.mousedown_function = button_mousedown_handler;

    // Init the new button mousedown handler
    button->onmousedown = nullptr;

    // And clear the toggle value
    button->color_toggle = 0;

    return button;
}

void button_paint(window_t *button_window)
{
    auto button = (button_t *)button_window;

    uint32_t border_color;
    if (button->color_toggle) {
        border_color = WIN_TITLECOLOR;
    } else {
        border_color = WIN_BGCOLOR - 0x101010;
    }

    context_fill_rect(button_window->context, 1, 1, button_window->width - 1, button_window->height - 1, WIN_BGCOLOR);
    context_draw_rect(button_window->context, 0, 0, button_window->width, button_window->height, 0xFF000000);
    context_draw_rect(button_window->context, 3, 3, button_window->width - 6, button_window->height - 6, border_color);
    context_draw_rect(button_window->context, 4, 4, button_window->width - 8, button_window->height - 8, border_color);

    int title_len = (int)strlen(button_window->title);

    // Convert it into pixels
    title_len *= VESA_CHAR_WIDTH;

    // Draw the title centered within the button
    if (button_window->title) {
        context_draw_text(button_window->context,
                          button_window->title,
                          (button_window->width / 2) - (title_len / 2),
                          (button_window->height / 2) - 6,
                          WIN_BORDERCOLOR);
    }
}

// This just sets and resets the toggle
void button_mousedown_handler(window_t *button_window, int16_t x, int16_t y)
{
    auto button = (button_t *)button_window;

    button->color_toggle = !button->color_toggle;

    // Since the button has visibly changed state, we need to invalidate the
    // area that needs updating
    window_invalidate((window_t *)button, 0, 0, button->window.height - 1, button->window.width - 1);

    // Fire the associated button click event if it exists
    if (button->onmousedown) {
        button->onmousedown(button, x, y);
    }
}
