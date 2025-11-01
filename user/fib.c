#include "user.h"
#include "types.h"

// Fibonacci recursive
u64 fib(u64 n)
{
    if (n <= 1) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

int main(const int argc, char **argv)
{
    if (argc < 2) {
        printf("Usage: fib <number>\n");
        return -1;
    }

    int number = atoi(argv[1]);
    printf("Calculating fibonacci of %d using recursion.\n", number);
    const u64 res = fib((u64)number);
    printf("Result: %llu\n", res);

    // u64 a = 0, b = 1, c, i, n = 100;
    // for (i = 0; i < n; i++) {
    //     if (i <= 1) {
    //         c = i;
    //     } else {
    //         c = a + b;
    //         a = b;
    //         b = c;
    //     }
    //     printf("%d ", c);
    // }

    return 0;
}
