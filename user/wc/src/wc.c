#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

char buf[512];

void wc(int fd, char *name)
{
    int n;
    int w, c;

    int l = w = c = 0;
    int inword    = 0;
    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        printf(".");
        for (int i = 0; i < n; i++) {
            if (buf[i] != '\0') {
                c++;
            }
            if (buf[i] == '\n') {
                l++;
            }
            if (strchr(" \r\t\n\v", buf[i])) {
                inword = 0;
            } else if (!inword) {
                w++;
                inword = 1;
            }
        }
    }
    if (n < 0) {
        printf("wc: read error\n");
        exit(0);
    }
    printf("\nlines: %d\n"
           "words: %d\n"
           "characters: %d\n"
           "%s\n",
           l,
           w,
           c,
           name);
}

int main(int argc, char *argv[])
{
    int fd;

    if (argc <= 1) {
        wc(0, "");
        exit(0);
    }

    for (int i = 1; i < argc; i++) {
        if ((fd = open(argv[i], 0)) < 0) {
            printf("wc: cannot open %s\n", argv[i]);
            exit(0);
        }
        wc(fd, argv[i]);
        close(fd);
    }
    exit(0);
}
