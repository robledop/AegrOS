#pragma once

#include "types.h"

#define BRKINT 0x0002 // Signal interrupt on break
#define ICRNL  0x0100 // Map CR to NL on input
#define INPCK  0x0010 // Enable input parity check
#define ISTRIP 0x0020 // Strip 8th bit off chars
#define IXON   0x0400 // Enable XON/XOFF flow control on output

#define OPOST  0x0001 // Enable implementation-defined output processing

#define CS8    0x0030 // 8 bits per byte

#define ECHO   0x0008 // Enable echoing of input characters
#define ICANON 0x0100 // Enable canonical mode
#define IEXTEN 0x0400 // Enable implementation-defined input processing
#define ISIG   0x0001 // Enable signals

#define NCCS 4 // Number of control characters
#define VMIN 0 // Minimum number of bytes for non-canonical read
#define VTIME 1 // Timeout in deciseconds for non-canonical read
#define VINTR 2 // Interrupt character
#define VQUIT 3 // Quit character

#define TCSAFLUSH 0

struct termios
{
    u32 c_iflag;
    u32 c_oflag;
    u32 c_cflag;
    u32 c_lflag;
    u8 c_cc[NCCS];
};