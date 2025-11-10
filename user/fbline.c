#include "user.h"
#include "fcntl.h"
#include "mman.h"

#define FB_WIDTH 1024
#define FB_HEIGHT 768
#define FB_BYTES_PER_PIXEL 4
#define FB_PITCH (FB_WIDTH * FB_BYTES_PER_PIXEL)

static int plot(volatile u32 *fb, int x, int y, u32 color)
{
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) {
        return 0;
    }

    const int stride = FB_PITCH / FB_BYTES_PER_PIXEL;
    fb[y * stride + x] = color;
    return 0;
}

static void usage(void)
{
    printf("Usage: fbline x1 y1 x2 y2 color\n");
}

static int hex_value(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    return -1;
}

static u32 parse_color(const char *arg)
{
    if (arg[0] == '0' && (arg[1] == 'x' || arg[1] == 'X')) {
        arg += 2;
        u32 value = 0;
        int digit;
        while (*arg) {
            digit = hex_value(*arg++);
            if (digit < 0) {
                break;
            }
            value = (value << 4) | (u32)digit;
        }
        return value;
    }
    return (u32)atoi(arg);
}

int main(int argc, char **argv)
{
    if (argc != 6) {
        usage();
        exit();
    }

    int x1    = atoi(argv[1]);
    int y1    = atoi(argv[2]);
    int x2    = atoi(argv[3]);
    int y2    = atoi(argv[4]);
    u32 color = parse_color(argv[5]);

    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        printf("fbline: unable to open /dev/fb0\n");
        exit();
    }

    void *map = mmap(nullptr, FB_PITCH * FB_HEIGHT, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        printf("fbline: mmap failed\n");
        close(fd);
        exit();
    }
    volatile u32 *fb = map;
    close(fd);

    int dx  = abs(x2 - x1);
    int sx  = x1 < x2 ? 1 : -1;
    int dy  = -abs(y2 - y1);
    int sy  = y1 < y2 ? 1 : -1;
    int err = dx + dy;

    for (;;) {
        plot(fb, x1, y1, color);
        if (x1 == x2 && y1 == y2) {
            break;
        }
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x1 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y1 += sy;
        }
    }

    exit();
}
