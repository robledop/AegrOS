#include <idt.h>
#include <io.h>
#include <kernel.h>
#include <mouse.h>
#include <pic.h>
#include <ps2_controller.h>
#include <spinlock.h>
#include <vesa.h>

#define ISR_PS2_MOUSE (0x20 + 12)

static struct ps2_mouse mouse_device = {};

#define MOUSE_EVENT_QUEUE_CAPACITY 32

struct mouse_event_queue {
    mouse_t events[MOUSE_EVENT_QUEUE_CAPACITY];
    unsigned int head;
    unsigned int tail;
    unsigned int count;
};

static struct mouse_event_queue mouse_events = {};

static void mouse_queue_push(const mouse_t event)
{
    pushcli();

    if (mouse_events.count == MOUSE_EVENT_QUEUE_CAPACITY) {
        mouse_events.head = (mouse_events.head + 1) % MOUSE_EVENT_QUEUE_CAPACITY;
        mouse_events.count--;
    }

    mouse_events.events[mouse_events.tail] = event;
    mouse_events.tail                      = (mouse_events.tail + 1) % MOUSE_EVENT_QUEUE_CAPACITY;
    mouse_events.count++;

    popcli();
}

static bool mouse_queue_pop(mouse_t *event)
{
    if (mouse_events.count == 0) {
        return false;
    }

    *event = mouse_events.events[mouse_events.head];
    mouse_events.head = (mouse_events.head + 1) % MOUSE_EVENT_QUEUE_CAPACITY;
    mouse_events.count--;
    return true;
}

static void mouse_emit_event(void)
{
    mouse_device.prev_x = mouse_device.x;
    mouse_device.prev_y = mouse_device.y;

    mouse_device.x += mouse_device.packet.x;
    mouse_device.y -= mouse_device.packet.y;
    mouse_device.prev_flags = mouse_device.flags;
    mouse_device.flags      = mouse_device.packet.flags;

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

    mouse_device.received = 0;
    mouse_device.packet   = (struct ps2_mouse_packet){0};
    mouse_device.cycle    = 0;

    const mouse_t event = {
        .x          = mouse_device.x,
        .y          = mouse_device.y,
        .flags      = mouse_device.flags,
        .prev_flags = mouse_device.prev_flags,
    };

    mouse_queue_push(event);
}

void ps2_mouse_handle_byte(const uint8_t status, const uint8_t data)
{
    if (!mouse_device.initialized) {
        return;
    }

    if (!(status & MOUSE_F_BIT)) {
        return;
    }

    switch (mouse_device.cycle) {
    case 0:
        mouse_device.packet.flags = data;
        if (!(data & MOUSE_V_BIT)) {
            // resynchronise
            mouse_device.cycle = 0;
            return;
        }
        mouse_device.cycle = 1;
        break;
    case 1:
        mouse_device.packet.x = (int8_t)data;
        mouse_device.cycle    = 2;
        break;
    case 2:
        mouse_device.packet.y = (int8_t)data;
        mouse_emit_event();
        break;
    default:
        mouse_device.cycle = 0;
        break;
    }
}

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

// void mouse_draw_mouse_cursor()
// {
// #if 0
//     char *flags = "     ";
//     vesa_print_string("     ", 5, 850, 0, 0x000000, DESKTOP_BACKGROUND_COLOR);
//     itohex(mouse_device.flags, flags);
//     vesa_print_string(flags, (int)strlen(flags), 850, 0, 0xFFFFFF, DESKTOP_BACKGROUND_COLOR);
//
//     char *x = "     ";
//     vesa_print_string("     ", 5, 900, 0, 0x000000, DESKTOP_BACKGROUND_COLOR);
//     itoa(mouse_device.x, x);
//     vesa_print_string(x, (int)strlen(x), 900, 0, 0xFFFFFF, DESKTOP_BACKGROUND_COLOR);
//
//     char *y = "     ";
//     vesa_print_string("     ", 5, 950, 0, 0x000000, DESKTOP_BACKGROUND_COLOR);
//     itoa(mouse_device.y, y);
//     vesa_print_string(y, (int)strlen(y), 950, 0, 0xFFFFFF, DESKTOP_BACKGROUND_COLOR);
// #endif
//
//     // vesa_draw_mouse_cursor(mouse_device.x, mouse_device.y);
// }

/**
 * @brief Interrupt handler for PS/2 mouse packets.
 */
void mouse_handler([[maybe_unused]] struct interrupt_frame *frame)
{
    acquire(&ps2_controller_lock);
    uint8_t status;
    while ((status = inb(MOUSE_STATUS)) & MOUSE_B_BIT) {
        const uint8_t data = (uint8_t)inb(MOUSE_PORT);
        ps2_mouse_handle_byte(status, data);
    }
    release(&ps2_controller_lock);

    if (frame) {
        pic_acknowledge((int)frame->interrupt_number);
    } else {
        pic_acknowledge(ISR_PS2_MOUSE);
    }

    mouse_flush_pending_events();
}

/**
 * @brief Initialise the PS/2 mouse and register its callback.
 *
 * @param callback Function invoked for each processed mouse packet.
 */
void mouse_init(mouse_callback callback)
{
    ps2_controller_init_once();

    acquire(&ps2_controller_lock);
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
    release(&ps2_controller_lock);

    int result = idt_register_interrupt_callback(ISR_PS2_MOUSE, mouse_handler);
    if (result != 0) {
        panic("Failed to register mouse interrupt handler");
    }

    mouse_device.initialized = 1;
    mouse_device.callback    = callback;

    mouse_flush_pending_events();
}

void mouse_flush_pending_events(void)
{
    while (true) {
        mouse_t event;

        pushcli();
        const bool has_event = mouse_queue_pop(&event);
        popcli();

        if (!has_event) {
            break;
        }

        if (mouse_device.callback) {
            mouse_device.callback(event);
        }
    }
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
