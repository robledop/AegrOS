#include <config.h>
#include <gui/icon.h>
#include <kernel_heap.h>
#include <string.h>

#include "gui/window.h"
#include "vesa.h"

/**
 * @brief Allocate and initialise an icon window.
 *
 * @param image Pointer to a 32x32 bitmap rendered beneath the caption.
 * @param x Screen X coordinate for the icon.
 * @param y Screen Y coordinate for the icon.
 * @return Pointer to the created icon or nullptr on failure.
 */
icon_t *icon_new(unsigned int *image, int16_t x, int16_t y)
{
    auto icon = (icon_t *)kzalloc(sizeof(icon_t));
    if (!icon) {
        return icon;
    }

    if (!window_init((window_t *)icon, x, y, 32, 32 + VESA_CHAR_HEIGHT, WIN_NODECORATION, nullptr)) {
        kfree(icon);
        return nullptr;
    }

    // Override default window callbacks
    icon->window.paint_function     = icon_paint;
    icon->window.mousedown_function = icon_mousedown_handler;

    // Init the new icon mousedown handler
    icon->onmousedown = nullptr;

    icon->image = image;

    return icon;
}

/**
 * @brief Paint callback used to draw the icon and its label.
 *
 * @param button_window Window representing the icon.
 */
void icon_paint(window_t *button_window)
{
    auto icon = (icon_t *)button_window;

    int title_len = (int)strlen(button_window->title);

    title_len *= VESA_CHAR_WIDTH;

    context_fill_rect(
        button_window->context, 0, 0, button_window->width, button_window->height, DESKTOP_BACKGROUND_COLOR);

    // TODO: Needs to be handled by the context
    // vesa_puticon32(button_window->x, button_window->y, icon->image);
    vesa_put_bitmap_32(button_window->x, button_window->y, icon->image);

    // Draw the title below the image
    if (button_window->title) {
        context_draw_text(button_window->context,
                          button_window->title,
                          (button_window->width / 2) - (title_len / 2),
                          button_window->height - VESA_CHAR_HEIGHT,
                          WIN_TITLE_TEXT_COLOR);
    }
}

/**
 * @brief Mouse down handler forwarding clicks to the icon callback.
 *
 * @param button_window Window representing the icon.
 * @param x Cursor X coordinate relative to the icon.
 * @param y Cursor Y coordinate relative to the icon.
 */
void icon_mousedown_handler(window_t *button_window, int16_t x, int16_t y)
{
    auto icon = (icon_t *)button_window;

    // window_invalidate((window_t *)icon, 0, 0, icon->window.height - 1, icon->window.width - 1);

    if (icon->onmousedown) {
        icon->onmousedown(icon, x, y);
    }
}
