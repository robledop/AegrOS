#include "user.h"

extern int main(int argc, char **argv);

void c_start(int argc, char **argv)
{
    atexit_init();
    main(argc, argv);
    exit();
}
