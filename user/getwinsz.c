#include "user.h"
#include "sys/ioctl.h"
int main(void) {
    struct winsize ws;
    if (ioctl(0, TIOCGWINSZ, &ws) < 0) {
        printf("ioctl failed\n");
        exit();
    }
    printf("rows=%d cols=%d\n", ws.ws_row, ws.ws_col);
    exit();
}
