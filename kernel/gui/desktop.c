#include <config.h>
#include <gui/desktop.h>
#include <gui/rect.h>
#include <gui/video_context.h>
#include <kernel_heap.h>
#include <mouse.h>
#include <stdint.h>
#include <string.h>
#include <vesa.h>

// Mouse image data
#define CA 0xFF000000 // Black
#define CB 0xFFFFFFFF // White
#define CD 0x00000000 // Clear

unsigned int mouse_img[MOUSE_BUFSZ] = {
    CA, CD, CD, CD, CD, CD, CD, CD, CD, CD, CD, CA, CA, CD, CD, CD, CD, CD, CD, CD, CD, CD, CA, CB, CA, CD, CD, CD, CD,
    CD, CD, CD, CD, CA, CB, CB, CA, CD, CD, CD, CD, CD, CD, CD, CA, CB, CB, CB, CA, CD, CD, CD, CD, CD, CD, CA, CB, CB,
    CB, CB, CA, CD, CD, CD, CD, CD, CA, CB, CB, CB, CB, CB, CA, CD, CD, CD, CD, CA, CB, CB, CB, CB, CB, CB, CA, CD, CD,
    CD, CA, CB, CB, CB, CB, CB, CB, CB, CA, CD, CD, CA, CB, CB, CB, CB, CB, CB, CB, CB, CA, CD, CA, CB, CB, CB, CB, CB,
    CB, CB, CB, CB, CA, CA, CA, CA, CA, CB, CB, CB, CA, CA, CA, CA, CD, CD, CD, CD, CA, CB, CB, CA, CD, CD, CD, CD, CD,
    CD, CD, CA, CB, CB, CA, CD, CD, CD, CD, CD, CD, CD, CD, CA, CB, CB, CA, CD, CD, CD, CD, CD, CD, CD, CA, CB, CB, CA,
    CD, CD, CD, CD, CD, CD, CD, CD, CA, CB, CA, CD, CD, CD, CD, CD, CD, CD, CD, CD, CA, CA, CD, CD};

desktop_t *desktop_new(video_context_t *context, uint32_t *wallpaper)
{
    auto desktop = (desktop_t *)kzalloc(sizeof(desktop_t));
    if (!desktop) {
        return desktop;
    }

    // Initialize the window_t bits of our desktop
    if (!window_init((window_t *)desktop, 0, 0, context->width, context->height, WIN_NODECORATION, context)) {
        kfree(desktop);
        return nullptr;
    }

    // Override our paint function
    desktop->window.paint_function = desktop_paint_handler;

    // Now continue by filling out the desktop-unique properties
    desktop->window.last_button_state = 0;

    // Init mouse to the center of the screen
    desktop->mouse_x   = (int16_t)(desktop->window.context->width / 2);
    desktop->mouse_y   = (int16_t)(desktop->window.context->height / 2);
    desktop->wallpaper = wallpaper;

    mouse_set_position(desktop->mouse_x, desktop->mouse_y);

    return desktop;
}

// Paint the desktop
void desktop_paint_handler(window_t *desktop_window)
{
    if (((desktop_t *)desktop_window)->wallpaper) {
        context_draw_bitmap(desktop_window->context,
                            0,
                            0,
                            desktop_window->context->width,
                            desktop_window->context->height,
                            ((desktop_t *)desktop_window)->wallpaper);
    } else {
        context_fill_rect(desktop_window->context,
                          0,
                          0,
                          desktop_window->context->width,
                          desktop_window->context->height,
                          DESKTOP_BACKGROUND_COLOR);
    }

    context_draw_text(desktop_window->context,
                      "AegrOS",
                      desktop_window->width - (int)strlen("AegrOS") * VESA_CHAR_WIDTH - 10,
                      desktop_window->height - 22,
                      0xFFFFFFFF);
}

// Our overload of the Window_process_mouse function used to capture the screen mouse position
void desktop_process_mouse(desktop_t *desktop, uint16_t mouse_x, uint16_t mouse_y, uint8_t mouse_buttons)
{
    // Do the old generic mouse handling
    window_process_mouse((window_t *)desktop, mouse_x, mouse_y, mouse_buttons);

    // window_t painting now happens inside of the window raise and move operations

    // Build a dirty rect list for the mouse area
    list_t *dirty_list = list_new();
    if (!dirty_list) {
        return;
    }

    rect_t *mouse_rect = rect_new(
        desktop->mouse_y, desktop->mouse_x, desktop->mouse_y + MOUSE_HEIGHT - 1, desktop->mouse_x + MOUSE_WIDTH - 1);
    if (!mouse_rect) {
        kfree(dirty_list);
        return;
    }

    list_add(dirty_list, mouse_rect);

    // Do a dirty update for the desktop, which will, in turn, do a
    // dirty update for all affected child windows
    window_paint((window_t *)desktop, dirty_list, 1);

    // Clean up mouse dirty list
    list_remove_at(dirty_list, 0);
    kfree(dirty_list);
    kfree(mouse_rect);

    // Update mouse position
    desktop->mouse_x = mouse_x;
    desktop->mouse_y = mouse_y;

    // No more hacky mouse, instead we're going to rather inefficiently
    // copy the pixels from our mouse image into the framebuffer
    for (int y = 0; y < MOUSE_HEIGHT; y++) {

        // Make sure we don't draw off the bottom of the screen
        if ((y + mouse_y) >= desktop->window.context->height) {
            break;
        }

        for (int x = 0; x < MOUSE_WIDTH; x++) {

            // Make sure we don't draw off the right side of the screen
            if ((x + mouse_x) >= desktop->window.context->width) {
                break;
            }

            // Don't place a pixel if it's transparent (still going off of ABGR here,
            // change to suit your palette)
            if (mouse_img[y * MOUSE_WIDTH + x] & 0xFF000000) {
                vesa_putpixel(x + mouse_x, y + mouse_y, mouse_img[y * MOUSE_WIDTH + x]);
            }
        }
    }
}
