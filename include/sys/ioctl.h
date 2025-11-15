#pragma once

struct winsize
{
    unsigned short ws_row;
    unsigned short ws_col;
    unsigned short ws_xpixel;
    unsigned short ws_ypixel;
};

#define TIOCGWINSZ 0x5413

#define FB_IOCTL_GET_WIDTH  0x1001
#define FB_IOCTL_GET_HEIGHT 0x1002
#define FB_IOCTL_GET_FBADDR 0x1003
#define FB_IOCTL_GET_PITCH  0x1004
