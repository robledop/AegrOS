#include <io.h>
#include <mouse.h>
#include <types.h>
#include <traps.h>
#include <framebuffer.h>
#include <defs.h>
#include <file.h>
#include <string.h>

static struct ps2_mouse mouse_device = {};
static struct ps2_mouse_packet mouse_buffer[32];
static int mouse_buf_head = 0;
static int mouse_buf_tail = 0;
struct spinlock mouse_lock;

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
void mouse_set_position(i16 x, i16 y)
{
    mouse_device.x = x;
    mouse_device.y = y;
}

/**
 * @brief Send a command byte to the PS/2 mouse device.
 */
static void mouse_device_write(u8 command)
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
u8 mouse_device_read()
{
    mouse_wait(0);
    return inb(MOUSE_PORT);
}

static inline bool mouse_buffer_empty(void)
{
    return mouse_buf_head == mouse_buf_tail;
}

static inline bool mouse_buffer_full(void)
{
    return ((mouse_buf_head + 1) % NELEM(mouse_buffer)) == mouse_buf_tail;
}

static void mouse_buffer_push(struct ps2_mouse_packet pkt)
{
    int next = (mouse_buf_head + 1) % NELEM(mouse_buffer);
    if (next == mouse_buf_tail) {
        mouse_buf_tail = (mouse_buf_tail + 1) % NELEM(mouse_buffer);
    }
    mouse_buffer[mouse_buf_head] = pkt;
    mouse_buf_head               = next;
}

static bool mouse_buffer_pop(struct ps2_mouse_packet *pkt)
{
    if (mouse_buffer_empty()) {
        return false;
    }
    *pkt           = mouse_buffer[mouse_buf_tail];
    mouse_buf_tail = (mouse_buf_tail + 1) % NELEM(mouse_buffer);
    return true;
}

int mouse_read(struct inode *ip, char *dst, int n, [[maybe_unused]] u32 offset)
{
    ip->iops->iunlock(ip);
    acquire(&mouse_lock);

    while (mouse_buffer_empty()) {
        sleep(&mouse_device, &mouse_lock);
    }

    struct ps2_mouse_packet packet;
    mouse_buffer_pop(&packet);
    const int bytes = (n < (int)sizeof(packet)) ? n : (int)sizeof(packet);
    memmove(dst, &packet, (size_t)bytes);

    release(&mouse_lock);
    ip->iops->ilock(ip);

    return bytes;
}


/**
 * @brief Interrupt handler for PS/2 mouse packets.
 */
void mouse_handler([[maybe_unused]] struct trapframe *frame)
{
    u8 status = inb(MOUSE_STATUS);
    while (status & MOUSE_B_BIT) {
        const i8 byte = (i8)inb(MOUSE_PORT);

        if ((status & MOUSE_F_BIT) == 0) {
            status = inb(MOUSE_STATUS);
            continue;
        }

        switch (mouse_device.cycle) {
        case 0:
            mouse_device.packet.flags = (u8)byte;
            if ((mouse_device.packet.flags & MOUSE_V_BIT) == 0) {
                mouse_device.cycle = 0;
                status             = inb(MOUSE_STATUS);
                continue;
            }
            mouse_device.cycle = 1;
            break;
        case 1:
            mouse_device.packet.x = byte;
            mouse_device.cycle = 2;
            break;
        case 2: {
            mouse_device.packet.y = byte;

            mouse_device.prev_x = mouse_device.x;
            mouse_device.prev_y = mouse_device.y;

            mouse_device.x += mouse_device.packet.x;
            mouse_device.y -= mouse_device.packet.y;

            mouse_device.prev_flags   = mouse_device.flags;
            mouse_device.flags        = mouse_device.packet.flags & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE);
            mouse_device.packet.flags = mouse_device.packet.flags & (MOUSE_LEFT | MOUSE_RIGHT | MOUSE_MIDDLE);

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

            mouse_device.packet.x = mouse_device.x;
            mouse_device.packet.y = mouse_device.y;

            acquire(&mouse_lock);
            mouse_buffer_push(mouse_device.packet);
            wakeup(&mouse_device);
            release(&mouse_lock);

            mouse_device.cycle = 0;
            break;
        }
        default:
            mouse_device.cycle = 0;
            break;
        }

        status = inb(MOUSE_STATUS);
    }

    lapic_ack_interrupt();
}

/**
 * @brief Initialise the PS/2 mouse and register its callback.
 *
 * @param callback Function invoked for each processed mouse packet.
 */
void mouse_init(mouse_callback callback)
{
    initlock(&mouse_lock, "mouse");
    mouse_buf_head = mouse_buf_tail = 0;

    mouse_wait(1);
    outb(MOUSE_STATUS, 0xA8);
    mouse_wait(1);
    outb(MOUSE_STATUS, 0x20);
    mouse_wait(0);
    u8 status = inb(0x60);
    status |= 0x03;  // Enable both keyboard (bit 0) and mouse (bit 1) interrupts
    status |= 0x40;  // Ensure translation stays enabled (bit 6)
    status &= ~0x20; // Enable mouse clock (bit 5 = 0 means enabled)
    mouse_wait(1);
    outb(MOUSE_STATUS, 0x60);
    mouse_wait(1);
    outb(MOUSE_PORT, status);
    // Set defaults and put mouse in stream mode, then enable data reporting
    mouse_device_write(MOUSE_SET_DEFAULTS);
    mouse_device_read();
    mouse_device_write(MOUSE_SET_STREAM_MODE);
    mouse_device_read();
    mouse_device_write(MOUSE_ENABLE_DATA_REPORTING);
    mouse_device_read();

    idt_register_interrupt_callback(T_IRQ0 + IRQ_MOUSE, mouse_handler);
    enable_ioapic_interrupt(IRQ_MOUSE, 0);

    mouse_device.initialized = 1;

    devsw[MOUSE_MAJOR].read  = mouse_read;
    devsw[MOUSE_MAJOR].write = nullptr;
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