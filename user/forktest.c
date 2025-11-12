// Test that fork fails gracefully.
// Tiny executable so that the limit can be filling the proc table.

#include "user.h"

#define N 1000
#define number 40

// Fibonacci recursive
u64 fib(u64 n)
{
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

void forktest(void)
{
    int n;

    printf("fork test\n");
    printf("launching a bunch of processes that calculate the 40th fibonacci number using recursion\n");

    for (n = 0; n < N; n++) {
        int pid = fork();
        if (pid < 0) {
            printf("!");
            break;
        }
        if (pid == 0) {
            printf(".");
            fib((u64)number);
            printf("+");
            exit();
        }
    }

    if (n == N) {
        printf("fork claimed to work %d times!\n", N);
        exit();
    }

    int total = 0;
    for (; n > 0; n--) {
        if (wait() < 0) {
            printf("wait stopped early\n");
            exit();
        }
        total++;
    }

    printf("\nfork worked %d times\n", total);

    if (wait() != -1) {
        printf("wait got too many\n");
        exit();
    }

    printf("fork test OK\n");
}

int main(void)
{
    forktest();
    exit();
}