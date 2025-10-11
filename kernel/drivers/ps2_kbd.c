#include <idt.h>
#include <io.h>
#include <kernel.h>
#include <kernel_heap.h>
#include <keyboard.h>
#include <mouse.h>
#include <pic.h>
#include <printf.h>
#include <ps2_controller.h>
#include <ps2_kbd.h>
#include <spinlock.h>
#include <string.h>

struct spinlock keyboard_lock         = {};
struct spinlock keyboard_getchar_lock = {};

int ps2_keyboard_init();
void ps2_keyboard_interrupt_handler(struct interrupt_frame *frame);

#define PS2_CAPSLOCK 0x3A

/**
 * @brief Wait for the keyboard controller to acknowledge the previous command.
 */
void kbd_ack(void)
{
    bool acquired_here = false;
    if (!holding(&ps2_controller_lock)) {
        acquire(&ps2_controller_lock);
        acquired_here = true;
    }
    unsigned int timeout = 1000000;
    while (timeout--) {
        const uint8_t status = inb(KBD_STATUS_PORT);
        if ((status & KBD_DATA_IN_BUFFER) == 0) {
            continue;
        }

        const uint8_t data = inb(KBD_DATA_PORT);
        if (status & 0x20) {
            ps2_mouse_handle_byte(status, data);
            continue;
        }

        if (data == 0xFA) {
            if (acquired_here) {
                release(&ps2_controller_lock);
            }
            return;
        }
    }

    printf("[KBD] Warning: timeout waiting for ACK\n");
    if (acquired_here) {
        release(&ps2_controller_lock);
    }
}

/**
 * @brief Update the keyboard LED status.
 */
void kbd_led_handling(const unsigned char ledstatus)
{
    bool acquired_here = false;
    if (!holding(&ps2_controller_lock)) {
        acquire(&ps2_controller_lock);
        acquired_here = true;
    }
    outb(0x60, 0xed);
    kbd_ack();
    outb(0x60, ledstatus);
    if (acquired_here) {
        release(&ps2_controller_lock);
    }
}

// Taken from xv6
/**
 * @brief Fetch a translated character from the PS/2 keyboard.
 *
 * @return ASCII character or 0 when no key is available.
 */
uint8_t keyboard_get_char()
{
    acquire(&keyboard_getchar_lock);
    acquire(&ps2_controller_lock);

    static unsigned int shift;
    static uint8_t *charcode[4] = {normalmap, shiftmap, ctlmap, ctlmap};

    unsigned int data   = 0;
    bool have_keyboard_data = false;

    for (int attempt = 0; attempt < 16; attempt++) {
        const uint8_t status = inb(KBD_STATUS_PORT);
        if ((status & KBD_DATA_IN_BUFFER) == 0) {
            break;
        }

        const uint8_t raw = inb(KBD_DATA_PORT);

        if (status & 0x20) {
            ps2_mouse_handle_byte(status, raw);
            pic_acknowledge(0x20 + 12);
            continue;
        }

        data               = raw;
        have_keyboard_data = true;
        break;
    }

    if (!have_keyboard_data) {
        release(&ps2_controller_lock);
        release(&keyboard_getchar_lock);
        return 0;
    }

    if (data == 0xE0) {
        shift |= E0ESC;
        release(&ps2_controller_lock);
        release(&keyboard_getchar_lock);
        return 0;
    }

    if (data & 0x80) {
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        release(&ps2_controller_lock);
        release(&keyboard_getchar_lock);
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

    release(&ps2_controller_lock);
    release(&keyboard_getchar_lock);
    return c;
}


/**
 * @brief Flush any pending bytes from the controller output buffer.
 */
static void keyboard_buffer_clear()
{
    bool acquired_here = false;
    if (!holding(&ps2_controller_lock)) {
        acquire(&ps2_controller_lock);
        acquired_here = true;
    }
    while (true) {
        const uint8_t status = inb(KBD_STATUS_PORT);
        if ((status & KBD_DATA_IN_BUFFER) == 0) {
            break;
        }

        const uint8_t data = inb(KBD_DATA_PORT);
        if (status & 0x20) {
            ps2_mouse_handle_byte(status, data);
        }
    }
    if (acquired_here) {
        release(&ps2_controller_lock);
    }
}

/**
 * @brief Initialise the PS/2 keyboard controller and register interrupts.
 */
int ps2_keyboard_init()
{
    initlock(&keyboard_lock, "keyboard");
    initlock(&keyboard_getchar_lock, "getchar");
    ps2_controller_init_once();


    acquire(&ps2_controller_lock);
    outb(KBD_STATUS_PORT, 0xAD); // Disable first ps2 port
    keyboard_buffer_clear();
    outb(KBD_STATUS_PORT, 0xAE); // keyboard enable command

    while (inb(KBD_STATUS_PORT) & 2)
        ;
    outb(KBD_DATA_PORT, 0xFF); // keyboard reset command

    int timeout = 100000;
    while ((inb(0x64) & 1) == 0 && --timeout)
        ;

    kbd_led_handling(0x07);
    keyboard_buffer_clear();
    release(&ps2_controller_lock);

    idt_register_interrupt_callback(ISR_KEYBOARD, ps2_keyboard_interrupt_handler);

    return 0;
}

/**
 * @brief Interrupt handler for PS/2 keyboard events.
 */
void ps2_keyboard_interrupt_handler(struct interrupt_frame *frame)
{
    pic_acknowledge((int)frame->interrupt_number);
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
