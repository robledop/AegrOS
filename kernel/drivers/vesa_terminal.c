#include <string.h>
#include <framebuffer.h>
#include <vesa_terminal.h>
#include <console.h>
#include <ansi.h>
#include "printf.h"

#ifdef GRAPHICS
#define MARGIN 0
#define VESA_TTY_COLS 80
#define VESA_TTY_ROWS 25

static bool cursor_visible = true;
static int cursor_x        = MARGIN;
static int cursor_y        = MARGIN;
static int cursor_drawn_x  = -1;
static int cursor_drawn_y  = -1;

static u32 forecolor = 0x999999;
static u32 backcolor = 0x000000;

u16 vesa_terminal_columns(void)
{
    if (vbe_info != nullptr && vbe_info->width >= VESA_CHAR_WIDTH) {
        return (u16)(vbe_info->width / VESA_CHAR_WIDTH);
    }
    return VESA_TTY_COLS;
}

u16 vesa_terminal_rows(void)
{
    if (vbe_info != nullptr && vbe_info->height >= VESA_LINE_HEIGHT) {
        return (u16)(vbe_info->height / VESA_LINE_HEIGHT);
    }
    return VESA_TTY_ROWS;
}

static inline int vesa_max_cols(void)
{
    return vesa_terminal_columns();
}

static inline int vesa_max_rows(void)
{
    return vesa_terminal_rows();
}

static inline int vesa_text_width(void)
{
    return (int)vesa_terminal_columns() * VESA_CHAR_WIDTH;
}

static inline int vesa_text_height(void)
{
    return (int)vesa_terminal_rows() * VESA_LINE_HEIGHT;
}

static inline int vesa_bottom_limit(void)
{
    return MARGIN + vesa_text_height();
}

static inline void vesa_clamp_cursor(void)
{
    int max_x = MARGIN + (vesa_max_cols() - 1) * VESA_CHAR_WIDTH;
    int max_y = MARGIN + (vesa_max_rows() - 1) * VESA_LINE_HEIGHT;
    if (cursor_x < MARGIN) {
        cursor_x = MARGIN;
    } else if (cursor_x > max_x) {
        cursor_x = max_x;
    }
    if (cursor_y < MARGIN) {
        cursor_y = MARGIN;
    } else if (cursor_y > max_y) {
        cursor_y = max_y;
    }
}

static inline void vesa_cursor_erase(void)
{
    if (cursor_drawn_x >= 0 && cursor_drawn_y >= 0) {
        framebuffer_erase_cursor(cursor_drawn_x, cursor_drawn_y);
        cursor_drawn_x = cursor_drawn_y = -1;
    }
}

static inline void vesa_draw_cursor(void)
{
    vesa_cursor_erase();
    if (!cursor_visible) {
        return;
    }
    framebuffer_draw_cursor(cursor_x, cursor_y);
    cursor_drawn_x = cursor_x;
    cursor_drawn_y = cursor_y;
}

static inline void vesa_hide_cursor(void)
{
    cursor_visible = false;
    vesa_cursor_erase();
}

static inline void vesa_show_cursor(void)
{
    cursor_visible = true;
    vesa_draw_cursor();
}

static void vesa_cursor_set_position(int row, int col)
{
    if (row < 1) {
        row = 1;
    }
    if (col < 1) {
        col = 1;
    }
    int max_rows = vesa_max_rows();
    int max_cols = vesa_max_cols();
    if (row > max_rows) {
        row = max_rows;
    }
    if (col > max_cols) {
        col = max_cols;
    }

    cursor_x = MARGIN + (col - 1) * VESA_CHAR_WIDTH;
    cursor_y = MARGIN + (row - 1) * VESA_LINE_HEIGHT;
    vesa_draw_cursor();
}

