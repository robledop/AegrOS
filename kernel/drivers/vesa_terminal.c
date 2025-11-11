#include <string.h>
#include <framebuffer.h>
#include <vesa_terminal.h>

#define MARGIN 15

static bool cursor_visible = true;
static int cursor_x        = MARGIN;
static int cursor_y        = MARGIN;
static int cursor_drawn_x  = -1;
static int cursor_drawn_y  = -1;

static bool v_param_escaping = false;
static bool v_param_inside   = false;
static bool v_private_mode   = false;
static int v_params[10]      = {0};
static int v_param_count     = 1;

static int forecolor = 0xFFFFFF;
static int backcolor = 0x000000;

static inline int vesa_max_cols(void)
{
    if (vbe_info == nullptr || vbe_info->width == 0) {
        return 80;
    }
    int usable = (int)vbe_info->width - 2 * MARGIN;
    if (usable <= 0) {
        return 1;
    }
    return usable / VESA_CHAR_WIDTH;
}

static inline int vesa_max_rows(void)
{
    if (vbe_info == nullptr || vbe_info->height == 0) {
        return 25;
    }
    int usable = (int)vbe_info->height - 2 * MARGIN;
    if (usable <= 0) {
        return 1;
    }
    return usable / VESA_LINE_HEIGHT;
}

static inline int vesa_text_height(void)
{
    return vesa_max_rows() * VESA_LINE_HEIGHT;
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

static inline int vesa_param_or_default(int index, int def)
{
    if (index < v_param_count && v_params[index] != 0) {
        return v_params[index];
    }
    return def;
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
    u8 *fb    = (u8 *)vbe_info->framebuffer;
    u32 pitch = vbe_info->pitch;
    u8 *dst   = fb + (size_t)MARGIN * pitch;
    u8 *src   = dst + VESA_LINE_HEIGHT * pitch;
    int rows = text_height - VESA_LINE_HEIGHT;
    if (rows > 0) {
        memmove(dst, src, (size_t)rows * pitch);
    }
    framebuffer_fill_rect32(0,
                            MARGIN + text_height - VESA_LINE_HEIGHT,
                            (int)vbe_info->width,
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
    case 0: { // cursor to end of screen
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


/**
 * @brief Map an ANSI colour code to an RGB value.
 */
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

/**
 * @brief Reset ANSI parsing state for the pixel terminal.
 */
void reset()
{
    v_param_escaping = false;
    v_param_inside   = false;
    v_private_mode   = false;

    v_param_inside = 0;
    memset(v_params, 0, sizeof(v_params));
    v_param_count = 1;
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


/**
 * @brief Process a single character within an ANSI escape sequence.
 */
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

    if (c == '?') {
        v_private_mode = true;
        return false;
    }

    switch (c) {
    case 'A': // Cursor up
        for (int n = vesa_param_or_default(0, 1); n > 0; n--) {
            cursor_up();
        }
        break;
    case 'B': // Cursor down
        for (int n = vesa_param_or_default(0, 1); n > 0; n--) {
            cursor_down();
        }
        break;
    case 'C': // Cursor forward
        for (int n = vesa_param_or_default(0, 1); n > 0; n--) {
            cursor_right();
        }
        break;
    case 'D': // Cursor back
        for (int n = vesa_param_or_default(0, 1); n > 0; n--) {
            cursor_left();
        }
        break;
    case 'H':
    case 'f':
        vesa_cursor_set_position(vesa_param_or_default(0, 1), vesa_param_or_default(1, 1));
        break;
    case 'J':
        vesa_clear_screen(vesa_param_or_default(0, 0));
        break;
    case 'K':
        vesa_clear_line(vesa_param_or_default(0, 0));
        break;
    case 'm':
        static bool bold = false;
        // static int blinking = 0;

        for (int i = 0; i < v_param_count; i++) {
            switch (v_params[i]) {
            case 0: // Reset all attributes
                forecolor = ansi_to_rgb(37, false);
                backcolor = ansi_to_rgb(40, false);
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
    case 'h':
        if (v_private_mode && vesa_param_or_default(0, 0) == 25) {
            vesa_show_cursor();
        }
        break;
    case 'l':
        if (v_private_mode && vesa_param_or_default(0, 0) == 25) {
            vesa_hide_cursor();
        }
        break;

    default:
        // Not implemented
        break;
    }

    v_private_mode = false;
    return true;
}

/**
 * @brief Update escape state machine, consuming characters when appropriate.
 */
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


#ifdef GRAPHICS
/**
 * @brief Print a character to the VESA terminal, handling ANSI escapes and cursor.
 */
void putchar(char c)
{
    if (v_handle_ansi_escape(c)) {
        return;
    }

    vesa_cursor_erase();

    if (c == '\n') {
        cursor_x = MARGIN;
        cursor_y += VESA_LINE_HEIGHT;
        vesa_maybe_scroll();
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
    if (cursor_x + VESA_CHAR_WIDTH + MARGIN > vbe_info->width) {
        cursor_x = MARGIN;
        cursor_y += VESA_LINE_HEIGHT;
        vesa_maybe_scroll();
    }

    vesa_clamp_cursor();
    vesa_draw_cursor();
}

#endif
