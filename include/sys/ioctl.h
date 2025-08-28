#pragma once

#define TIOCGWINSZ 0x5413     // Get window size
#define TIOCSWINSZ 0x5414     // Set window size
#define TIOCGPGRP 0x540F      // Get process group
#define TIOCSPGRP 0x5410      // Set process group
#define TIOCGPTN 0x80045430   // Get pty number
#define TIOCSPTLCK 0x40045431 // Set pty lock
#define TIOCGDEV 0x80045432   // Get device number
#define TIOCSIG 0x40045436    // Signal pty
#define TIOCVHANGUP 0x5400    // Hang up
#define TIOCGPKT 0x80045438   // Get packet mode
#define TIOCGPTLCK 0x40045439 // Get pty lock

int ioctl(int file_descriptor, int request, ...);