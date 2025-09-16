#include <memory.h>
#include <vesa.h>
#include <vesa_terminal.h>

#include "config.h"
#include "gui/video_context.h"
#include "kernel.h"

#define MARGIN 15

bool cursor_visible = true;
int cursor_x        = MARGIN;
int cursor_y        = MARGIN;

bool v_param_escaping = false;
bool v_param_inside   = false;
int v_params[10]      = {0};
int v_param_count     = 1;

int forecolor = 0xFFFFFF;
int backcolor = DESKTOP_BACKGROUND_COLOR;


extern struct vbe_mode_info *vbe_info;
putchar_func_t putchar_func = nullptr;

int ansi_to_rgb(int ansi, bool bold)
{
    switch (ansi) {
    case 30:
        return 0x000000; // Black
    case 31:
        return bold ? 0xFF0000 : 0x990000; // Red
    case 32:
        return bold ? 0x00FF00 : 0x009900; // Green
    case 33:
        return bold ? 0xFFFF22 : 0x999900; // Yellow
    case 34:
        return bold ? 0x8888FF : 0x555599; // Blue
    case 35:
        return bold ? 0xFF00FF : 0x990099; // Magenta
    case 36:
        return bold ? 0x00FFFF : 0x009999; // Cyan
    case 37:
        return bold ? 0xFFFFFF : 0x999999; // White
    default:
        return 0x000000; // Fallback: black
    }
}
void reset()
{
    v_param_escaping = false;
    v_param_inside   = false;

    v_param_inside = 0;
    memset(v_params, 0, sizeof(v_params));
    v_param_count = 1;
}

void cursor_up()
{
    if (cursor_y > 0) {
        cursor_y -= VESA_LINE_HEIGHT;
    }

    vesa_draw_cursor(cursor_x + VESA_CHAR_WIDTH, cursor_y);
}

void cursor_down()
{
    if (cursor_y < vbe_info->height - VESA_LINE_HEIGHT) {
        cursor_y += VESA_LINE_HEIGHT;
    }

    vesa_draw_cursor(cursor_x + VESA_CHAR_WIDTH, cursor_y);
}

void cursor_left()
{
    if (cursor_x > 0) {
        cursor_x -= VESA_CHAR_WIDTH;
    }

    vesa_draw_cursor(cursor_x + VESA_CHAR_WIDTH, cursor_y);
}

void cursor_right()
{
    if (cursor_x < vbe_info->width - VESA_CHAR_WIDTH) {
        cursor_x += VESA_CHAR_WIDTH;
    }

    vesa_draw_cursor(cursor_x + VESA_CHAR_WIDTH, cursor_y);
}


bool v_param_process(const int c)
{
    if (c >= '0' && c <= '9') {
        v_params[v_param_count - 1] = v_params[v_param_count - 1] * 10 + (c - '0');

        return false;
    }

    if (c == ';') {
        v_param_count++;

        return false;
    }

    switch (c) {
    case 'A': // Cursor up
        cursor_up();
        break;
    case 'B': // Cursor down
        cursor_down();
        break;
    case 'C': // Cursor forward
        cursor_left();
        break;
    case 'D': // Cursor back
        cursor_right();
        break;
    case 'H':
        // not implemented yet

        // const int row = v_params[0];
        // const int col = v_params[1];
        // update_cursor(row, col);
        break;
    case 'J':
        switch (v_params[0]) {
        case 2:
            vesa_clear_screen(DESKTOP_BACKGROUND_COLOR);
            cursor_x = MARGIN;
            cursor_y = MARGIN;
            break;
        default:
            // Not implemented
            break;
        }
        break;
    case 'm':
        static bool bold = false;
        // static int blinking = 0;

        for (int i = 0; i < v_param_count; i++) {
            switch (v_params[i]) {
            case 0:
                // blinking = 0;
                bold = false;
                break;
            case 1:
                bold = true;
                break;
            case 5:
                // blinking = 1;
                break;
            case 22:
                bold = false;
                break;
            case 25:
                // blinking = 0;
                break;
            default:
                if (v_params[i] >= 30 && v_params[i] <= 47) {

                    if (v_params[i] >= 30 && v_params[i] <= 37) {
                        forecolor = ansi_to_rgb(v_params[i], bold);
                    } else if (v_params[i] >= 40 && v_params[i] <= 47) {
                        backcolor = ansi_to_rgb(v_params[i], false);
                    }

                    // attribute = ((blinking & 1) << 7) | ((backcolor & 0x07) << 4) | (forecolor & 0x0F);
                }
            }
        }
        break;

    default:
        // Not implemented
    }

    return true;
}

bool v_handle_ansi_escape(const int c)
{
    if (c == 0x1B) {
        reset();
        v_param_escaping = true;
        return true;
    }

    if (v_param_escaping && c == '[') {
        reset();
        v_param_escaping = true;
        v_param_inside   = true;
        return true;
    }

    if (v_param_escaping && v_param_inside) {
        if (v_param_process(c)) {
            reset();
        }
        return true;
    }

    return false;
}

void vesa_terminal_init(putchar_func_t func)
{
    putchar_func = func;
}


#ifdef PIXEL_RENDERING // If this is not defined, then we use the putchar defined in vga_buffer.c
void putchar(char c)
{
    // vesa_erase_cursor(cursor_x, cursor_y);

    // if (c == '\n') {
    //     cursor_x = MARGIN;
    //     cursor_y += VESA_LINE_HEIGHT;
    //     if (cursor_y + VESA_LINE_HEIGHT > vbe_info->height) {
    //         vesa_scroll_up();
    //         cursor_y = vbe_info->height - VESA_LINE_HEIGHT;
    //     }
    //     return;
    // }
    //
    // if (c == '\t') {
    //     cursor_x += 4 * VESA_CHAR_WIDTH;
    //     return;
    // }
    //
    // if (c == '\b') {
    //     vesa_erase_cursor(cursor_x + VESA_CHAR_WIDTH, cursor_y);
    //     cursor_x -= VESA_CHAR_WIDTH;
    //     vesa_erase_cursor(cursor_x, cursor_y);
    //     vesa_draw_cursor(cursor_x, cursor_y);
    //     return;
    // }

    if (v_handle_ansi_escape(c)) {
        return;
    }

    if (putchar_func) {
        putchar_func(c);
    } else {
        vesa_put_char8(c, cursor_x, cursor_y, forecolor, backcolor);
    }

    cursor_x += VESA_CHAR_WIDTH;
    if (cursor_x + VESA_CHAR_WIDTH + MARGIN > vbe_info->width) {
        cursor_x = MARGIN;
        cursor_y += VESA_LINE_HEIGHT;
    }

    if (cursor_visible) {
        vesa_draw_cursor(cursor_x, cursor_y);
    }
}

#endif
