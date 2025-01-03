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

void kbd_ack(void)
{
    while (inb(0x60) != 0xfa)
        ;
}

void kbd_led_handling(const unsigned char ledstatus)
{
    outb(0x60, 0xed);
    kbd_ack();
    outb(0x60, ledstatus);
}

// Taken from xv6
uint8_t keyboard_get_char()
{
    acquire(&keyboard_getchar_lock);

    static unsigned int shift;
    static uint8_t *charcode[4] = {normalmap, shiftmap, ctlmap, ctlmap};

    const unsigned int st = inb(KBD_STATUS_PORT);
    if ((st & KBD_DATA_IN_BUFFER) == 0) {
        release(&keyboard_getchar_lock);
        return -1;
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

int ps2_keyboard_init()
{
    initlock(&keyboard_lock, "keyboard");
    initlock(&keyboard_getchar_lock, "getchar");

    idt_register_interrupt_callback(ISR_KEYBOARD, ps2_keyboard_interrupt_handler);

    outb(KBD_STATUS_PORT, 0xAE); // keyboard enable command
    outb(KBD_DATA_PORT, 0xFF);   // keyboard reset command

    kbd_led_handling(0x07);

    return 0;
}

void ps2_keyboard_interrupt_handler(struct interrupt_frame *frame)
{
    pic_acknowledge((int)frame->interrupt_number);

    const uint8_t c = keyboard_get_char();
    // Delete key
    if (c > 0 && c != 233) {
        keyboard_push(c);
    }
}

struct keyboard *ps2_init()
{
    struct keyboard *kbd = kzalloc(sizeof(struct keyboard));
    strncpy(kbd->name, "ps2", sizeof(kbd->name));
    kbd->init = ps2_keyboard_init;

    return kbd;
}