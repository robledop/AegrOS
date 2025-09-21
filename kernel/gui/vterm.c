#include <config.h>
#include <gui/video_context.h>
#include <gui/vterm.h>
#include <gui/window.h>
#include <kernel_heap.h>
#include <list.h>
#include <memory.h>
#include <stdint.h>

#define VTERM_WIDTH 800
#define VTERM_HEIGHT 400

bool vterm_param_escaping = false;
bool vterm_param_inside   = false;
int vterm_params[10]      = {0};
int vterm_param_count     = 1;

int vterm_forecolor = 0xFFFFFF;
int vterm_backcolor = DESKTOP_BACKGROUND_COLOR;

void vterm_reset()
{
    vterm_param_escaping = false;
    vterm_param_inside   = false;

    vterm_param_inside = 0;
    memset(vterm_params, 0, sizeof(vterm_params));
    vterm_param_count = 1;
}


int vterm_ansi_to_rgb(int ansi, bool bold)
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
        return bold ? 0xFFFFFF : 0xaaaaaa; // White
    default:
        return 0x000000; // Fallback: black
    }
}

void vterm_clear_screen(struct vterm *vterm)
{
    uint16_t width  = vterm->window.width - (2 * WIN_BORDER_WIDTH);
    uint16_t height = vterm->window.height - (WIN_TITLE_HEIGHT + (2 * WIN_BORDER_WIDTH));

    int buffer_width  = width / VESA_CHAR_WIDTH;
    int buffer_height = height / VESA_LINE_HEIGHT;

    memset(vterm->buffer, 0, buffer_width * buffer_height * sizeof(char));
    memset(vterm->color_buffer, 0, buffer_width * buffer_height * sizeof(uint32_t));
    vterm->cursor_x = 0;
    vterm->cursor_y = 0;

    window_paint((window_t *)vterm, nullptr, 1);
}

