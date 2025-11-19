#include <user.h>
#include <wm/video_context.h>
#include <wm/desktop.h>
#include <wm/window.h>
#include <wm/bmp.h>
#include <mman.h>
#include <fcntl.h>
#include <types.h>
#include <printf.h>
#include <errno.h>
#include <status.h>
#include "wm/button.h"
#include "wm/icon.h"
#include "wm/vterm.h"
#include <wm/calculator.h>
#include <mouse.h>

#define FB_WIDTH 1024
#define FB_HEIGHT 768
#define FB_BYTES_PER_PIXEL 4
#define FB_PITCH (FB_WIDTH * FB_BYTES_PER_PIXEL)

static u32 *fb;
static desktop_t *desktop;
static struct termios orig_termios;
static int raw_mode        = 0;
static bool wm_should_exit = false;
static int mousefd;
vterm_t *terminal        = {};
calculator_t *calculator = {};

void spawn_calculator([[maybe_unused]] struct button *button, [[maybe_unused]] int x, [[maybe_unused]] int y)
{
    if (calculator) {
        return;
    }

    calculator = calculator_new();
    window_insert_child((window_t *)desktop, (window_t *)calculator);
    window_move((window_t *)calculator, button->window.context->width / 2, button->window.context->height / 2);
}

static void disable_raw_mode(int fd)
{
    /* Don't even check the return value as it's too late. */
    if (raw_mode) {
        tcsetattr(fd, TCSAFLUSH, &orig_termios);
        raw_mode = 0;
    }

    // Clear screen on exit
    write(STDOUT_FILENO, "\x1b[2J", 4);
}

/* Called at exit to avoid remaining in raw mode. */
void wm_at_exit(void)
{
    disable_raw_mode(STDIN_FILENO);
}

/* Raw mode: 1960 magic shit. */
static int enable_raw_mode(int fd)
{
    struct termios raw;

    if (raw_mode) {
        return 0; /* Already enabled. */
    }
    if (!isatty(STDIN_FILENO)) {
        goto fatal;
    }
    atexit(wm_at_exit);
    if (tcgetattr(fd, &orig_termios) == -1) {
        goto fatal;
    }

    raw = orig_termios; /* modify the original mode */
    /* input modes: no break, no CR to NL, no parity check, no strip char,
     * no start/stop output control. */
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    /* output modes - disable post processing */
    raw.c_oflag &= ~(OPOST);
    /* control modes - set 8 bit chars */
    raw.c_cflag |= (CS8);
    /* local modes - echoing off, canonical off, no extended functions,
     * no signal chars (^Z,^C) */
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    /* control chars - set return condition: min number of bytes and timer. */
    raw.c_cc[VMIN]  = 0; /* Return each byte, or zero for timeout. */
    raw.c_cc[VTIME] = 1; /* 100 ms timeout (unit is tens of second). */

    raw.c_iflag &= ~(IXON | ICRNL | BRKINT | INPCK | ISTRIP);

    /* put terminal in raw mode after flushing */
    if (tcsetattr(fd, TCSAFLUSH, &raw) < 0) {
        goto fatal;
    }
    raw_mode = 1;
    return 0;

fatal:
    errno = -ENOTTY;
    return -1;
}

void exit_button_handler([[maybe_unused]] struct button *button, [[maybe_unused]] int x, [[maybe_unused]] int y)
{
    wm_should_exit = true;
}

void doom_button_handler([[maybe_unused]] struct button *button, [[maybe_unused]] int x, [[maybe_unused]] int y)
{
    int pid = fork();
    if (pid == 0) {
        char *args[] = {(char *)"doom", nullptr};
        exec("/bin/doom", args);
        exit();
    }
    if (pid < 0) {
        printf("wm: failed to fork for doom\n");
    } else {
        wait();
        window_paint((window_t *)desktop, nullptr, 1);
    }
}

void wm_process_events(void)
{
    struct ps2_mouse_packet mp;
    int n = read(mousefd, &mp, sizeof(mp));
    if (n == sizeof(mp)) {
        desktop_process_mouse(desktop, mp.x, mp.y, mp.flags);
    }

    char c;
    int nread = read(STDIN_FILENO, &c, 1);
    if (nread == -1) {
        printf("wm: read error\n");
        wm_should_exit = true;
        return;
    }

    if (nread == 0) {
        return;
    }

    // if (c == 'q') {
    //     wm_should_exit = true;
    //     return;
    // }

    terminal->putchar(terminal, c);
}

int main([[maybe_unused]] const int argc, [[maybe_unused]] char **argv)
{
    enable_raw_mode(STDIN_FILENO);

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

    mousefd = open("/dev/mouse", O_RDWR);
    if (mousefd < 0) {
        printf("wm: cannot open /dev/mouse\n");
        exit();
    }

    u32 *pixels = nullptr;
    bitmap_load_argb("wpaper.bmp", &pixels);
    video_context_t *context = context_new(fb, 1024, 768);
    desktop                  = desktop_new(context, pixels);

    button_t *launch_button    = button_new(10, 10, 100, 30);
    launch_button->onmousedown = spawn_calculator;
    window_set_title((window_t *)launch_button, "Calculator");
    window_insert_child((window_t *)desktop, (window_t *)launch_button);

    button_t *doom_button    = button_new(115, 10, 100, 30);
    doom_button->onmousedown = doom_button_handler;
    window_set_title((window_t *)doom_button, "Doom");
    window_insert_child((window_t *)desktop, (window_t *)doom_button);

    button_t *exit_button    = button_new(220, 10, 100, 30);
    exit_button->onmousedown = exit_button_handler;
    window_set_title((window_t *)exit_button, "Exit");
    window_insert_child((window_t *)desktop, (window_t *)exit_button);

    // terminal = vterm_new();
    // window_insert_child((window_t *)desktop, (window_t *)terminal);
    // window_move((window_t *)terminal, 0, 0);

    window_paint((window_t *)desktop, nullptr, 1);

    while (!wm_should_exit) {
        wm_process_events();
    }

    return 0;
}