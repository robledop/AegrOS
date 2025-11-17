#include "defs.h"
#include "proc.h"
#include "x86.h"

void fpu_save_state(struct proc *p)
{
    if (p == nullptr || !p->fpu_initialized) {
        return;
    }

    struct cpu *cpu = current_cpu();
    if (cpu->has_xsave) {
        xsave(p->fpu_state, cpu->xsave_features_low, cpu->xsave_features_high);
    } else {
        fxsave(p->fpu_state);
    }
}

void fpu_restore_state(struct proc *p)
{
    if (p == nullptr) {
        return;
    }

    struct cpu *cpu = current_cpu();
    if (!p->fpu_initialized) {
        __asm__ volatile("fninit");
        if (cpu->has_xsave) {
            xsave(p->fpu_state, cpu->xsave_features_low, cpu->xsave_features_high);
        } else {
            fxsave(p->fpu_state);
        }
        p->fpu_initialized = true;
    }

    if (cpu->has_xsave) {
        xrstor(p->fpu_state, cpu->xsave_features_low, cpu->xsave_features_high);
    } else {
        fxrstor(p->fpu_state);
    }
}
