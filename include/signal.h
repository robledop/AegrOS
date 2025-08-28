#pragma once

#define SIGWINCH 1

int signal(int signum, void (*handler)(int));