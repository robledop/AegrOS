#include "types.h"
#include "defs.h"
#include "keyboard.h"
#include "io.h"
#include "printf.h"
#include "traps.h"

static u8 shiftcode[256] =
{
    [0x1D] CTL,
    [0x2A] SHIFT,
    [0x36] SHIFT,
    [0x38] ALT,
    [0x9D] CTL,
    [0xB8] ALT
};

static u8 togglecode[256] =
{
    [0x3A] CAPSLOCK,
    [0x45] NUMLOCK,
    [0x46] SCROLLLOCK
};

static u8 normalmap[256] =
{
    NO, 0x1B, '1', '2', '3', '4', '5', '6', // 0x00
    '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', // 0x10
    'o', 'p', '[', ']', '\n', NO, 'a', 's',
    'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', // 0x20
    '\'', '`', NO, '\\', 'z', 'x', 'c', 'v',
    'b', 'n', 'm', ',', '.', '/', NO, '*', // 0x30
    NO, ' ', NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, '7', // 0x40
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', NO, NO, NO, NO, // 0x50
    [0x9C]'\n',                         // KP_Enter
    [0xB5]'/',                          // KP_Div
    [0xC8] KEY_UP, [0xD0] KEY_DN,
    [0xC9] KEY_PGUP, [0xD1] KEY_PGDN,
    [0xCB] KEY_LF, [0xCD] KEY_RT,
    [0x97] KEY_HOME, [0xCF] KEY_END,
    [0xD2] KEY_INS, [0xD3] KEY_DEL
};

static u8 shiftmap[256] =
{
    NO, 033, '!', '@', '#', '$', '%', '^', // 0x00
    '&', '*', '(', ')', '_', '+', '\b', '\t',
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', // 0x10
    'O', 'P', '{', '}', '\n', NO, 'A', 'S',
    'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', // 0x20
    '"', '~', NO, '|', 'Z', 'X', 'C', 'V',
    'B', 'N', 'M', '<', '>', '?', NO, '*', // 0x30
    NO, ' ', NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, '7', // 0x40
    '8', '9', '-', '4', '5', '6', '+', '1',
    '2', '3', '0', '.', NO, NO, NO, NO, // 0x50
    [0x9C]'\n',                         // KP_Enter
    [0xB5]'/',                          // KP_Div
    [0xC8] KEY_UP, [0xD0] KEY_DN,
    [0xC9] KEY_PGUP, [0xD1] KEY_PGDN,
    [0xCB] KEY_LF, [0xCD] KEY_RT,
    [0x97] KEY_HOME, [0xCF] KEY_END,
    [0xD2] KEY_INS, [0xD3] KEY_DEL
};

static u8 ctlmap[256] =
{
    NO, NO, NO, NO, NO, NO, NO, NO,
    NO, NO, NO, NO, NO, NO, NO, NO,
    C('Q'), C('W'), C('E'), C('R'), C('T'), C('Y'), C('U'), C('I'),
    C('O'), C('P'), NO, NO, '\r', NO, C('A'), C('S'),
    C('D'), C('F'), C('G'), C('H'), C('J'), C('K'), C('L'), NO,
    NO, NO, NO, C('\\'), C('Z'), C('X'), C('C'), C('V'),
    C('B'), C('N'), C('M'), NO, NO, C('/'), NO, NO,
    [0x9C]'\r',    // KP_Enter
    [0xB5] C('/'), // KP_Div
    [0xC8] KEY_UP, [0xD0] KEY_DN,
    [0xC9] KEY_PGUP, [0xD1] KEY_PGDN,
    [0xCB] KEY_LF, [0xCD] KEY_RT,
    [0x97] KEY_HOME, [0xCF] KEY_END,
    [0xD2] KEY_INS, [0xD3] KEY_DEL
};


/**
 * @brief Flush any pending bytes from the controller output buffer.
 */
static void keyboard_buffer_clear()
{
    while (true) {
        const u8 status = inb(KBD_STATUS_PORT);
        if ((status & KBD_DATA_IN_BUFFER) == 0) {
            break;
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
        const u8 status = inb(KBD_STATUS_PORT);
        if ((status & KBD_DATA_IN_BUFFER) == 0) {
            continue;
        }

        const u8 data = inb(KBD_DATA_PORT);
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

int keyboard_getchar(void)
{
    static u32 shift;
    static u8 *charcode[4] = {
        normalmap, shiftmap, ctlmap, ctlmap
    };

    u32 st = inb(KBD_STATUS_PORT);
    if ((st & KBD_DATA_IN_BUFFER) == 0) {
        return -1;
    }
    u32 data = inb(KBD_DATA_PORT);

    if (data == 0xE0) {
        shift |= E0ESC;
        return 0;
    }
    if (data & 0x80) {
        // Key released
        data = (shift & E0ESC ? data : data & 0x7F);
        shift &= ~(shiftcode[data] | E0ESC);
        return 0;
    }
    if (shift & E0ESC) {
        // The last character was an E0 escape; or with 0x80
        data |= 0x80;
        shift &= ~E0ESC;
    }

    shift |= shiftcode[data];
    shift ^= togglecode[data];
    u32 c = charcode[shift & (CTL | SHIFT)][data];
    if (shift & CAPSLOCK) {
        if ('a' <= c && c <= 'z') {
            c += 'A' - 'a';
        } else if ('A' <= c && c <= 'Z') {
            c += 'a' - 'A';
        }
    }
    return c;
}


// void keyboard_handler([[maybe_unused]] struct trapframe *tf)
// {
//     keyboard_interrupt_handler();
//     lapiceoi(); // Acknowledge the interrupt
// }


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
    outb(KBD_STATUS_PORT, 0x20); // Read configuration byte
    ps2_wait(0);
    u8 config = inb(KBD_DATA_PORT);
    config |= 0x01;  // Enable keyboard interrupt (bit 0)
    config |= 0x40;  // Enable translation (bit 6)
    config &= ~0x10; // Enable keyboard clock (bit 4 = 0 means enabled)
    ps2_wait(1);
    outb(KBD_STATUS_PORT, 0x60); // Write configuration byte
    ps2_wait(1);
    outb(KBD_DATA_PORT, config);

    while (inb(KBD_STATUS_PORT) & 2) {
    }
    outb(KBD_DATA_PORT, 0xFF); // keyboard reset command

    int timeout = 100000;
    while ((inb(0x64) & 1) == 0 && --timeout) {
    }

    kbd_led_handling(0x07);
    // keyboard_buffer_clear();

    // idt_register_interrupt_callback(T_IRQ0 + IRQ_KBD, keyboard_handler);
    return 0;
}

void keyboard_interrupt_handler(void)
{
    console_input_handler(keyboard_getchar);
}