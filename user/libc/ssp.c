#include <user.h>

// https://wiki.osdev.org/Stack_Smashing_Protector

[[noreturn]] void __stack_chk_fail(void) // NOLINT(*-reserved-identifier)
{
    panic("Stack smashing detected");

    __builtin_unreachable();
}