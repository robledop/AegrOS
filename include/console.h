#pragma once
#include "spinlock.h"
#include "termios.h"
#include "sys/ioctl.h"

struct console_lock
{
    struct spinlock lock;
    int locking;
};

int console_tcgetattr(struct termios *out);
int console_tcsetattr(int action, const struct termios *t);
void console_get_winsize(struct winsize *ws);
void console_queue_input(const char *seq);
void console_queue_input_locked(const char *seq);
int console_should_auto_scroll(void);
