#include <stdio.h>
#include <stdlib.h>

class Echo
{
public:
    static void print(int argc, char **argv)
    {
        putchar('\n');
        for (int i = 1; i < argc; i++) {
            printf("%s ", argv[i]);
        }
    }
};

int main(const int argc, char **argv)
{
    Echo::print(argc, argv);

    return 0;
}