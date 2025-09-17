#include <config.h>
#include <gui/video_context.h>
#include <gui/vterm.h>
#include <kernel_heap.h>
#include <memory.h>
#include "gui/window.h"

#define VTERM_WIDTH 600
#define VTERM_HEIGHT 400

static void vterm_scroll_up(vterm_t *vterm)
{
    uint16_t width  = vterm->window.width - (2 * WIN_BORDERWIDTH);
    uint16_t height = vterm->window.height - (WIN_TITLEHEIGHT + WIN_BORDERWIDTH);

    int buffer_width  = width / VESA_CHAR_WIDTH;
    int buffer_height = height / VESA_LINE_HEIGHT;

    // Move all lines up by one
    memmove(vterm->buffer, vterm->buffer + buffer_width, (buffer_height - 1) * buffer_width * sizeof(char));

    // Clear the last line
    memset(vterm->buffer + (buffer_height - 1) * buffer_width, 0, buffer_width * sizeof(char));

    vterm->cursor_y = buffer_height - 2;
}

static void vterm_repaint_characters(struct vterm *vterm, int old_cursor_x, int old_cursor_y)
{
    uint16_t width  = vterm->window.width;
    uint16_t height = vterm->window.height;

    auto dirty_regions = list_new();

    // Only mark the specific character positions as dirty
    int buffer_width  = width / VESA_CHAR_WIDTH;
    int buffer_height = height / VESA_LINE_HEIGHT;

    // Mark old cursor position as dirty
    if (vterm->cursor_x < buffer_width && vterm->cursor_y < buffer_height) {
        int char_x = WIN_BORDERWIDTH + old_cursor_x * VESA_CHAR_WIDTH + vterm->window.x;
        int char_y = WIN_TITLEHEIGHT + WIN_BORDERWIDTH + old_cursor_y * VESA_LINE_HEIGHT + vterm->window.y - 2;
        int bottom = char_y + VESA_CHAR_HEIGHT;
        int right  = char_x + VESA_CHAR_WIDTH;

        auto dirty_rect = rect_new(char_y, char_x, bottom, right);
        list_add(dirty_regions, dirty_rect);
        // context_draw_rect(
        //     vterm->window.context, char_x - 1, char_y - 1, VESA_CHAR_WIDTH + 1, VESA_CHAR_HEIGHT + 1, 0xFF00FF00);
    }

    // Mark new cursor position as dirty
    if (vterm->cursor_x < buffer_width && vterm->cursor_y < buffer_height) {
        int char_x = WIN_BORDERWIDTH + vterm->cursor_x * VESA_CHAR_WIDTH + vterm->window.x;
        int char_y = WIN_TITLEHEIGHT + WIN_BORDERWIDTH + vterm->cursor_y * VESA_LINE_HEIGHT + vterm->window.y - 2;
        int bottom = char_y + VESA_CHAR_HEIGHT;
        int right  = char_x + VESA_CHAR_WIDTH;

        auto dirty_rect = rect_new(char_y, char_x, bottom, right);
        list_add(dirty_regions, dirty_rect);
        // context_draw_rect(
        //     vterm->window.context, char_x - 1, char_y - 1, VESA_CHAR_WIDTH + 1, VESA_CHAR_HEIGHT + 1, 0xFFFF0000);
    }

    window_paint((window_t *)vterm, dirty_regions, 1);
}

void vterm_paint(window_t *window)
{
    auto vterm      = (vterm_t *)window;
    uint16_t width  = window->width;
    uint16_t height = window->height;

    context_fill_rect(window->context, 0, 0, window->width, window->height, 0xFF000000);
    for (int i = 0; i < (height / VESA_LINE_HEIGHT); i++) {
        for (int j = 0; j < (width / VESA_CHAR_WIDTH); j++) {
            auto cur_char = vterm->buffer[i * (width / VESA_CHAR_WIDTH) + j];
            if (cur_char != 0) {
                context_draw_char(window->context, cur_char, j * VESA_CHAR_WIDTH, i * VESA_LINE_HEIGHT, 0xFFFFFFFF);
            }
        }
    }
}

void vterm_clear_screen(struct vterm *vterm)
{
    uint16_t width  = vterm->window.width - (2 * WIN_BORDERWIDTH);
    uint16_t height = vterm->window.height - (WIN_TITLEHEIGHT + WIN_BORDERWIDTH);

    int buffer_width  = width / VESA_CHAR_WIDTH;
    int buffer_height = height / VESA_LINE_HEIGHT;

    memset(vterm->buffer, 0, buffer_width * buffer_height * sizeof(char));
    vterm->cursor_x = 0;
    vterm->cursor_y = 0;

    window_paint((window_t *)vterm, nullptr, 1);
}

void vterm_putchar(struct vterm *vterm, char c)
{
    int old_cursor_x = vterm->cursor_x;
    int old_cursor_y = vterm->cursor_y;

    uint16_t width  = vterm->window.width - (2 * WIN_BORDERWIDTH);
    uint16_t height = vterm->window.height - (WIN_TITLEHEIGHT + WIN_BORDERWIDTH);

    if (vterm->cursor_x >= width / VESA_CHAR_WIDTH || c == '\n') {
        vterm->cursor_x = 0;
        vterm->cursor_y++;
    } else if (vterm->cursor_y >= height / VESA_LINE_HEIGHT) {
        vterm->cursor_y = 0;
    } else {
        if (c == '\b') {
            if (vterm->cursor_x > 0) {
                vterm->cursor_x--;
                vterm->buffer[vterm->cursor_y * (width / VESA_CHAR_WIDTH) + vterm->cursor_x] = 0;
            }
        } else if (c == '\t') {
            vterm->cursor_x += 4; // Move cursor forward by 4 spaces
            if (vterm->cursor_x >= width / VESA_CHAR_WIDTH) {
                vterm->cursor_x = 0;
                vterm->cursor_y++;
            }
        } else if (c == '\r') {
            vterm->cursor_x = 0; // Carriage return
        } else {
            vterm->buffer[vterm->cursor_y * (width / VESA_CHAR_WIDTH) + vterm->cursor_x] = c;
            vterm->cursor_x++;
        }
    }

    int buffer_height = height / VESA_LINE_HEIGHT;

    if (vterm->cursor_y >= buffer_height - 1) {
        vterm_scroll_up(vterm);
        window_paint((window_t *)vterm, nullptr, 1);
    } else {
        vterm_repaint_characters(vterm, old_cursor_x, old_cursor_y);
    }
}

vterm_t *vterm_new()
{
    auto vterm = (vterm_t *)kzalloc(sizeof(vterm_t));
    if (!vterm) {
        return vterm;
    }

    uint16_t width  = VTERM_WIDTH;
    uint16_t height = VTERM_HEIGHT;

    if (!window_init((window_t *)vterm,
                     0,
                     0,
                     (2 * WIN_BORDERWIDTH) + width,
                     WIN_TITLEHEIGHT + WIN_BORDERWIDTH + height,
                     0,
                     nullptr)) {

        kfree(vterm);
        return nullptr;
    }

    uint16_t buffer_width  = width / VESA_CHAR_WIDTH;
    uint16_t buffer_height = height / VESA_LINE_HEIGHT;

    vterm->buffer = (char *)kzalloc(buffer_width * buffer_height);
    if (!vterm->buffer) {
        kfree(vterm);
        return nullptr;
    }

    window_set_title((window_t *)vterm, "Terminal");
    vterm->putchar               = vterm_putchar;
    vterm->window.paint_function = vterm_paint;
    vterm->clear_screen          = vterm_clear_screen;

    return vterm;
}