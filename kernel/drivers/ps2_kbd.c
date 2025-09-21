#include <idt.h>
#include <io.h>
#include <kernel_heap.h>
#include <keyboard.h>
#include <pic.h>
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
    while (inb(0x60) != 0xfa)
        ;
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
    acquire(&keyboard_getchar_lock);

    static unsigned int shift;
    static uint8_t *charcode[4] = {normalmap, shiftmap, ctlmap, ctlmap};

    const unsigned int st = inb(KBD_STATUS_PORT);
    if ((st & KBD_DATA_IN_BUFFER) == 0) {
        release(&keyboard_getchar_lock);
        return 0;
    }
    unsigned int data = inb(KBD_DATA_PORT);

    if (data == 0xE0) {
        shift |= E0ESC;
        release(&keyboard_getchar_lock);
        return 0;
    }
    if (data & 0x80) {
        // Key released
        // key_released = true;
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        release(&keyboard_getchar_lock);
        return 0;
    }
    if (shift & E0ESC) {
        // Last character was an E0 escape; or with 0x80
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

    release(&keyboard_getchar_lock);
    return c;
}


/**
 * @brief Flush any pending bytes from the controller output buffer.
 */
static void keyboard_buffer_clear()
{
    while (inb(0x64) & 1) {
        inb(0x60); // Read and discard
    }
}

/**
 * @brief Initialise the PS/2 keyboard controller and register interrupts.
 */
int ps2_keyboard_init()
{
    initlock(&keyboard_lock, "keyboard");
    initlock(&keyboard_getchar_lock, "getchar");


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
struct keyboard *ps2_init()
{
    struct keyboard *kbd = kzalloc(sizeof(struct keyboard));
    strncpy(kbd->name, "ps2", sizeof(kbd->name));
    kbd->init = ps2_keyboard_init;

    return kbd;
}
