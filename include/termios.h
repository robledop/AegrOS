#pragma once

#define TCSANOW 0   // Apply changes immediately
#define TCSADRAIN 1 // Apply changes when the output buffer is empty
#define TCSAFLUSH 2 // Apply changes when the output buffer is empty; also flush the input buffer

#define TCSETS 0x5402  // Set terminal attributes
#define TCGETS 0x5401  // Get terminal attributes
#define TCSETSW 0x5403 // Set terminal attributes and wait
#define TCSETSF 0x5404 // Set terminal attributes and flush

#define ICANON 0000002 // Enable canonical mode
#define ECHO 0000010   // Enable echoing of input characters
#define ISIG 0000001   // Enable signals
#define IEXTEN 0100000 // Enable extended functions
#define VMIN 16        // Minimum number of characters for non-canonical read
#define VTIME 17       // Timeout in deciseconds for non-canonical read

#define OPOST 0000001  // Post-process output
#define CS8 0000060    // 8 bits per byte
#define BRKINT 0000002 // Signal interrupt on break
#define ICRNL 0000400  // Map CR to NL on input
#define INPCK 0000020  // Enable input parity check
#define ISTRIP 0000040 // Strip character
#define IXON 0002000   // Enable start/stop output control


struct termios {
    unsigned int c_iflag;   // input mode flags
    unsigned int c_oflag;   // output mode flags
    unsigned int c_cflag;   // control mode flags
    unsigned int c_lflag;   // local mode flags
    unsigned char c_line;   // line discipline
    unsigned char c_cc[19]; // control characters
};

struct winsize {
    unsigned short ws_row;    // rows, in characters
    unsigned short ws_col;    // columns, in characters
    unsigned short ws_xpixel; // horizontal size, pixels
    unsigned short ws_ypixel; // vertical size, pixels
};


int tcsetattr(int file_descriptor, int optional_actions, const struct termios *termios_p);
int tcgetattr(int file_descriptor, struct termios *termios_p);