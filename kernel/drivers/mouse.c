#include <idt.h>
#include <io.h>
#include <kernel.h>
#include <mouse.h>
#include <vesa.h>

extern struct vbe_mode_info *vbe_info;
#define ISR_PS2_MOUSE (0x20 + 12)

static struct ps2_mouse mouse_device = {};

/**
 * @brief Wait for the PS/2 controller to become ready for read or write.
 *
 * @param a_type 0 to wait for available data, non-zero to wait for write readiness.
 */
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

/**
 * @brief Update the cached mouse coordinates without generating callbacks.
 */
void mouse_set_position(int16_t x, int16_t y)
{
    mouse_device.x = x;
    mouse_device.y = y;
}

/**
 * @brief Send a command byte to the PS/2 mouse device.
 */
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

/**
 * @brief Read a byte from the PS/2 mouse data port.
 */
uint8_t mouse_read()
{
    mouse_wait(0);
    return inb(MOUSE_PORT);
}

/**
 * @brief Interrupt handler for PS/2 mouse packets.
 */
void mouse_handler([[maybe_unused]] struct interrupt_frame *frame)
{
    uint8_t status                 = inb(MOUSE_STATUS);
    struct ps2_mouse_packet packet = {};
    while (status & MOUSE_B_BIT) {
        int8_t mouse_data = (int8_t)inb(MOUSE_PORT);
        if (status & MOUSE_F_BIT) {
            // The mouse data comes in three packets:
            switch (mouse_device.cycle) {
            case 0: // The first one sends the flags.
                {
                    packet.flags = mouse_data;
                    if (!(mouse_data & MOUSE_V_BIT)) {
                        return;
                    }
                    ++mouse_device.cycle;
                }
                break;
            case 1: // The second one sends the x position.
                {
                    packet.x = mouse_data;
                    ++mouse_device.cycle;
                }
                break;
            case 2: // The third one sends the y position.
                {
                    packet.y = mouse_data;

                    mouse_device.prev_x = mouse_device.x;
                    mouse_device.prev_y = mouse_device.y;

                    mouse_device.x += packet.x;
                    mouse_device.y -= packet.y;
                    mouse_device.prev_flags = mouse_device.flags;
                    mouse_device.flags      = packet.flags;

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

    // mouse_draw_mouse_cursor();
    mouse_device.callback((mouse_t){
        .x          = mouse_device.x,
        .y          = mouse_device.y,
        .flags      = mouse_device.flags,
        .prev_flags = mouse_device.prev_flags,
    });
}

/**
 * @brief Initialise the PS/2 mouse and register its callback.
 *
 * @param callback Function invoked for each processed mouse packet.
 */
void mouse_init(mouse_callback callback)
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
    mouse_device.callback    = callback;
}

// Function to get current mouse position and status
/**
 * @brief Retrieve the latest mouse coordinates and button state.
 *
 * @param mouse Output structure receiving the state.
 */
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
