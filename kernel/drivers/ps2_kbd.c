#include <idt.h>
#include <io.h>
#include <keyboard.h>
#include <mouse.h>
#include <printf.h>
#include <ps2_kbd.h>
#include <spinlock.h>

#include "kernel.h"

struct spinlock keyboard_lock         = {};
struct spinlock keyboard_getchar_lock = {};

int ps2_keyboard_init();
void ps2_keyboard_interrupt_handler(struct interrupt_frame *frame);

#define PS2_CAPSLOCK 0x3A

/**
 * @brief Wait for the PS/2 controller to become ready for read or write.
 *
 * @param a_type 0 to wait for available data, non-zero to wait for write readiness.
 */
static void ps2_wait(unsigned char a_type)
{
    unsigned int timeout = 1000000;
    if (!a_type) {
        // Wait for output buffer to be full (data available)
        while (--timeout) {
            if ((inb(KBD_STATUS_PORT) & 0x01) == 1) {
                return;
            }
        }
    } else {
        // Wait for input buffer to be empty (ready to write)
        while (--timeout) {
            if (!(inb(KBD_STATUS_PORT) & 0x02)) {
                return;
            }
        }
    }
}

/**
 * @brief Wait for the keyboard controller to acknowledge the previous command.
 */
void kbd_ack(void)
{
    unsigned int timeout = 1000000;
    while (timeout--) {
        const uint8_t status = inb(KBD_STATUS_PORT);
        if ((status & KBD_DATA_IN_BUFFER) == 0) {
            continue;
        }

        const uint8_t data = inb(KBD_DATA_PORT);
        if (status & 0x20) {
            panic("Mouse data in keyboard ack");
            continue;
        }

        if (data == 0xFA) {
            return;
        }
    }

    printf("[KBD] Warning: timeout waiting for ACK\n");
}

/**
 * @brief Update the keyboard LED status.
 */
void kbd_led_handling(const unsigned char ledstatus)
{
    outb(0x60, 0xed);
    kbd_ack();
    outb(0x60, ledstatus);
}

// Taken from xv6
/**
 * @brief Fetch a translated character from the PS/2 keyboard.
 *
 * @return ASCII character or 0 when no key is available.
 */
uint8_t keyboard_get_char()
{
    static unsigned int shift;
    static uint8_t *charcode[4] = {normalmap, shiftmap, ctlmap, ctlmap};

    unsigned int data       = 0;
    bool have_keyboard_data = false;

    for (int attempt = 0; attempt < 16; attempt++) {
        const uint8_t status = inb(KBD_STATUS_PORT);
        if ((status & KBD_DATA_IN_BUFFER) == 0) {
            break;
        }

        const uint8_t raw = inb(KBD_DATA_PORT);

        if (status & 0x20) {
            // Mouse data
            panic("Mouse data in keyboard handler");
        }

        data               = raw;
        have_keyboard_data = true;
        // break;
    }

    if (!have_keyboard_data) {
        return 0;
    }

    if (data == 0xE0) {
        shift |= E0ESC;
        return 0;
    }

    if (data & 0x80) {
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        return 0;
    }

    if (shift & E0ESC) {
        data |= 0x80;
        shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];

    uint8_t c = charcode[shift & (CTRL | SHIFT)][data];
    if (shift & CAPSLOCK) {
        if ('a' <= c && c <= 'z') {
            c += 'A' - 'a';
        } else if ('A' <= c && c <= 'Z') {
            c += 'a' - 'A';
        }
    }

    return c;
}


/**
 * @brief Flush any pending bytes from the controller output buffer.
 */
static void keyboard_buffer_clear()
{
    while (true) {
        const uint8_t status = inb(KBD_STATUS_PORT);
        if ((status & KBD_DATA_IN_BUFFER) == 0) {
            break;
        }

        const uint8_t data = inb(KBD_DATA_PORT);
        if (status & 0x20) {
            panic("Mouse data in keyboard buffer clear");
        }
    }
}

/**
 * @brief Initialise the PS/2 keyboard controller and register interrupts.
 */
int ps2_keyboard_init()
{
    outb(KBD_STATUS_PORT, 0xAD); // Disable first ps2 port
    keyboard_buffer_clear();
    outb(KBD_STATUS_PORT, 0xAE); // keyboard enable command

    // Configure PS/2 controller: enable keyboard interrupt and translation
    ps2_wait(1);
    outb(KBD_STATUS_PORT, 0x20);  // Read configuration byte
    ps2_wait(0);
    uint8_t config = inb(KBD_DATA_PORT);
    config |= 0x01;   // Enable keyboard interrupt (bit 0)
    config |= 0x40;   // Enable translation (bit 6)
    config &= ~0x10;  // Enable keyboard clock (bit 4 = 0 means enabled)
    ps2_wait(1);
    outb(KBD_STATUS_PORT, 0x60);  // Write configuration byte
    ps2_wait(1);
    outb(KBD_DATA_PORT, config);

    while (inb(KBD_STATUS_PORT) & 2)
        ;
    outb(KBD_DATA_PORT, 0xFF); // keyboard reset command

    int timeout = 100000;
    while ((inb(0x64) & 1) == 0 && --timeout)
        ;

    kbd_led_handling(0x07);
    keyboard_buffer_clear();

    idt_register_interrupt_callback(ISR_KEYBOARD, ps2_keyboard_interrupt_handler);

    return 0;
}

/**
 * @brief Interrupt handler for PS/2 keyboard events.
 */
void ps2_keyboard_interrupt_handler([[maybe_unused]] struct interrupt_frame *frame)
{
    const uint8_t c = keyboard_get_char();

    // Delete key
    if (c > 0 && c != 233) {
        keyboard_push(c);
    }
}

/**
 * @brief Create a keyboard driver instance for the PS/2 controller.
 */
struct keyboard ps2_init()
{
    return (struct keyboard){.name = "ps2", .init = ps2_keyboard_init};
}
