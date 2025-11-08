// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "string.h"
#include "defs.h"
#include "traps.h"
#include "spinlock.h"
#include "file.h"
#include "memlayout.h"
#include "console.h"
#include "proc.h"
#include "x86.h"
#include "debug.h"
#include "printf.h"
#include "termcolors.h"
#include "io.h"
#include "scheduler.h"

// Special keycodes
#define KEY_HOME        0xE0
#define KEY_END         0xE1
#define KEY_UP          0xE2
#define KEY_DN          0xE3
#define KEY_LF          0xE4
#define KEY_RT          0xE5
#define KEY_PGUP        0xE6
#define KEY_PGDN        0xE7
#define KEY_INS         0xE8
#define KEY_DEL         0xE9

bool handle_ansi_escape(int c);
/** @brief Flag indicating if the system has panicked */
static int panicked = 0;

#define VGA_WIDTH 80

struct console_lock cons;

/** @brief Print an integer in the given base */
static void printint(int xx, int base, int sign)
{
    static char digits[] = "0123456789abcdef";
    char buf[16];
    u32 x;

    if (sign && ((sign = xx < 0)))
        x              = -xx;
    else
        x = xx;

    int i = 0;
    do {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (--i >= 0) {
        consputc(buf[i]);
    }
}

/** @brief Print to the console. Only understands %d, %x, %p, %s. */
void cprintf(char *fmt, ...)
{
    int c;
    char *s;

    const int locking = cons.locking;
    if (locking)
        acquire(&cons.lock);

    if (fmt == nullptr)
        panic("null fmt");

    const int *argp = (int *)(void *)(&fmt + 1);
    for (int i = 0; (c = fmt[i] & 0xff) != 0; i++) {
        if (c != '%') {
            consputc(c);
            continue;
        }
        c = fmt[++i] & 0xff;
        if (c == 0)
            break;
        switch (c) {
        case 'd':
            printint(*argp++, 10, 1);
            break;
        case 'x':
        case 'p':
            printint(*argp++, 16, 0);
            break;
        case 's':
            if ((s = (char *)*argp++) == nullptr)
                s  = "(null)";
            for (; *s; s++)
                consputc(*s);
            break;
        case '%':
            consputc('%');
            break;
        default:
            // Print an unknown% sequence to draw attention.
            consputc('%');
            consputc(c);
            break;
        }
    }

    if (locking)
        release(&cons.lock);
}

/** @brief Panic and print the message */
void panic(const char *fmt, ...)
{
    cli();
    cons.locking = 0;
    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("lapicid %d: panic: " KRED "%s\n" KRESET, lapicid(), buf);

    debug_stats();
    panicked = 1; // freeze other CPU
    for (;;) {
        hlt();
    }
}

#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VIDEO_MEMORY P2V(0xB8000)
#define DEFAULT_ATTRIBUTE 0x07 // Light grey on black background
#define BYTES_PER_CHAR 2       // 1 byte for character, 1 byte for attribute (color)
#define SCREEN_SIZE (VGA_WIDTH * VGA_HEIGHT * BYTES_PER_CHAR)
#define ROW_SIZE (VGA_WIDTH * BYTES_PER_CHAR)
u8 attribute = DEFAULT_ATTRIBUTE;
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

#define BACKSPACE 0x100
#define CRTPORT 0x3d4

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
 * @brief Scroll the visible text buffer up by one row and clear the final line.
 */
void scroll_screen()
{
    auto const video_memory = (u8 *)VIDEO_MEMORY;

    // Move all rows up by one
    memmove(video_memory, video_memory + ROW_SIZE, SCREEN_SIZE - ROW_SIZE);

    // Clear the last line (fill with spaces and default attribute)
    for (u32 i = SCREEN_SIZE - ROW_SIZE; i < SCREEN_SIZE; i += BYTES_PER_CHAR) {
        video_memory[i]     = ' ';
        video_memory[i + 1] = DEFAULT_ATTRIBUTE;
    }

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


/**
 * @brief Write a character cell to video memory.
 *
 * @param c ASCII character to render.
 * @param attribute VGA attribute byte controlling foreground and background colors.
 * @param x Column position or -1 to use the current cursor column.
 * @param y Row position or -1 to use the current cursor row.
 */
void vga_buffer_write(const char c, const u8 attribute, const int x, const int y)
{
    const int pos_x = x == -1 ? cursor_x : x;
    const int pos_y = y == -1 ? cursor_y : y;

    volatile u16 *where = (volatile u16 *)VIDEO_MEMORY + (pos_y * VGA_WIDTH + pos_x);
    *where              = c | (attribute << 8);
}

/** @brief Put a character on the CGA screen */
static void cgaputc(int c)
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

    // Scroll if needed
    if (cursor_y >= VGA_HEIGHT) {
        scroll_screen();
        cursor_y = VGA_HEIGHT - 1;
    }

    update_cursor(cursor_y, cursor_x);
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

    enable_cursor();
}

/** @brief Put a character on the console (screen and serial) */
void consputc(int c)
{

    if (panicked) {
        cli();
        // ReSharper disable once CppDFAEndlessLoop
        for (;;);
    }

    if (c == BACKSPACE) {
        uartputc('\b');
        uartputc(' ');
        uartputc('\b');
    } else {
        uartputc(c);
    }

    if (handle_ansi_escape(c)) {
        return;
    }
    cgaputc(c);
}


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
        static bool bold = false;
        static int blinking = 0;

        for (int i = 0; i < param_count; i++) {
            switch (params[i]) {
            case 0:
                attribute = DEFAULT_ATTRIBUTE;
                blinking = 0;
                bold     = false;
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


#define INPUT_BUF 128

/** @brief Input buffer for console */
struct
{
    char buf[INPUT_BUF];
    u32 r; // Read index
    u32 w; // Write index
    u32 e; // Edit index
} input;

void boot_message(warning_level_t level, const char *fmt, ...)
{
    switch (level) {
    case WARNING_LEVEL_INFO:
        cprintf(KWHT "[ " KBGRN "INFO" KRESET " ] ");
        break;
    case WARNING_LEVEL_WARNING:
        cprintf(KWHT "[ " KYEL "WARNING" KRESET " ] ");
        break;
    case WARNING_LEVEL_ERROR:
        cprintf(KWHT "[ " KRED "ERROR" KRESET " ] ");
        break;
    }

    va_list ap;
    va_start(ap, fmt);
    char buf[512];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    printf("%s\n", buf);
}


#define CTRL(x) ((x) - '@') // Control-x

/** @brief Handle console input */
void consoleintr(int (*getc)(void))
{
    int c, doprocdump = 0;

    acquire(&cons.lock);
    while ((c = getc()) >= 0) {
        switch (c) {
        case CTRL('P'): // Process listing.
            // procdump() locks cons.lock indirectly; invoke later
            doprocdump = 1;
            break;
        case CTRL('U'): // Kill line.
            while (input.e != input.w &&
                input.buf[(input.e - 1) % INPUT_BUF] != '\n') {
                input.e--;
                consputc(BACKSPACE);
            }
            break;
        case CTRL('H'):
        case '\x7f': // Backspace
            if (input.e != input.w) {
                input.e--;
                consputc(BACKSPACE);
            }
            break;
        case 226: // Up arrow
            input.buf[input.e++ % INPUT_BUF] = c;
            input.w = input.e;
            wakeup(&input.r);
            break;
        case 227: // Down arrow
            input.buf[input.e++ % INPUT_BUF] = c;
            input.w = input.e;
            wakeup(&input.r);
            break;
        default:
            if (c != 0 && input.e - input.r < INPUT_BUF) {
                c                                = (c == '\r') ? '\n' : c;
                input.buf[input.e++ % INPUT_BUF] = c;
                consputc(c);
                if (c == '\n' || c == CTRL('D') || input.e == input.r + INPUT_BUF) {
                    input.w = input.e;
                    wakeup(&input.r);
                }
            }
            break;
        }
    }
    release(&cons.lock);
    if (doprocdump) {
        procdump(); // now call procdump() wo. cons.lock held
    }
}

/** @brief Read from the console */
int consoleread(struct inode *ip, char *dst, int n)
{
    ip->iops->iunlock(ip);
    int target = n;
    acquire(&cons.lock);
    while (n > 0) {
        while (input.r == input.w) {
            if (current_process()->killed) {
                release(&cons.lock);
                ip->iops->ilock(ip);
                return -1;
            }
            sleep(&input.r, &cons.lock);
        }
        int c = input.buf[input.r++ % INPUT_BUF];
        if (c == CTRL('D')) {
            // EOF
            if (n < target) {
                // Save ^D for next time to make sure
                // the caller gets a 0-byte result.
                input.r--;
            }
            break;
        }
        *dst++ = c;
        --n;
        if (c == '\n')
            break;
    }
    release(&cons.lock);
    ip->iops->ilock(ip);

    return target - n;
}

/** @brief Write to the console */
int consolewrite(struct inode *ip, char *buf, int n)
{
    ip->iops->iunlock(ip);
    acquire(&cons.lock);
    for (int i = 0; i < n; i++)
        consputc(buf[i] & 0xff);
    release(&cons.lock);
    ip->iops->ilock(ip);

    return n;
}

/** @brief Initialize console */
void consoleinit(void)
{
    initlock(&cons.lock, "console");

    devsw[CONSOLE].write = consolewrite;
    devsw[CONSOLE].read  = consoleread;
    cons.locking         = 1;

    terminal_clear();

    enable_cursor();
    ioapicenable(IRQ_KBD, 0);
}