#ifdef GRAPHICS
static void vesa_scroll_up_text_area(void)
{
    if (vbe_info == nullptr) {
        return;
    }
    const int text_height = vesa_text_height();
    if (text_height <= 0) {
        return;
    }
    u8 *fb    = framebuffer_kernel_bytes();
    if (fb == nullptr) {
        return;
    }
    u32 pitch = vbe_info->pitch;
    u8 *dst   = fb + (size_t)MARGIN * pitch;
    u8 *src   = dst + VESA_LINE_HEIGHT * pitch;
    int rows  = text_height - VESA_LINE_HEIGHT;
    if (rows > 0) {
        memmove(dst, src, (size_t)rows * pitch);
    }
    framebuffer_fill_rect32(MARGIN,
                            MARGIN + text_height - VESA_LINE_HEIGHT,
                            vesa_text_width(),
                            VESA_LINE_HEIGHT,
                            backcolor);
    cursor_drawn_x = cursor_drawn_y = -1;
}

static void vesa_maybe_scroll(void)
{
    int bottom = vesa_bottom_limit();
    if (cursor_y >= bottom) {
        vesa_scroll_up_text_area();
        cursor_y = bottom - VESA_LINE_HEIGHT;
    }
}
#endif

static void vesa_clear_line(int mode)
{
    int line_start_x = MARGIN;
    int line_width   = vesa_max_cols() * VESA_CHAR_WIDTH;
    switch (mode) {
    case 0: // cursor to end
        framebuffer_fill_rect32(cursor_x,
                                cursor_y,
                                line_start_x + line_width - cursor_x,
                                VESA_LINE_HEIGHT,
                                backcolor);
        break;
    case 1: // start to cursor
        framebuffer_fill_rect32(line_start_x,
                                cursor_y,
                                cursor_x - line_start_x,
                                VESA_LINE_HEIGHT,
                                backcolor);
        break;
    case 2: // entire line
        framebuffer_fill_rect32(line_start_x,
                                cursor_y,
                                line_width,
                                VESA_LINE_HEIGHT,
                                backcolor);
        cursor_x = line_start_x;
        break;
    default:
        break;
    }
}

static void vesa_clear_screen(int mode)
{
    switch (mode) {
    case 0: {
        // cursor to end of screen
        vesa_clear_line(0);
        int remaining = (int)vbe_info->height - (cursor_y + VESA_LINE_HEIGHT);
        if (remaining > 0) {
            framebuffer_fill_rect32(MARGIN,
                                    cursor_y + VESA_LINE_HEIGHT,
                                    vesa_max_cols() * VESA_CHAR_WIDTH,
                                    remaining,
                                    backcolor);
        }
        break;
    }
    case 2: // entire screen
        framebuffer_clear_screen(backcolor);
        cursor_x = MARGIN;
        cursor_y = MARGIN;
        break;
    default:
        break;
    }
    vesa_draw_cursor();
}


static u32 ansi_rgb_from_index(int idx, bool bright)
{
    static const u32 normal[8] = {
        0x000000, // black
        0x990000, // red
        0x009900, // green
        0x999900, // yellow
        0x555599, // blue
        0x990099, // magenta
        0x009999, // cyan
        0x999999  // white/gray
    };
    static const u32 intense[8] = {
        0x555555,
        0xFF0000,
        0x00FF00,
        0xFFFF22,
        0x8888FF,
        0xFF00FF,
        0x00FFFF,
        0xFFFFFF
    };
    if (idx < 0 || idx > 7) {
        idx = 7;
    }
    return bright ? intense[idx] : normal[idx];
}

static u32 vesa_foreground_from_code(int code, bool bold)
{
    bool bright = false;
    int idx     = 7;
    if (code >= 90 && code <= 97) {
        bright = true;
        idx    = code - 90;
    } else if (code >= 30 && code <= 37) {
        idx = code - 30;
    } else if (code == 39) {
        idx = 7;
    } else {
        return ansi_rgb_from_index(7, bold);
    }
    return ansi_rgb_from_index(idx, bright || bold);
}

