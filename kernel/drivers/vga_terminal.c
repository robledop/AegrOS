#include "ansi.h"
#include "io.h"
#include "vga_terminal.h"

#include "console.h"
#include "defs.h"
#include "memlayout.h"
#include "printf.h"
#include "string.h"
#include "x86.h"

#ifndef GRAPHICS

u8 attribute = DEFAULT_ATTRIBUTE;
static int cursor_y;
static int cursor_x;

int ansi_to_vga_foreground[] = {
    0x00, // Black
    0x04, // Red
    0x02, // Green
    0x0E, // Yellow (Brown in VGA)
    0x01, // Blue
    0x05, // Magenta
    0x03, // Cyan
    0x07  // White (Light Grey in VGA)
};

int ansi_to_vga_background[] = {
    0x00, // Black
    0x40, // Red
    0x20, // Green
    0xE0, // Yellow (Brown in VGA)
    0x10, // Blue
    0x50, // Magenta
    0x30, // Cyan
    0x70  // White (Light Grey in VGA)
};

void terminal_clear();

static void vga_fill_words(u16 *dst, u32 count, u16 value)
{
    if (count == 0) {
        return;
    }

    if (!memory_sse_available()) {
        for (u32 i = 0; i < count; i++) {
            dst[i] = value;
        }
        return;
    }

    u8 *bytes         = (u8 *)dst;
    u32 remaining     = count;
    u16 pattern_words[16];
    int pattern_init = 0;
    const u32 saved_cr0 = rcr0();
    clts();
    int used_avx = 0;

    if (memory_avx_available() && remaining >= 16) {
        if (!pattern_init) {
            for (int i = 0; i < 16; i++) {
                pattern_words[i] = value;
            }
            pattern_init = 1;
        }
        __asm__ volatile("vmovdqu (%0), %%ymm0" : : "r"(pattern_words));
        while (remaining >= 16) {
            __asm__ volatile("vmovdqu %%ymm0, (%0)" : : "r"(bytes) : "memory");
            bytes += 32;
            remaining -= 16;
        }
        used_avx = 1;
    }

    if (remaining >= 8) {
        if (!pattern_init) {
            for (int i = 0; i < 8; i++) {
                pattern_words[i] = value;
            }
            pattern_init = 1;
        }
        __asm__ volatile("movdqu (%0), %%xmm0" : : "r"(pattern_words));
        while (remaining >= 8) {
            __asm__ volatile("movdqu %%xmm0, (%0)" : : "r"(bytes) : "memory");
            bytes += 16;
            remaining -= 8;
        }
    }

    if (used_avx) {
        __asm__ volatile("vzeroupper" ::: "memory");
    }

    lcr0(saved_cr0);

    u16 *tail = (u16 *)bytes;
    while (remaining-- > 0) {
        *tail++ = value;
    }
}

/** @brief Enable the hardware text cursor using standard scanlines */
static void enable_cursor(void)
{
    outb(CRTPORT, 0x0A);
    outb(CRTPORT + 1, (inb(CRTPORT + 1) & 0xC0) | 14);
    outb(CRTPORT, 0x0B);
    outb(CRTPORT + 1, (inb(CRTPORT + 1) & 0xE0) | 15);
}


/**
 * @brief Move the hardware cursor to the given position.
 *
 * @param row Zero-based row index within the VGA text buffer.
 * @param col Zero-based column index within the VGA text buffer.
 */
void update_cursor(const int row, const int col)
{
    const u16 position = (row * VGA_WIDTH) + col;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (position >> 8) & 0xFF);
}

/**
 * @brief Write a character cell to video memory.
 *
 * @param c ASCII character to render.
 * @param attr VGA attribute byte controlling foreground and background colors.
 * @param x Column position or -1 to use the current cursor column.
 * @param y Row position or -1 to use the current cursor row.
 */
void vga_buffer_write(const char c, const u8 attr, const int x, const int y)
{
    const int pos_x = x == -1 ? cursor_x : x;
    const int pos_y = y == -1 ? cursor_y : y;

    volatile u16 *where = (volatile u16 *)VIDEO_MEMORY + (pos_y * VGA_WIDTH + pos_x);
    *where              = c | (attr << 8);
}


