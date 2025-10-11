#pragma once

int tty_init(void);
void tty_input_buffer_put(char c);
void tty_process_pending_wakeup_locked(void);
