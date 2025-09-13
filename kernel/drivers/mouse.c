#include <idt.h>
#include <io.h>
#include <mouse.h>
#include <string.h>
#include <vbe.h>

#include "compositor.h"
#include "kernel.h"
#include "pic.h"
#include "printf.h"

#define ISR_PS2_MOUSE (0x20 + 12)

extern window_t *windows[MAX_WINDOWS];

static struct ps2_mouse mouse_device = {};

static void mouse_wait(unsigned char a_type)
{
    unsigned int timeout = 1000000;
    if (!a_type) {
        while (--timeout) {
            if ((inb(MOUSE_STATUS) & MOUSE_B_BIT) == 1) {
                return;
            }
        }
    } else {
        while (--timeout) {
            if (!((inb(MOUSE_STATUS) & MOUSE_A_BIT))) {
                return;
            }
        }
    }
}

static void mouse_write(uint8_t command)
{
    // Wait to be able to send a command
    mouse_wait(1);
    // Tell the mouse we are sending a command
    outb(MOUSE_STATUS, MOUSE_WRITE);
    // Wait for the final part
    mouse_wait(1);
    // Finally write
    outb(MOUSE_PORT, command);
}

uint8_t mouse_read()
{
    mouse_wait(0);
    return inb(MOUSE_PORT);
}

void draw_mouse_cursor(void)
{
    char *flags = "     ";
    vesa_print_string("     ", 5, 850, 0, 0x000000, 0x000000);
    itohex(mouse_device.flags, flags);
    vesa_print_string(flags, (int)strlen(flags), 850, 0, 0xFFFFFF, 0x000000);

    char *x = "     ";
    vesa_print_string("     ", 5, 900, 0, 0x000000, 0x000000);
    itoa(mouse_device.x, x);
    vesa_print_string(x, (int)strlen(x), 900, 0, 0xFFFFFF, 0x000000);

    char *y = "     ";
    vesa_print_string("     ", 5, 950, 0, 0x000000, 0x000000);
    itoa(mouse_device.y, y);
    vesa_print_string(y, (int)strlen(y), 950, 0, 0xFFFFFF, 0x000000);

    vesa_restore_mouse_cursor();
    vesa_draw_mouse_cursor(mouse_device.x, mouse_device.y);

    if (mouse_device.flags & MOUSE_LEFT) {
        vesa_print_string("L", 1, 800, 0, 0xFFFFFF, 0x000000);

        for (int i = 0; i < MAX_WINDOWS; i++) {
            auto clicked = window_was_clicked(windows[i], mouse_device.x, mouse_device.y);
            if (clicked) {
                printf("Clicked window %s\n", windows[i]->name);
            }
        }
    } else {
        vesa_print_string(" ", 1, 800, 0, 0xFFFFFF, 0x000000);
    }

    if (mouse_device.flags & MOUSE_RIGHT) {
        vesa_print_string("R", 1, 810, 0, 0xFFFFFF, 0x000000);
    } else {
        vesa_print_string(" ", 1, 810, 0, 0xFFFFFF, 0x000000);
    }

    if (mouse_device.flags & MOUSE_MIDDLE) {
        vesa_print_string("M", 1, 820, 0, 0xFFFFFF, 0x000000);
    } else {
        vesa_print_string(" ", 1, 820, 0, 0xFFFFFF, 0x000000);
    }
}
void mouse_handler([[maybe_unused]] struct interrupt_frame *frame)
{
    uint8_t status                 = inb(MOUSE_STATUS);
    struct ps2_mouse_packet packet = {};
    while (status & MOUSE_B_BIT) {
        int8_t mouse_in = (int8_t)inb(MOUSE_PORT);
        if (status & MOUSE_F_BIT) {
            // The mouse data comes in three packets:
            switch (mouse_device.cycle) {
            case 0: // The first one sends the flags.
                {
                    packet.flags = mouse_in;
                    if (!(mouse_in & MOUSE_V_BIT)) {
                        return;
                    }
                    ++mouse_device.cycle;
                }
                break;
            case 1: // The second one sends the x position.
                {
                    packet.x = mouse_in;
                    ++mouse_device.cycle;
                }
                break;
            case 2: // The third one sends the y position.
                {
                    packet.y = mouse_in;

                    mouse_device.x += packet.x;
                    mouse_device.y -= packet.y;
                    mouse_device.flags = packet.flags;

                    // Clamp to screen bounds
                    if (mouse_device.x < 0) {
                        mouse_device.x = 0;
                    }
                    if (mouse_device.y < 0) {
                        mouse_device.y = 0;
                    }

                    if (mouse_device.x > vbe_info->width) {
                        mouse_device.x = vbe_info->width;
                    }
                    if (mouse_device.y > vbe_info->height) {
                        mouse_device.y = vbe_info->height;
                    }

                    // Start over
                    mouse_device.received = 0;
                    mouse_device.cycle    = 0;
                }
                break;
            default:
                panic("This should never happen");
            }
        }
        status = inb(MOUSE_STATUS);
    }

    draw_mouse_cursor();
}

void mouse_init()
{
    mouse_wait(1);
    outb(MOUSE_STATUS, 0xA8);
    mouse_wait(1);
    outb(MOUSE_STATUS, 0x20);
    mouse_wait(0);
    uint8_t status = inb(0x60) | 2;
    mouse_wait(1);
    outb(MOUSE_STATUS, 0x60);
    mouse_wait(1);
    outb(MOUSE_PORT, status);
    mouse_write(0xF6);
    mouse_read();
    mouse_write(0xF4);
    mouse_read();

    int result = idt_register_interrupt_callback(ISR_PS2_MOUSE, mouse_handler);
    if (result != 0) {
        panic("Failed to register mouse interrupt handler");
    }

    mouse_device.initialized = 1;
}

// Function to get current mouse position and status
void mouse_get_position(struct mouse *mouse)
{
    if (mouse_device.received) {
        return;
    }

    mouse->x     = mouse_device.x;
    mouse->y     = mouse_device.y;
    mouse->flags = mouse_device.flags;

    mouse_device.received = 1;
}