/**
 * @brief Scroll the visible text buffer up by one row and clear the final line.
 */
void scroll_screen()
{
    auto const video_memory = (u8 *)VIDEO_MEMORY;

    // Move all rows up by one
    memmove(video_memory, video_memory + ROW_SIZE, SCREEN_SIZE - ROW_SIZE);

    u16 *row = (u16 *)(video_memory + SCREEN_SIZE - ROW_SIZE);
    const u16 fill = (u16)(' ' | (DEFAULT_ATTRIBUTE << 8));
    vga_fill_words(row, VGA_WIDTH, fill);

    if (cursor_y > 0) {
        cursor_y--;
    }
    update_cursor(cursor_y, cursor_x);
}


/**
 * @brief Move the cursor one column to the left.
 */
static void cursor_left()
{
    cursor_x--;
    update_cursor(cursor_y, cursor_x);
}

/**
 * @brief Move the cursor one column to the right.
 */
static void cursor_right()
{
    cursor_x++;
    update_cursor(cursor_y, cursor_x);
}

/**
 * @brief Move the cursor one row up.
 */
static void cursor_up()
{
    cursor_y--;
    update_cursor(cursor_y, cursor_x);
}

/**
 * @brief Move the cursor one row down.
 */
static void cursor_down()
{
    cursor_y++;
    update_cursor(cursor_y, cursor_x);
}

// VGA-specific ANSI callback implementations
static void vga_cursor_up_cb(int n)
{
    for (int i = 0; i < n; i++) {
        cursor_up();
    }
}

static void vga_cursor_down_cb(int n)
{
    for (int i = 0; i < n; i++) {
        cursor_down();
    }
}

static void vga_cursor_left_cb(int n)
{
    for (int i = 0; i < n; i++) {
        cursor_left();
    }
}

static void vga_cursor_right_cb(int n)
{
    for (int i = 0; i < n; i++) {
        cursor_right();
    }
}

static void vga_cursor_set_position_cb(int row, int col)
{
    // Params are 1-indexed, need defaults
    if (row < 1)
        row = 1;
    if (col < 1)
        col = 1;
    row--;
    col--;

    // Clamp to valid range
    if (row < 0)
        row = 0;
    if (row >= VGA_HEIGHT)
        row = VGA_HEIGHT - 1;
    if (col < 0)
        col = 0;
    if (col >= VGA_WIDTH)
        col = VGA_WIDTH - 1;

    cursor_x = col;
    cursor_y = row;
    update_cursor(row, col);
}

static void vga_clear_screen_cb(int mode)
{
    switch (mode) {
    case 2:
        terminal_clear();
        break;
    default:
        // Not implemented
        break;
    }
}

static void vga_clear_line_cb(int mode)
{
    switch (mode) {
    case 0: // Clear from cursor to end of line
        for (int x = cursor_x; x < VGA_WIDTH; x++) {
            vga_buffer_write(' ', attribute, x, cursor_y);
        }
        break;
    case 1: // Clear from start of line to cursor
        for (int x = 0; x <= cursor_x; x++) {
            vga_buffer_write(' ', attribute, x, cursor_y);
        }
        break;
    case 2: // Clear entire line
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer_write(' ', attribute, x, cursor_y);
        }
        break;
    default:
        break;
    }
}

