#include <gui/vterm.h>
#include <kernel_heap.h>

#include "config.h"
#include "gui/video_context.h"
#include "string.h"
#include "vesa.h"

int c_x = 0;
int c_y = 0;
char *buffer;

void vterm_putchar(char c)
{
    if (c_x >= 600 / VESA_CHAR_WIDTH || c == '\n') {
        c_x = 0;
        c_y++;
    } else if (c_y >= 400 / VESA_LINE_HEIGHT) {
        c_y = 0;
    } else {
        buffer[c_y * (600 / VESA_CHAR_WIDTH) + c_x] = c;
        c_x++;
    }
}

void vterm_paint(window_t *window)
{
    context_fill_rect(window->context, 0, 0, window->width, window->height, 0xFF000000);
    for (int i = 0; i < (400 / VESA_LINE_HEIGHT); i++) {
        for (int j = 0; j < (600 / VESA_CHAR_WIDTH); j++) {
            auto cur_char = buffer[i * (600 / VESA_CHAR_WIDTH) + j];
            if (cur_char != 0) {
                context_draw_char(window->context, cur_char, j * VESA_CHAR_WIDTH, i * VESA_LINE_HEIGHT, 0xFFFFFFFF);
            }
        }
    }
}

vterm_t *vterm_new()
{
    auto vterm = (vterm_t *)kzalloc(sizeof(vterm_t));
    if (!vterm) {
        return vterm;
    }

    if (!window_init((window_t *)vterm,
                     0,
                     0,
                     (2 * WIN_BORDERWIDTH) + 600,
                     WIN_TITLEHEIGHT + WIN_BORDERWIDTH + 400,
                     0,
                     nullptr)) {

        kfree(vterm);
        return nullptr;
    }

    buffer = (char *)kzalloc((600 / VESA_CHAR_WIDTH) * (400 / VESA_LINE_HEIGHT));
    if (!buffer) {
        kfree(vterm);
        return nullptr;
    }

    window_set_title((window_t *)vterm, "Terminal");
    vterm->putchar               = vterm_putchar;
    vterm->window.paint_function = vterm_paint;

    return vterm;
}