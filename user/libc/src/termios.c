#include <sys/ioctl.h>
#include <termios.h>

int tcsetattr(int file_descriptor, int optional_actions, const struct termios *termios_p)
{
    int request;

    switch (optional_actions) {
    case TCSANOW:
        request = TCSETS;
        break;
    case TCSADRAIN:
        request = TCSETSW;
        break;
    case TCSAFLUSH:
        request = TCSETSF;
        break;
    default:
        return -1; // Invalid optional_actions
    }

    return ioctl(file_descriptor, request, termios_p);
}

int tcgetattr(int file_descriptor, struct termios *termios_p)
{
    return ioctl(file_descriptor, TCGETS, termios_p);
}