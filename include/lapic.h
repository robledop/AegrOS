#pragma once

#include <stdint.h>
#include <date.h>

extern volatile uint32_t* lapic;

void cmostime(struct rtcdate* r);
int lapicid(void);
void lapiceoi(void);
void lapicinit(void);
void lapicstartap(uint8_t, uint32_t);
void microdelay(int);
