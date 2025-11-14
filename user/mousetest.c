#include <user.h>
#include "mouse.h"

int main(int argc, char *argv[])
{
    int mousefd = open("/dev/mouse", 0);
    if (mousefd < 0) {
        printf("mousetest: cannot open /dev/mouse\n");
        exit();
    }

    struct ps2_mouse_packet me;
    while (1) {
        int n = read(mousefd, &me, sizeof(me));
        if (n != sizeof(me)) {
            printf("mousetest: read error\n");
            exit();
        }
        if (me.flags & MOUSE_LEFT) {
            printf(" Left button pressed");
        } else if (me.flags & MOUSE_RIGHT) {
            printf(" Right button pressed");
        } else if (me.flags & MOUSE_MIDDLE) {
            printf(" Middle button pressed");
            break;
        } else {
            printf("Mouse event: x=%d, y=%d, flags=0x%x\n", me.x, me.y, me.flags);
        }
    }

    close(mousefd);
    exit();
}