#include <user.h>
#include <wm/video_context.h>
#include <wm/desktop.h>
#include <wm/bmp.h>
#include <mman.h>
#include <fcntl.h>
#include <types.h>
#include <printf.h>

#define FB_WIDTH 1024
#define FB_HEIGHT 768
#define FB_BYTES_PER_PIXEL 4
#define FB_PITCH (FB_WIDTH * FB_BYTES_PER_PIXEL)

volatile u32 *fb;
static desktop_t *desktop;

int main(const int argc, char **argv)
{
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        printf("wm: unable to open /dev/fb0\n");
        exit();
    }

    void *map = mmap(nullptr, FB_PITCH * FB_HEIGHT, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (map == MAP_FAILED) {
        printf("wm: mmap failed\n");
        close(fd);
        exit();
    }
    fb = map;
    close(fd);

    u32 *pixels = nullptr;
    bitmap_load_argb("wpaper.bmp", &pixels);
    video_context_t *context = context_new(1024, 768);
    desktop                  = desktop_new(context, pixels);


    window_paint((window_t *)desktop, nullptr, 1);
    return 0;
}