#include <fpu.h>
#include <memory.h>
#include <stdint.h>
#include <x86.h>

static _Alignas(16) uint8_t initial_fx_state[512];

void fpu_init(void)
{
    uint32_t cr0 = read_cr0();
    cr0 &= ~CR0_EM;
    cr0 |= CR0_MP;
    write_cr0(cr0);

    uint32_t cr4 = read_cr4();
    cr4 |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    write_cr4(cr4);

    fninit();
    fxsave(initial_fx_state);
}

void fpu_save(void *state)
{
    fxsave(state);
}

void fpu_restore(const void *state)
{
    fxrstor(state);
}

void fpu_load_initial_state(void *state)
{
    memcpy(state, initial_fx_state, sizeof initial_fx_state);
}

