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

#define NCCS 2
#define VMIN 0
#define VTIME 1

#define TCSAFLUSH 0

struct termios
{
    u32 c_iflag;
    u32 c_oflag;
    u32 c_cflag;
    u32 c_lflag;
    u8 c_cc[NCCS];
};
