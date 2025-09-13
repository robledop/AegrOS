#pragma once

// https://wiki.osdev.org/8259_PIC

void pic_init(void);
void pic_acknowledge(int irq);
void pic_enable_irq(int irq);
void pic_disable_irq(int irq);
int pic_irq_enabled(int irq);