#include "user.h"

#define STACK_CHK_GUARD 0xe2dee396
u32 __stack_chk_guard = STACK_CHK_GUARD; // NOLINT(*-reserved-identifier)

extern int main(int argc, char **argv);

void c_start(int argc, char **argv)
{
    atexit_init();
    main(argc, argv);
    exit();
}