static u32 vesa_background_from_code(int code)
{
    int idx = 0;
    if (code >= 100 && code <= 107) {
        code -= 60;
    }
    if (code >= 40 && code <= 47) {
        idx = code - 40;
    } else if (code != 49) {
        return ansi_rgb_from_index(0, false);
    }
    return ansi_rgb_from_index(idx, false);
}


/**
 * @brief Move the on-screen cursor one text line up.
 */
void cursor_up()
{
    if (cursor_y > MARGIN) {
        cursor_y -= VESA_LINE_HEIGHT;
    }
    if (cursor_y < MARGIN) {
        cursor_y = MARGIN;
    }
    vesa_draw_cursor();
}

/**
 * @brief Move the on-screen cursor one text line down.
 */
void cursor_down()
{
    int max_y = MARGIN + (vesa_max_rows() - 1) * VESA_LINE_HEIGHT;
    if (cursor_y < max_y) {
        cursor_y += VESA_LINE_HEIGHT;
    }
    vesa_draw_cursor();
}

/**
 * @brief Move the on-screen cursor one character left.
 */
void cursor_left()
{
    if (cursor_x > MARGIN) {
        cursor_x -= VESA_CHAR_WIDTH;
    }
    if (cursor_x < MARGIN) {
        cursor_x = MARGIN;
    }
    vesa_draw_cursor();
}

/**
 * @brief Move the on-screen cursor one character right.
 */
void cursor_right()
{
    int max_x = MARGIN + (vesa_max_cols() - 1) * VESA_CHAR_WIDTH;
    if (cursor_x < max_x) {
        cursor_x += VESA_CHAR_WIDTH;
    }
    vesa_draw_cursor();
}


// VESA-specific ANSI callback implementations
static void vesa_cursor_up_cb(int n)
{
    for (int i = 0; i < n; i++) {
        cursor_up();
    }
}

static void vesa_cursor_down_cb(int n)
{
    for (int i = 0; i < n; i++) {
        cursor_down();
    }
}

static void vesa_cursor_left_cb(int n)
{
    for (int i = 0; i < n; i++) {
        cursor_left();
    }
}

static void vesa_cursor_right_cb(int n)
{
    for (int i = 0; i < n; i++) {
        cursor_right();
    }
}

static void vesa_cursor_set_position_cb(int row, int col)
{
    vesa_cursor_set_position(row, col);
}

static void vesa_clear_screen_cb(int mode)
{
    vesa_clear_screen(mode);
}

static void vesa_clear_line_cb(int mode)
{
    vesa_clear_line(mode);
}

static void vesa_set_graphics_mode_cb(int count, const int *params)
{
    static bool bold    = false;
    static bool reverse = false;
    static int fg_code  = 37;
    static int bg_code  = 40;

    for (int i = 0; i < count; i++) {
        const int p = params[i];
        switch (p) {
        case 0:
            bold = false;
            reverse = false;
            fg_code = 37;
            bg_code = 40;
            break;
        case 1:
            bold = true;
            break;
        case 7:
            reverse = true;
            break;
        case 22:
            bold = false;
            break;
        case 27:
            reverse = false;
            break;
        case 39:
            fg_code = 37;
            break;
        case 49:
            bg_code = 40;
            break;
        default:
            if ((p >= 30 && p <= 37) || (p >= 90 && p <= 97)) {
                fg_code = p;
            } else if ((p >= 40 && p <= 47) || (p >= 100 && p <= 107)) {
                bg_code = p;
            }
            break;
        }
    }

    // Apply colors (swap if reverse video is active)
    if (reverse) {
        forecolor = vesa_background_from_code(bg_code);
        backcolor = vesa_foreground_from_code(fg_code, bold);
    } else {
        forecolor = vesa_foreground_from_code(fg_code, bold);
        backcolor = vesa_background_from_code(bg_code);
    }
}

static void vesa_show_cursor_cb(void)
{
    vesa_show_cursor();
}

static void vesa_hide_cursor_cb(void)
{
    vesa_hide_cursor();
}

