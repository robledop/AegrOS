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
    printf("Calculating fibonacci of 46 using recursion.\n");
    const u64 result = fib(46);
    printf("\nResult: %llu", result);

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
