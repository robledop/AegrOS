#pragma once

#include <stddef.h>

void fpu_init(void);
void fpu_save(void *state);
void fpu_restore(const void *state);
void fpu_load_initial_state(void *state);

