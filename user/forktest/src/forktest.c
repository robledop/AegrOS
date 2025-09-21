#include <stdio.h>
#include <stdlib.h>

int main([[maybe_unused]] const int argc, [[maybe_unused]] char **argv)
{
    memstat();

    const int r = fork();
    if (r < 0) {
        printf("Fork failed\n");
    } else if (r == 0) {
        printf("\nChild\n" KWHT " ");
        // exec("/bin/blank.elf", nullptr);
    } else {
        printf("Parent waiting on pid %d\n", r);
        waitpid(r, nullptr);
        printf(KYEL "Parent\n" KWHT);
    }

    memstat();

    return 0;
}