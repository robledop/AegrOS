#include "os.h"
#include <stdlib.h>
#include "string.h"
#include "syscall.h"

struct command_argument *os_parse_command(const char *command, int max) {
    struct command_argument *head = nullptr;
    char scommand[1025];
    if (max >= (int)sizeof(scommand)) {
        return nullptr;
    }

    strncpy(scommand, command, sizeof(scommand));
    const char *token = strtok(scommand, " ");

    if (token == NULL) {
        return head;
    }

    head = malloc(sizeof(struct command_argument));
    if (head == NULL) {
        return nullptr;
    }

    strncpy(head->argument, token, sizeof(head->argument));
    head->next = nullptr;

    struct command_argument *current = head;
    token                            = strtok(nullptr, " ");

    while (token != NULL) {
        struct command_argument *next = malloc(sizeof(struct command_argument));
        if (next == NULL) {
            break;
        }

        strncpy(next->argument, token, sizeof(next->argument));
        next->next    = nullptr;
        current->next = next;
        current       = next;
        token         = strtok(nullptr, " ");
    }

    return head;
}


void os_terminal_readline(unsigned char *out, const int max, const bool output_while_typing) {
    int i = 0;
    for (; i < max - 1; i++) {
        const unsigned char key = getkey();
        if (key == '\n' || key == '\r') {
            break;
        }

        if (key == '\b' && i <= 0) {
            i = -1;
            continue;
        }

        if (output_while_typing) {
            putchar(key);
        }

        if (key == '\b' && i > 0) {
            i -= 2;
            continue;
        }

        out[i] = key;
    }

    out[i] = 0x00;
}

int os_create_process(const char *command, const char *current_directory) {
    char buffer[1024];
    strncpy(buffer, command, sizeof(buffer));
    struct command_argument *root_command_argument = os_parse_command(buffer, sizeof(buffer));
    if (root_command_argument == NULL) {
        return -1;
    }

    if (root_command_argument->current_directory == NULL) {
        root_command_argument->current_directory = malloc(MAX_PATH_LENGTH);
    }

    strncpy(root_command_argument->current_directory, current_directory, MAX_PATH_LENGTH);

    return syscall1(SYSCALL_CREATE_PROCESS, root_command_argument);
}