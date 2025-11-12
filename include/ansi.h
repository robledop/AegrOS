#pragma once
#include "types.h"

// Callback interface for terminal-specific actions
struct ansi_callbacks {
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

// Initialize the ANSI parser with terminal-specific callbacks
void ansi_set_callbacks(struct ansi_callbacks *cb);

// Reset parser state
void ansi_reset_state(void);

// Process a character, returns true if it was part of an escape sequence
bool ansi_handle_escape(int c);


