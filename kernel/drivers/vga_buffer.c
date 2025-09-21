/**
 * @file vga_buffer.c
 * @brief Text-mode VGA terminal driver providing character rendering and ANSI handling.
 */

#include <config.h>
#include <io.h>
#include <memory.h>
#include <printf.h>
#include <spinlock.h>
#include <string.h>
#include <vga_buffer.h>

#define VIDEO_MEMORY 0xB8000
#define DEFAULT_ATTRIBUTE 0x07 // Light grey on black background
#define BYTES_PER_CHAR 2       // 1 byte for character, 1 byte for attribute (color)
#define SCREEN_SIZE (VGA_WIDTH * VGA_HEIGHT * BYTES_PER_CHAR)
#define ROW_SIZE (VGA_WIDTH * BYTES_PER_CHAR)
uint8_t attribute = DEFAULT_ATTRIBUTE;
static int cursor_y;
static int cursor_x;

bool param_escaping = false;
bool param_inside   = false;
int params[10]      = {0};
int param_count     = 1;

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

struct spinlock vga_lock;

/**
 * @brief Configure the hardware text-mode cursor scanline range.
 *
 * @param cursor_start Upper scanline of the cursor (0-15).
 * @param cursor_end Lower scanline of the cursor (0-15, must be >= cursor_start).
 */
void enable_cursor(const uint8_t cursor_start, const uint8_t cursor_end)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);

    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

/**
 * @brief Move the hardware cursor to the given position.
 *
 * @param row Zero-based row index within the VGA text buffer.
 * @param col Zero-based column index within the VGA text buffer.
 */
void update_cursor(const int row, const int col)
{
    const uint16_t position = (row * VGA_WIDTH) + col;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (position >> 8) & 0xFF);
}

/**
 * @brief Write a character cell to video memory.
 *
 * @param c ASCII character to render.
 * @param attribute VGA attribute byte controlling foreground and background colors.
 * @param x Column position or -1 to use the current cursor column.
 * @param y Row position or -1 to use the current cursor row.
 */
void vga_buffer_write(const char c, const uint8_t attribute, const int x, const int y)
{
    const int pos_x = x == -1 ? cursor_x : x;
    const int pos_y = y == -1 ? cursor_y : y;

    volatile uint16_t *where = (volatile uint16_t *)VIDEO_MEMORY + (pos_y * VGA_WIDTH + pos_x);
    *where                   = c | (attribute << 8);
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

/**
 * @brief Read the current cursor position from the hardware registers.
 *
 * @return Absolute cursor position in characters from the top-left corner.
 */
uint16_t get_cursor_position(void)
{
    uint16_t pos = 0;
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);
    outb(0x3D4, 0x0E);
    pos |= ((uint16_t)inb(0x3D5)) << 8;
    return pos;
}

/**
 * @brief Scroll the visible text buffer up by one row and clear the final line.
 */
void scroll_screen()
{
    auto const video_memory = (uint8_t *)VIDEO_MEMORY;

    // Move all rows up by one
    memmove(video_memory, video_memory + ROW_SIZE, SCREEN_SIZE - ROW_SIZE);

    // Clear the last line (fill with spaces and default attribute)
    for (size_t i = SCREEN_SIZE - ROW_SIZE; i < SCREEN_SIZE; i += BYTES_PER_CHAR) {
        video_memory[i]     = ' ';
        video_memory[i + 1] = DEFAULT_ATTRIBUTE;
    }

    if (cursor_y > 0) {
        cursor_y--;
    }
    update_cursor(cursor_y, cursor_x);
}

/**
 * @brief Render a single character while handling control and scrolling logic.
 *
 * @param c Character to render.
 */
void vga_putchar(const char c)
{
    switch (c) {
    case 0x08: // Backspace
        if (cursor_x > 0) {
            cursor_x--;
            if (cursor_x == 0 && cursor_y > 0) {
                cursor_x = VGA_WIDTH - 1;
                cursor_y--;
            }
            vga_buffer_write(' ', attribute, cursor_x, cursor_y);
        }
        break;
    case '\n': // Newline
    case '\r':
        cursor_x = 0;
        cursor_y++;
        break;
    case '\t': // Tab
        cursor_x += 4;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
        break;
    default:
        vga_buffer_write(c, attribute, -1, -1);
        cursor_x++;
        if (cursor_x >= VGA_WIDTH) {
            cursor_x = 0;
            cursor_y++;
        }
    }

    // Scroll if needed
    if (cursor_y >= VGA_HEIGHT) {
        scroll_screen();
        cursor_y = VGA_HEIGHT - 1;
    }

    update_cursor(cursor_y, cursor_x);
}