static void vesa_report_cursor_position_cb(void)
{
    int col = 1;
    int row = 1;
    if (cursor_x >= MARGIN) {
        col = (cursor_x - MARGIN) / VESA_CHAR_WIDTH + 1;
    }
    if (cursor_y >= MARGIN) {
        row = (cursor_y - MARGIN) / VESA_LINE_HEIGHT + 1;
    }
    // Build response manually to avoid snprintf issues
    char resp[32];
    resp[0] = '\x1b';
    resp[1] = '[';
    int pos = 2;

    // Add row number
    if (row >= 10) {
        resp[pos++] = '0' + (row / 10);
        resp[pos++] = '0' + (row % 10);
    } else {
        resp[pos++] = '0' + row;
    }

    resp[pos++] = ';';

    // Add col number
    if (col >= 100) {
        resp[pos++] = '0' + (col / 100);
        resp[pos++] = '0' + ((col / 10) % 10);
        resp[pos++] = '0' + (col % 10);
    } else if (col >= 10) {
        resp[pos++] = '0' + (col / 10);
        resp[pos++] = '0' + (col % 10);
    } else {
        resp[pos++] = '0' + col;
    }

    resp[pos++] = 'R';
    resp[pos]   = '\0';

    console_queue_input_locked(resp);
}

static struct ansi_callbacks vesa_callbacks = {
    .cursor_up = vesa_cursor_up_cb,
    .cursor_down = vesa_cursor_down_cb,
    .cursor_left = vesa_cursor_left_cb,
    .cursor_right = vesa_cursor_right_cb,
    .cursor_set_position = vesa_cursor_set_position_cb,
    .clear_screen = vesa_clear_screen_cb,
    .clear_line = vesa_clear_line_cb,
    .set_graphics_mode = vesa_set_graphics_mode_cb,
    .show_cursor = vesa_show_cursor_cb,
    .hide_cursor = vesa_hide_cursor_cb,
    .report_cursor_position = vesa_report_cursor_position_cb,
};


/**
 * @brief Initialize VESA terminal
 */
void vesa_terminal_init(void)
{
    ansi_set_callbacks(&vesa_callbacks);
}

/**
 * @brief Print a character to the VESA terminal, handling ANSI escapes and cursor.
 */
void putchar(char c)
{
    if (ansi_handle_escape(c)) {
        return;
    }

    vesa_cursor_erase();

    if (c == '\n') {
        cursor_x = MARGIN;
        cursor_y += VESA_LINE_HEIGHT;
        // Only auto-scroll if output post-processing is enabled
        if (console_should_auto_scroll()) {
            vesa_maybe_scroll();
        }
        vesa_clamp_cursor();
        vesa_draw_cursor();
        return;
    }

    if (c == '\r') {
        cursor_x = MARGIN;
        vesa_draw_cursor();
        return;
    }

    if (c == '\t') {
        cursor_x += 4 * VESA_CHAR_WIDTH;
        vesa_clamp_cursor();
        vesa_draw_cursor();
        return;
    }

    if (c == '\b') {
        cursor_x -= VESA_CHAR_WIDTH;
        if (cursor_x < MARGIN) {
            cursor_x = MARGIN;
        }
        framebuffer_fill_rect32(cursor_x,
                                cursor_y,
                                VESA_CHAR_WIDTH,
                                VESA_LINE_HEIGHT,
                                backcolor);
        vesa_draw_cursor();
        return;
    }

    framebuffer_put_char8(c, cursor_x, cursor_y, forecolor, backcolor);
    cursor_x += VESA_CHAR_WIDTH;
    if ((cursor_x - MARGIN) / VESA_CHAR_WIDTH >= vesa_max_cols()) {
        cursor_x = MARGIN;
        cursor_y += VESA_LINE_HEIGHT;
        // Only auto-scroll if output post-processing is enabled
        if (console_should_auto_scroll()) {
            vesa_maybe_scroll();
        }
    }

    vesa_clamp_cursor();
    vesa_draw_cursor();
}

#endif