bool vterm_param_process(vterm_t *vterm, const int c)
{
    if (c >= '0' && c <= '9') {
        vterm_params[vterm_param_count - 1] = vterm_params[vterm_param_count - 1] * 10 + (c - '0');

        return false;
    }

    if (c == ';') {
        vterm_param_count++;

        return false;
    }

    switch (c) {
    case 'A': // Cursor up
        vterm->cursor_y++;
        break;
    case 'B': // Cursor down
        vterm->cursor_y--;
        break;
    case 'C': // Cursor forward
        vterm->cursor_x++;
        break;
    case 'D': // Cursor back
        vterm->cursor_x--;
        break;
    case 'H':
        const int row   = vterm_params[0];
        const int col   = vterm_params[1];
        vterm->cursor_x = col;
        vterm->cursor_y = row;
        break;
    case 'J':
        switch (vterm_params[0]) {
        case 2:
            vterm_clear_screen(vterm);
            break;
        default:
            // Not implemented
            break;
        }
        break;
    case 'm':
        static bool bold = false;
        // static int blinking = 0;

        for (int i = 0; i < vterm_param_count; i++) {
            switch (vterm_params[i]) {
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
                if (vterm_params[i] >= 30 && vterm_params[i] <= 47) {

                    if (vterm_params[i] >= 30 && vterm_params[i] <= 37) {
                        vterm->color_buffer[vterm->cursor_y * (vterm->buffer_width) + vterm->cursor_x] =
                            vterm_ansi_to_rgb(vterm_params[i], bold);
                    } else if (vterm_params[i] >= 40 && vterm_params[i] <= 47) {
                        vterm_backcolor = vterm_ansi_to_rgb(vterm_params[i], false);
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

bool vterm_handle_ansi_escape(vterm_t *vterm, const int c)
{
    if (c == 0x1B) {
        vterm_reset();
        vterm_param_escaping = true;
        return true;
    }

    if (vterm_param_escaping && c == '[') {
        vterm_reset();
        vterm_param_escaping = true;
        vterm_param_inside   = true;
        return true;
    }

    if (vterm_param_escaping && vterm_param_inside) {
        if (vterm_param_process(vterm, c)) {
            vterm_reset();
        }
        return true;
    }

    return false;
}

static void vterm_scroll_up(vterm_t *vterm)
{
    uint16_t width  = vterm->window.width - (2 * WIN_BORDER_WIDTH);
    uint16_t height = vterm->window.height - (WIN_TITLE_HEIGHT + (2 * WIN_BORDER_WIDTH));

    int buffer_width  = width / VESA_CHAR_WIDTH;
    int buffer_height = height / VESA_LINE_HEIGHT;

    // Move all lines up by one
    memmove(vterm->buffer, vterm->buffer + buffer_width, (buffer_height - 1) * buffer_width * sizeof(char));
    memmove(
        vterm->color_buffer, vterm->color_buffer + buffer_width, (buffer_height - 1) * buffer_width * sizeof(uint32_t));

    // Clear the last line
    memset(vterm->buffer + (buffer_height - 1) * buffer_width, 0, buffer_width * sizeof(char));
    memset(vterm->color_buffer + (buffer_height - 1) * buffer_width, 0, buffer_width * sizeof(uint32_t));

    vterm->cursor_y = buffer_height - 2;
}

static void vterm_get_char_cell(vterm_t *vterm, int cursor_x, int cursor_y, rect_t *out_rect)
{
    out_rect->top    = vterm->window.y + WIN_TITLE_HEIGHT + cursor_y * VESA_LINE_HEIGHT;
    out_rect->left   = vterm->window.x + WIN_BORDER_WIDTH + cursor_x * VESA_CHAR_WIDTH;
    out_rect->bottom = out_rect->top + VESA_LINE_HEIGHT;
    out_rect->right  = out_rect->left + VESA_CHAR_WIDTH;

    // For debugging
#if 0
    context_draw_rect(vterm->window.context,
                      out_rect->left - 1,
                      out_rect->top - 1,
                      out_rect->right - out_rect->left + 2,
                      out_rect->bottom - out_rect->top + 2,
                      0xFF00FF00);
#endif
}

static void vterm_repaint_characters(struct vterm *vterm, int old_cursor_x, int old_cursor_y)
{
    rect_t rect_storage[2];
    list_node_t node_storage[2];
    list_t dirty_regions = {0};
    list_node_t *tail    = nullptr;

    // Always repaint the previous cursor position
    vterm_get_char_cell(vterm, old_cursor_x, old_cursor_y, &rect_storage[0]);
    node_storage[0].payload = &rect_storage[0];
    node_storage[0].prev    = nullptr;
    node_storage[0].next    = nullptr;
    dirty_regions.root_node = &node_storage[0];
    dirty_regions.count     = 1;
    tail                    = &node_storage[0];

    // Repaint the new cursor position if it differs
    if (old_cursor_x != vterm->cursor_x || old_cursor_y != vterm->cursor_y) {
        vterm_get_char_cell(vterm, vterm->cursor_x, vterm->cursor_y, &rect_storage[1]);
        node_storage[1].payload = &rect_storage[1];
        node_storage[1].prev    = tail;
        node_storage[1].next    = nullptr;
        tail->next              = &node_storage[1];
        dirty_regions.count++;
    }

    window_paint((window_t *)vterm, &dirty_regions, 1);
}

void vterm_paint(window_t *window)
{
    auto vterm             = (vterm_t *)window;
    uint16_t buffer_width  = window->width / VESA_CHAR_WIDTH;
    uint16_t buffer_height = window->height / VESA_LINE_HEIGHT;
    vterm->buffer_width    = buffer_width;
    vterm->buffer_height   = buffer_height;

    uint32_t last_color = 0xFFFFFFFF;

    context_fill_rect(window->context, 0, 0, window->width, window->height, 0xFF000000);
    for (int y = 0; y < buffer_height; y++) {
        for (int x = 0; x < buffer_width; x++) {
            auto cur_char  = vterm->buffer[y * buffer_width + x];
            auto cur_color = vterm->color_buffer[y * buffer_width + x];
            if (cur_color == 0) {
                cur_color = last_color;
            } else {
                last_color = cur_color;
            }
            if (cur_char != 0) {
                context_draw_char(window->context, cur_char, x * VESA_CHAR_WIDTH, y * VESA_LINE_HEIGHT, cur_color);
            }
        }
    }
}

void vterm_putchar(struct vterm *vterm, char c)
{
    if (vterm_handle_ansi_escape(vterm, c)) {
        return; // Do not print escape sequences characters
    }

    int old_cursor_x = vterm->cursor_x;
    int old_cursor_y = vterm->cursor_y;

    uint16_t width  = vterm->window.width - (2 * WIN_BORDER_WIDTH);
    uint16_t height = vterm->window.height - (WIN_TITLE_HEIGHT + WIN_BORDER_WIDTH);

    if (vterm->cursor_x >= width / VESA_CHAR_WIDTH || c == '\n') {
        vterm->cursor_x = 0;
        vterm->cursor_y++;
    } else {
        if (c == '\b') {
            if (vterm->cursor_x > 0) {
                vterm->cursor_x--;
                vterm->buffer[vterm->cursor_y * (width / VESA_CHAR_WIDTH) + vterm->cursor_x] = 0;
            }
        } else if (c == '\t') {
            vterm->cursor_x += 4;
            if (vterm->cursor_x >= width / VESA_CHAR_WIDTH) {
                vterm->cursor_x = 0;
                vterm->cursor_y++;
            }
        } else if (c == '\r') {
            vterm->cursor_x = 0;
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
                     (2 * WIN_BORDER_WIDTH) + width,
                     WIN_TITLE_HEIGHT + WIN_BORDER_WIDTH + height,
                     0,
                     nullptr)) {

        kfree(vterm);
        return nullptr;
    }

    uint16_t buffer_width  = width / VESA_CHAR_WIDTH;
    uint16_t buffer_height = height / VESA_LINE_HEIGHT;

    vterm->buffer = (char *)kzalloc(buffer_width * buffer_height * sizeof(char));
    if (!vterm->buffer) {
        kfree(vterm);
        return nullptr;
    }

    vterm->color_buffer = (uint32_t *)kzalloc(buffer_width * buffer_height * sizeof(uint32_t));
    if (!vterm->color_buffer) {
        kfree(vterm->buffer);
        kfree(vterm);
        return nullptr;
    }

    window_set_title((window_t *)vterm, "Terminal");
    vterm->putchar               = vterm_putchar;
    vterm->window.paint_function = vterm_paint;
    vterm->clear_screen          = vterm_clear_screen;

    return vterm;
}
