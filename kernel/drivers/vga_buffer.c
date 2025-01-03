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

void enable_cursor(const uint8_t cursor_start, const uint8_t cursor_end)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);

    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void update_cursor(const int row, const int col)
{
    const uint16_t position = (row * VGA_WIDTH) + col;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (position & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (position >> 8) & 0xFF);
}

void vga_buffer_write(const char c, const uint8_t attribute, const int x, const int y)
{
    const int pos_x = x == -1 ? cursor_x : x;
    const int pos_y = y == -1 ? cursor_y : y;

    volatile uint16_t *where = (volatile uint16_t *)VIDEO_MEMORY + (pos_y * VGA_WIDTH + pos_x);
    *where                   = c | (attribute << 8);
}

static void cursor_left()
{
    cursor_x--;
    update_cursor(cursor_y, cursor_x);
}

static void cursor_right()
{
    cursor_x++;
    update_cursor(cursor_y, cursor_x);
}

static void cursor_up()
{
    cursor_y--;
    update_cursor(cursor_y, cursor_x);
}

static void cursor_down()
{
    cursor_y++;
    update_cursor(cursor_y, cursor_x);
}

uint16_t get_cursor_position(void)
{
    uint16_t pos = 0;
    outb(0x3D4, 0x0F);
    pos |= inb(0x3D5);
    outb(0x3D4, 0x0E);
    pos |= ((uint16_t)inb(0x3D5)) << 8;
    return pos;
}

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

void print(const char str[static 1])
{
    const size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        vga_putchar(str[i]);
    }
}

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

void vga_buffer_init()
{
    cursor_x = 0;
    cursor_y = 0;
    initlock(&vga_lock, "vga_buffer");
    terminal_clear();
}

void ansi_reset()
{
    param_escaping = false;
    param_inside   = false;

    param_inside = 0;
    memset(params, 0, sizeof(params));
    param_count = 1;
}

/// @brief Process parameters of an ANSI escape sequence
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

/// @brief Check if the character is part of an ANSI escape sequence and process its parameters
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

void putchar(const char c)
{
    if (handle_ansi_escape(c)) {
        return;
    }
    vga_putchar(c);
}