/**
 * @brief Print a null-terminated string using the VGA text terminal.
 *
 * @param str Pointer to a valid C string.
 */
void print(const char str[static 1])
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        vga_putchar(str[i]);
    }
}

/**
 * @brief Clear the entire text terminal and reset the cursor.
 */
void terminal_clear()
{
    cursor_x = 0;
    cursor_y = 0;
    for (int y = 0; y < VGA_HEIGHT; y++) {
        for (int x = 0; x < VGA_WIDTH; x++) {
            vga_buffer_write(' ', attribute, x, y);
        }
    }

    enable_cursor(14, 15);
}

/**
 * @brief Initialize the VGA text-mode buffer and supporting synchronization.
 */
void vga_buffer_init()
{
    cursor_x = 0;
    cursor_y = 0;
    initlock(&vga_lock, "vga_buffer");
    terminal_clear();
}

/**
 * @brief Reset ANSI escape sequence parsing state.
 */
void ansi_reset()
{
    param_escaping = false;
    param_inside   = false;

    param_inside = 0;
    memset(params, 0, sizeof(params));
    param_count = 1;
}

/**
 * @brief Process parameters of an ANSI escape sequence.
 *
 * @param c Current character inside the escape sequence.
 * @return true when the escape sequence is complete, false otherwise.
 */
bool param_process(const int c)
{
    if (c >= '0' && c <= '9') {
        params[param_count - 1] = params[param_count - 1] * 10 + (c - '0');

        return false;
    }

    if (c == ';') {
        param_count++;

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
        const int row = params[0];
        const int col = params[1];
        update_cursor(row, col);
        break;
    case 'J':
        switch (params[0]) {
        case 2:
            terminal_clear();
            break;
        default:
            // Not implemented
            break;
        }
        break;
    case 'm':
        static bool bold    = false;
        static int blinking = 0;

        for (int i = 0; i < param_count; i++) {
            switch (params[i]) {
            case 0:
                attribute = DEFAULT_ATTRIBUTE;
                blinking  = 0;
                bold      = false;
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
            default:
                if (params[i] >= 30 && params[i] <= 47) {
                    int forecolor = 0x07;
                    int backcolor = 0x00;
                    if (params[i] >= 30 && params[i] <= 37) {
                        const int color_index = params[i] - 30;
                        forecolor             = ansi_to_vga_foreground[color_index];
                        if (bold) {
                            forecolor |= 0x08; // Set intensity bit for bold text
                        }
                    } else if (params[i] >= 40 && params[i] <= 47) {
                        const int color_index = params[i] - 40;
                        backcolor             = ansi_to_vga_foreground[color_index]; // Use the same mapping
                    }

                    attribute = ((blinking & 1) << 7) | ((backcolor & 0x07) << 4) | (forecolor & 0x0F);
                }
            }
        }
        break;

    default:
        // Not implemented
    }

    return true;
}

/**
 * @brief Detect and interpret ANSI escape sequences.
 *
 * @param c Current incoming character.
 * @return true if the character was consumed as part of an escape sequence.
 */
bool handle_ansi_escape(const int c)
{
    if (c == 0x1B) {
        ansi_reset();
        param_escaping = true;
        return true;
    }

    if (param_escaping && c == '[') {
        ansi_reset();
        param_escaping = true;
        param_inside   = true;
        return true;
    }

    if (param_escaping && param_inside) {
        if (param_process(c)) {
            ansi_reset();
        }
        return true;
    }

    return false;
}

#ifndef PIXEL_RENDERING // If this is defined, then we use the putchar defined in vesa_terminal.c
/**
 * @brief Terminal-compatible putchar implementation with ANSI support.
 *
 * @param c Character to write.
 */
void putchar(const char c)
{
    if (handle_ansi_escape(c)) {
        return;
    }
    vga_putchar(c);
}
#endif
