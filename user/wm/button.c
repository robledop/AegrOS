#include <wm/button.h>
#include <user.h>
#include <types.h>
#include "wm/desktop.h"

/**
 * @brief Allocate and initialise a GUI button.
 *
 * Creates a window-backed button positioned at the requested coordinates.
 *
 * @param x Left position in pixels.
 * @param y Top position in pixels.
 * @param w Button width in pixels.
 * @param h Button height in pixels.
 * @return Pointer to the new button or nullptr on failure.
 */
button_t *button_new(i16 x, i16 y, i16 w, i16 h)
{
    // Like a Desktop, this is just a special kind of window
    auto button = (button_t *)malloc(sizeof(button_t));
    if (!button) {
        return button;
    }

    if (!window_init((window_t *)button, x, y, w, h, WIN_NODECORATION, nullptr)) {

        free(button);
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

/**
 * @brief Paint callback that renders the button contents.
 *
 * @param button_window Window representing the button.
 */
void button_paint(window_t *button_window)
{
    auto button = (button_t *)button_window;

    u32 border_color;
    if (button->color_toggle) {
        border_color = WIN_TITLE_COLOR;
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
                          WIN_TEXT_COLOR);
    }
}

/**
 * @brief Mouse down handler invoked when the button is pressed.
 *
 * @param button_window Window associated with the button.
 * @param x Cursor X coordinate relative to the button.
 * @param y Cursor Y coordinate relative to the button.
 */
void button_mousedown_handler(window_t *button_window, i16 x, i16 y)
{
    auto button = (button_t *)button_window;

    // button->color_toggle = !button->color_toggle;

    // Since the button has visibly changed state, we need to invalidate the
    // area that needs updating
    window_invalidate((window_t *)button, 0, 0, button->window.height - 1, button->window.width - 1);

    // Fire the associated button click event if it exists
    if (button->onmousedown) {
        button->onmousedown(button, x, y);
    }
}
