#pragma once

#include <stdbool.h>
#include <stdint.h>

struct spinlock;

extern struct spinlock ps2_controller_lock;

struct ps2_diag_state {
    uint32_t sequence;
    uint8_t source;
    uint8_t status;
    uint8_t data;
    uint8_t info;
};

extern volatile struct ps2_diag_state ps2_diag_state_storage;

void ps2_controller_init_once(void);
void ps2_diag_update(uint8_t source, uint8_t status, uint8_t data, uint8_t info);
void ps2_diag_snapshot(struct ps2_diag_state *out);
