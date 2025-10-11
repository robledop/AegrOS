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
    uint8_t status = inb(MOUSE_STATUS);
    while (status & MOUSE_B_BIT) {
        const int8_t byte = (int8_t)inb(MOUSE_PORT);

        // Only consider bytes coming from the AUX (mouse) device
        if ((status & MOUSE_F_BIT) == 0) {
            status = inb(MOUSE_STATUS);
            continue;
        }

        switch (mouse_device.cycle) {
        case 0: {
            // First byte: flags. Bit 3 must be set for a valid packet.
            mouse_device.packet.flags = (uint8_t)byte;
            if ((mouse_device.packet.flags & MOUSE_V_BIT) == 0) {
                // Desynchronized: reset and look for a proper first byte.
                mouse_device.cycle = 0;
                status             = inb(MOUSE_STATUS);
                continue;
            }
            mouse_device.cycle = 1;
            break;
        }
        case 1: {
            // Second byte: X movement
            mouse_device.packet.x = byte;
            mouse_device.cycle    = 2;
            break;
        }
        case 2: {
            // Third byte: Y movement
            mouse_device.packet.y = byte;

            mouse_device.prev_x = mouse_device.x;
            mouse_device.prev_y = mouse_device.y;

            mouse_device.x += mouse_device.packet.x;
            mouse_device.y -= mouse_device.packet.y; // PS/2 Y is up-negative

            mouse_device.prev_flags = mouse_device.flags;
            // Only expose button bits to consumers
            mouse_device.flags = mouse_device.packet.flags & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE);

            // Clamp to screen bounds (inclusive minimum, exclusive maximum)
            if (mouse_device.x < 0) {
                mouse_device.x = 0;
            }
            if (mouse_device.y < 0) {
                mouse_device.y = 0;
            }
            if (vbe_info) {
                if (mouse_device.x >= (int)vbe_info->width) {
                    mouse_device.x = (int)vbe_info->width - 1;
                }
                if (mouse_device.y >= (int)vbe_info->height) {
                    mouse_device.y = (int)vbe_info->height - 1;
                }
            }

            // Packet complete
            mouse_device.received = 0;
            mouse_device.cycle    = 0;
            break;
        }
        default:
            // Should never happen; resync
            mouse_device.cycle = 0;
            break;
        }

        status = inb(MOUSE_STATUS);
    }

    // Deliver event after processing available bytes
    if (mouse_device.callback) {
        mouse_device.callback((mouse_t){
            .x          = mouse_device.x,
            .y          = mouse_device.y,
            .flags      = mouse_device.flags,
            .prev_flags = mouse_device.prev_flags,
        });
    }
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
    // Set defaults and put mouse in stream mode, then enable data reporting
    mouse_write(MOUSE_SET_DEFAULTS); // 0xF6
    mouse_read();
    mouse_write(MOUSE_SET_STREAM_MODE); // 0xEA
    mouse_read();
    mouse_write(MOUSE_ENABLE_DATA_REPORTING); // 0xF4
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
