#include "types.h"
#include "string.h"
#include "console.h"

// ANSI escape sequence parser state
static bool ansi_escaping   = false;
static bool ansi_inside     = false;
static bool ansi_private    = false;
static int ansi_params[10]  = {0};
static int ansi_param_count = 1;

// Callback interface for terminal-specific actions
struct ansi_callbacks
{
    void (*cursor_up)(int n);
    void (*cursor_down)(int n);
    void (*cursor_left)(int n);
    void (*cursor_right)(int n);
    void (*cursor_set_position)(int row, int col);
    void (*clear_screen)(int mode);
    void (*clear_line)(int mode);
    void (*set_graphics_mode)(int count, const int *params);
    void (*show_cursor)(void);
    void (*hide_cursor)(void);
    void (*report_cursor_position)(void);
};

static struct ansi_callbacks *callbacks = nullptr;

void ansi_set_callbacks(struct ansi_callbacks *cb)
{
    callbacks = cb;
}

void ansi_reset_state(void)
{
    ansi_escaping = false;
    ansi_inside   = false;
    ansi_private  = false;
    memset(ansi_params, 0, sizeof(ansi_params));
    ansi_param_count = 1;
}

static inline int ansi_param_or_default(int index, int def)
{
    if (index < ansi_param_count && ansi_params[index] != 0) {
        return ansi_params[index];
    }
    return def;
}

__attribute__((target("avx,sse2")))
static bool ansi_process_command(int c)
{
    if (c >= '0' && c <= '9') {
        ansi_params[ansi_param_count - 1] = ansi_params[ansi_param_count - 1] * 10 + (c - '0');
        return false;
    }

    if (c == ';') {
        ansi_param_count++;
        return false;
    }

    if (c == '?') {
        ansi_private = true;
        return false;
    }

    if (callbacks == nullptr) {
        return true;
    }

    switch (c) {
    case 'A': // Cursor up
        if (callbacks->cursor_up) {
            callbacks->cursor_up(ansi_param_or_default(0, 1));
        }
        break;
    case 'B': // Cursor down
        if (callbacks->cursor_down) {
            callbacks->cursor_down(ansi_param_or_default(0, 1));
        }
        break;
    case 'C': // Cursor forward (right)
        if (callbacks->cursor_right) {
            callbacks->cursor_right(ansi_param_or_default(0, 1));
        }
        break;
    case 'D': // Cursor back (left)
        if (callbacks->cursor_left) {
            callbacks->cursor_left(ansi_param_or_default(0, 1));
        }
        break;
    case 'H':
    case 'f': // Cursor position
        if (callbacks->cursor_set_position) {
            callbacks->cursor_set_position(
                ansi_param_or_default(0, 1),
                ansi_param_or_default(1, 1)
                );
        }
        break;
    case 'J': // Clear screen
        if (callbacks->clear_screen) {
            callbacks->clear_screen(ansi_param_or_default(0, 0));
        }
        break;
    case 'K': // Clear line
        if (callbacks->clear_line) {
            callbacks->clear_line(ansi_param_or_default(0, 0));
        }
        break;
    case 'm': // Graphics mode (colors)
        if (callbacks->set_graphics_mode) {
            callbacks->set_graphics_mode(ansi_param_count, ansi_params);
        }
        break;
    case 'h': // Set mode
        if (ansi_private && ansi_param_or_default(0, 0) == 25) {
            if (callbacks->show_cursor) {
                callbacks->show_cursor();
            }
        }
        break;
    case 'l': // Reset mode
        if (ansi_private && ansi_param_or_default(0, 0) == 25) {
            if (callbacks->hide_cursor) {
                callbacks->hide_cursor();
            }
        }
        break;
    case 'n': // Device status report
        if (!ansi_private && ansi_param_or_default(0, 0) == 6) {
            if (callbacks->report_cursor_position) {
                callbacks->report_cursor_position();
            }
        }
        break;
    default:
        // Unknown command - ignore
        break;
    }

    ansi_private = false;
    return true;
}

bool ansi_handle_escape(int c)
{
    if (c == 0x1B) {
        ansi_reset_state();
        ansi_escaping = true;
        return true;
    }

    if (ansi_escaping && c == '[') {
        ansi_reset_state();
        ansi_escaping = true;
        ansi_inside   = true;
        return true;
    }

    if (ansi_escaping && ansi_inside) {
        if (ansi_process_command(c)) {
            ansi_reset_state();
        }
        return true;
    }

    return false;
}


