#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITERATIONS 10

void at_exit()
{
    printf("Finished\n");
}

int main([[maybe_unused]] const int argc, [[maybe_unused]] char **argv)
{
    atexit(at_exit);

    printf(KBRED "\n##################################\n"
                 "Tests with waitpid()\n"
                 "##################################\n" KWHT);

    printf(KBRED "\ncreate_process tests\n" KRESET);

    for (int i = 0; i < ITERATIONS; i++) {
        char *current_directory = getcwd();
        printf("create_process (echo): %d\n", i);
        const int pid = create_process((char *)"echo lalala", current_directory);
        if (pid < 0) {
            printf("Failed to create process\n");
        } else {
            waitpid(pid, nullptr);
        }
    }

    printf(KBRED "\nfork tests\n" KRESET);

    for (int i = 0; i < ITERATIONS; i++) {
        const int r = fork();
        if (r < 0) {
            printf("Fork failed\n");
        } else if (r == 0) {
            printf("\nChild will exec blank.elf\t" KWHT " ");
            exec("/bin/blank.elf", nullptr);

            printf(KRED "This should not be printed\n" KRESET);
        } else {
            waitpid(r, nullptr);
            printf(KYEL "Parent of %d (pid:%d)\t" KWHT, i, getpid());
        }
    }

    printf(KBRED "\n##################################\n"
                 "Tests without wait()\n"
                 "##################################\n" KWHT);

    printf(KBRED "\ncreate_process tests\n" KRESET);

    for (int i = 0; i < ITERATIONS; i++) {
        const char *current_directory = getcwd();
        printf("\t create_process: %d", i);
        const int pid = create_process((char *)"echo lalala", current_directory);
        if (pid < 0) {
            printf("Failed to create process\n");
        }
    }

    printf(KBRED "\nfork tests\n" KRESET);

    for (int i = 0; i < ITERATIONS; i++) {
        const int r = fork();
        if (r < 0) {
            printf("Fork failed\n");
        } else if (r == 0) {
            printf(KGRN "\tForked child %d (pid:%d)" KWHT, i, getpid());
            exit(0);
        } else {
            // waitpid(r, nullptr);
            printf(KYEL "\tParent of %d (pid:%d)" KWHT, i, getpid());
        }
    }

    printf("\n");

    printf(KBRED "\n##################################\n"
                 "Tests with sleep()\n"
                 "##################################\n" KWHT);

    for (int i = 0; i < ITERATIONS; i++) {
        const char *current_directory = getcwd();
        printf("\t create_process (sleep 1000): %d", i);
        const int pid = create_process((char *)"sleep 1000", current_directory);
        if (pid < 0) {
            printf("Failed to create process\n");
        } else {
            waitpid(pid, nullptr);
        }
    }

    return 0;
}
