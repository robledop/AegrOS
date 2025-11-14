#pragma once

#include "types.h"

#define BRKINT 0x0002
#define ICRNL  0x0100
#define INPCK  0x0010
#define ISTRIP 0x0020
#define IXON   0x0400

#define OPOST  0x0001

#define CS8    0x0030

#define ECHO   0x0008
#define ICANON 0x0100
#define IEXTEN 0x0400
#define ISIG   0x0001

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