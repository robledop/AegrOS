#pragma once

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Detect whether the processor exposes a Local APIC.
 */
bool lapic_available(void);

/**
 * @brief Map and enable the Local APIC in preparation for timer use.
 */
void lapic_init(void);

/**
 * @brief Measure the Local APIC timer tick rate using the legacy PIT as a reference.
 *
 * @param hz Target timer frequency (currently informational).
 */
void lapic_timer_calibrate(uint32_t hz);

/**
 * @brief Configure the Local APIC timer to fire periodically at the requested rate.
 *
 * @param hz Desired interrupt frequency in Hertz.
 */
void lapic_timer_set_periodic(uint32_t hz);

/**
 * @brief Signal end-of-interrupt to the Local APIC.
 */
void lapic_eoi(void);

