#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ITERATIONS 100

void at_exit()
{
    printf("Finished\n");
}

int main([[maybe_unused]] const int argc, [[maybe_unused]] char **argv)
{
    atexit(at_exit);
    printf("\n exec\n");

    printf(KBRED "\n##################################\n"
                 "Tests with waitpid()\n"
                 "##################################\n" KWHT);
    printf(KYEL "\nBefore forking, (pid:%d)\n", getpid());

    const int rc = fork();

    if (rc < 0) {
        printf("Fork failed\n");
    } else if (rc == 0) {
        printf(KCYN "Child (pid:%d)\n", getpid());
        printf("Child will exec blank.elf" KWHT " ");
        exec("/bin/blank.elf", nullptr);

        printf("This should not be printed\n");
    } else {
        waitpid(rc, nullptr);
        printf(KYEL "\nAfter forking. Parent of %d (pid:%d)", rc, getpid());
    }

    for (int i = 0; i < ITERATIONS; i++) {
        char *current_directory = getcwd();
        printf("create_process: %d", i);
        const int pid = create_process((char *)"echo lalala", current_directory);
        if (pid < 0) {
        } else {
            waitpid(pid, nullptr);
        }
    }

    for (int i = 0; i < ITERATIONS; i++) {
        const int r = fork();
        if (r < 0) {
            printf("Fork failed\n");
        } else if (r == 0) {
            exit(0);
            printf(KGRN "Forked child %d (pid:%d)\t" KWHT, i, getpid());
        } else {
            wait(nullptr);
            printf(KYEL "Parent of %d (pid:%d)\t" KWHT, i, getpid());
        }
    }

    printf(KBRED "\n##################################\n"
                 "Tests without waitpid()\n"
                 "##################################\n" KWHT);

    const int nowait = fork();

    if (nowait < 0) {
        printf("Fork failed\n");
    } else if (nowait == 0) {
        printf(KCYN "Child (pid:%d)\t", getpid());
        printf("Child will exec blank.elf" KWHT " ");
        exec("/bin/blank.elf", nullptr);

        printf("THIS SHOULD NOT BE PRINTED\n");
    } else {
        // waitpid(rc, nullptr);
        printf(KYEL "\nAfter forking. Parent of %d (pid:%d)", rc, getpid());
    }

    for (int i = 0; i < ITERATIONS; i++) {
        const char *current_directory = getcwd();
        printf("\n create_process: %d", i);
        const int pid = create_process((char *)"echo lalala", current_directory);
        if (pid < 0) {
        } else {
            // waitpid(pid, nullptr);
        }
    }

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

    printf(KBRED "\n##################################\n"
                 "Tests without wait()\n"
                 "##################################\n" KWHT);

    const int waitp = fork();

    if (waitp < 0) {
        printf("Fork failed\n");
    } else if (waitp == 0) {
        printf(KCYN "Child (pid:%d)\t", getpid());
        printf("Child will exec blank.elf" KWHT " ");
        exec("/bin/blank.elf", nullptr);

        printf("THIS SHOULD NOT BE PRINTED\n");
    } else {
        wait(nullptr);
        printf(KYEL "\tAfter forking. Parent of %d (pid:%d)", rc, getpid());
    }

    for (int i = 0; i < ITERATIONS; i++) {
        const char *current_directory = getcwd();
        printf("\t create_process: %d", i);
        const int pid = create_process((char *)"echo lalala", current_directory);
        if (pid < 0) {
        } else {
            wait(nullptr);
        }
    }

    for (int i = 0; i < ITERATIONS; i++) {
        const int r = fork();
        if (r < 0) {
            printf("Fork failed\n");
        } else if (r == 0) {
            printf(KGRN "\tForked child %d (pid:%d)\n" KWHT, i, getpid());
            exit(0);
        } else {
            wait(nullptr);
            printf(KYEL "\tParent of %d (pid:%d)\n" KWHT, i, getpid());
        }
    }

    exit(0);

    return 0;
}
