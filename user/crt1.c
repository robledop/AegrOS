#include "user.h"

atexit_function atexit_functions[32] = {nullptr};

extern int main(int argc, char **argv);

void c_start(int argc, char **argv)
{
    for (int i = 0; i < 32; i++) {
        atexit_functions[i] = nullptr;
    }


    //open("/dev/tty", O_RDONLY); // stdin
    //open("/dev/tty", O_WRONLY); // stdout
    //open("/dev/tty", O_WRONLY); // stderr

    //init_standard_streams();

    main(argc, argv);

    exit();
}