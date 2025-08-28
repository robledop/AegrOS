#include <config.h>
#include <errno.h>
#include <os.h>
#include <status.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(const int argc, char **argv)
{
    char current_directory[MAX_PATH_LENGTH];
    const char *current_dir = getcwd();
    strncpy(current_directory, current_dir, MAX_PATH_LENGTH);

    if (argc != 2) {
        printf("\nUsage: cat <file>");
        return -EINVARG;
    }

    char full_path[MAX_PATH_LENGTH];
    char file[MAX_PATH_LENGTH];
    strncpy(file, argv[1], MAX_PATH_LENGTH);

    int fd = 0;

    if (starts_with("/", file)) {
        fd = open(file, O_RDONLY);
    } else {
        strncpy(full_path, current_directory, MAX_PATH_LENGTH);
        strcat(full_path, file);
        fd = open(full_path, O_RDONLY);
    }

    if (fd <= 0) {
        printf("\nFailed to open file: %s", full_path);
        errno = fd;
        perror("Error: ");
        return fd;
    }

    struct stat s;
    int res = fstat(fd, &s);
    if (res < 0) {
        printf("\nFailed to get file stat. File: %s", full_path);
        errno = res;
        perror("Error: ");
        return res;
    }

    char *buffer = malloc(s.st_size + 1);
    res          = read(fd, (void *)buffer, s.st_size);
    if (res < 0) {
        printf("\nFailed to read file: %s", full_path);
        errno = res;
        perror("Error: ");
        return res;
    }
    buffer[s.st_size] = 0x00;
    putchar('\n');

    write(1, buffer, s.st_size);

    // printf(KCYN "\n%s", buffer);


    close(fd);

    return 0;
}