static void vga_set_graphics_mode_cb(int count, const int *params)
{
    static bool bold      = false;
    static int blinking   = 0;
    static int fg_index   = 7;
    static bool fg_bright = false;
    static int bg_index   = 0;

    for (int i = 0; i < count; i++) {
        const int p = params[i];
        switch (p) {
        case 0:
            bold = false;
            blinking  = 0;
            fg_index  = 7;
            fg_bright = false;
            bg_index  = 0;
            break;
        case 1:
            bold = true;
            break;
        case 5:
            blinking = 1;
            break;
        case 22:
            bold = false;
            break;
        case 25:
            blinking = 0;
            break;
        case 39:
            fg_index = 7;
            fg_bright = false;
            break;
        case 49:
            bg_index = 0;
            break;
        default:
            if (p >= 30 && p <= 37) {
                fg_index  = p - 30;
                fg_bright = false;
            } else if (p >= 90 && p <= 97) {
                fg_index  = p - 90;
                fg_bright = true;
            } else if (p >= 40 && p <= 47) {
                bg_index = p - 40;
            } else if (p >= 100 && p <= 107) {
                bg_index = p - 100;
            }
            break;
        }
    }

    int forecolor = ansi_to_vga_foreground[fg_index & 7];
    int backcolor = ansi_to_vga_foreground[bg_index & 7];
    if (bold || fg_bright) {
        forecolor |= 0x08;
    }
    attribute = ((blinking & 1) << 7) | ((backcolor & 0x07) << 4) | (forecolor & 0x0F);
}

static void vga_report_cursor_position_cb(void)
{
    // Report cursor position (1-indexed)
    char resp[32];
    int len = snprintf(resp, sizeof(resp), "\x1b[%d;%dR", cursor_y + 1, cursor_x + 1);
    if (len > 0 && len < 32) {
        resp[len] = '\0';
        console_queue_input_locked(resp);
    }
}


/**
 * @brief Clear the entire text terminal and reset the cursor.
 */
void terminal_clear()
{
    cursor_x = 0;
    cursor_y = 0;
    const u16 fill = (u16)(' ' | (attribute << 8));
    vga_fill_words((u16 *)VIDEO_MEMORY, VGA_WIDTH * VGA_HEIGHT, fill);

    enable_cursor();
}

/** @brief Put a character on the CGA screen */
void vga_putc(int c)
{
    switch (c) {
    case BACKSPACE: // Backspace
        if (cursor_x > 0) {
            cursor_x--;
            if (cursor_x == 0 && cursor_y > 0) {
                cursor_x = VGA_WIDTH - 1;
                cursor_y--;
            }
            vga_buffer_write(' ', attribute, cursor_x, cursor_y);
        }
        break;
    case KEY_LF: // Left arrow
        if (cursor_x > 0) {
            cursor_x--;
        }
        break;
    case KEY_RT: // Right arrow
        if (cursor_x < VGA_WIDTH - 1) {
            cursor_x++;
        }
        break;
    case KEY_DEL: // Delete
        vga_buffer_write(' ', attribute, cursor_x, cursor_y);
        break;
    case '\n': // Newline
        cursor_y++;
        cursor_x = 0;
        break;
    case '\r':
        cursor_x = 0;
        break;
    case '\t': // Tab
        cursor_x += 4;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
        break;
    case '\b':
        cursor_left();
        vga_buffer_write(' ', attribute, cursor_x, cursor_y);
        break;
    default:
        vga_buffer_write(c, attribute, -1, -1);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    // Scroll if needed (but only if OPOST is enabled - don't scroll during kilo refresh)
    if (cursor_y >= VGA_HEIGHT) {
        if (console_should_auto_scroll()) {
            scroll_screen();
            cursor_y = VGA_HEIGHT - 1;
        } else {
            // In raw mode, just clamp the cursor
            if (cursor_y >= VGA_HEIGHT) {
                cursor_y = VGA_HEIGHT - 1;
            }
        }
    }

    update_cursor(cursor_y, cursor_x);
}

static struct ansi_callbacks vga_callbacks = {
    .cursor_up = vga_cursor_up_cb,
    .cursor_down = vga_cursor_down_cb,
    .cursor_left = vga_cursor_left_cb,
    .cursor_right = vga_cursor_right_cb,
    .cursor_set_position = vga_cursor_set_position_cb,
    .clear_screen = vga_clear_screen_cb,
    .clear_line = vga_clear_line_cb,
    .set_graphics_mode = vga_set_graphics_mode_cb,
    .show_cursor = nullptr, // VGA cursor always visible
    .hide_cursor = nullptr, // VGA doesn't support hiding
    .report_cursor_position = vga_report_cursor_position_cb,
};

void vga_terminal_init(void)
{
    ansi_set_callbacks(&vga_callbacks);

    terminal_clear();
    enable_cursor();
}

#endif // GRAPHICS
