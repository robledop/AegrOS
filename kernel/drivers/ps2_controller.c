#include <ps2_controller.h>
#include <spinlock.h>
#include <x86.h>

struct spinlock ps2_controller_lock;
static bool ps2_controller_lock_initialized;
volatile struct ps2_diag_state ps2_diag_state_storage;

void ps2_controller_init_once(void)
{
    if (!ps2_controller_lock_initialized) {
        initlock(&ps2_controller_lock, "ps2ctl");
        ps2_controller_lock_initialized = true;
    }
}

void ps2_diag_update(uint8_t source, uint8_t status, uint8_t data, uint8_t info)
{
    pushcli();
    ps2_diag_state_storage.sequence++;
    ps2_diag_state_storage.source = source;
    ps2_diag_state_storage.status = status;
    ps2_diag_state_storage.data   = data;
    ps2_diag_state_storage.info   = info;
    popcli();
}

void ps2_diag_snapshot(struct ps2_diag_state *out)
{
    pushcli();
    *out = ps2_diag_state_storage;
    popcli();
}
