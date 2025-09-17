#include <config.h>
#include <debug.h>
#include <gdt.h>
#include <gui/button.h>
#include <gui/calculator.h>
#include <gui/desktop.h>
#include <idt.h>
#include <io.h>
#include <kernel.h>
#include <kernel_heap.h>
#include <keyboard.h>
#include <memory.h>
#include <mouse.h>
#include <net/network.h>
#include <paging.h>
#include <pci.h>
#include <pic.h>
#include <pit.h>
#include <printf.h>
#include <process.h>
#include <root_inode.h>
#include <serial.h>
#include <syscall.h>
#include <thread.h>
#include <timer.h>
#include <vesa.h>
#include <vesa_terminal.h>
#include <vfs.h>
#include <vga_buffer.h>
#include <x86.h>

#include "gui/rect.h"
#include "gui/vterm.h"
#include "gui/window.h"
#include "list.h"


void display_grub_info(const multiboot_info_t *mbd, unsigned int magic);

#define STACK_CHK_GUARD 0xe2dee396
uint32_t wait_for_network_start;
uint32_t wait_for_network_timeout = 15'000;

uintptr_t __stack_chk_guard = STACK_CHK_GUARD; // NOLINT(*-reserved-identifier)

extern struct vbe_mode_info *vbe_info;
static desktop_t *desktop;
static vterm_t *terminal;

[[noreturn]] void panic(const char *msg)
{
    debug_stats();
    printf(KRED "\nKERNEL PANIC: " KWHT "%s\n", msg);

    while (1) {
        hlt();
    }

    __builtin_unreachable();
}

void main_mouse_event_handler(mouse_t mouse)
{
    desktop_process_mouse(desktop, mouse.x, mouse.y, mouse.flags);
}

void wait_for_network()
{
    wait_for_network_start = timer_tick;
    sti();
    while (!network_is_ready() && timer_tick - wait_for_network_start < wait_for_network_timeout) {
        hlt();
    }
    cli();
    if (!network_is_ready()) {
        printf("[ " KBRED "FAIL" KWHT " ] ");
        printf(KBYEL "Network failed to start\n" KWHT);
    }
}

void idle()
{
    // ReSharper disable once CppDFAEndlessLoop
    while (true) {
        hlt();
    }
}

void set_vbe_info(const multiboot_info_t *mbd)
{
    vbe_info->height      = mbd->framebuffer_height;
    vbe_info->width       = mbd->framebuffer_width;
    vbe_info->bpp         = mbd->framebuffer_bpp;
    vbe_info->pitch       = mbd->framebuffer_pitch;
    vbe_info->framebuffer = mbd->framebuffer_addr;
}

void putchar_handler(char c)
{
    int old_cursor_x = terminal->cursor_x;
    int old_cursor_y = terminal->cursor_y;

    terminal->putchar(terminal, c);

    auto dirty_regions = list_new();

    // Only mark the specific character positions as dirty
    int buffer_width  = 600 / VESA_CHAR_WIDTH;
    int buffer_height = 400 / VESA_LINE_HEIGHT;

    // Mark old cursor position as dirty (to erase cursor)
    // if (old_cursor_x < buffer_width && old_cursor_y < buffer_height) {
    //     int char_x      = WIN_BORDERWIDTH + old_cursor_x * VESA_CHAR_WIDTH;
    //     int char_y      = WIN_TITLEHEIGHT + WIN_BORDERWIDTH + old_cursor_y * VESA_LINE_HEIGHT;
    //     auto dirty_rect = rect_new(char_y, char_x, char_y + VESA_LINE_HEIGHT - 1, char_x + VESA_CHAR_WIDTH - 1);
    //     list_add(dirty_regions, dirty_rect);
    // }

    // Mark new cursor position as dirty
    if (terminal->cursor_x < buffer_width && terminal->cursor_y < buffer_height) {
        int char_x      = WIN_BORDERWIDTH + old_cursor_x * VESA_CHAR_WIDTH + terminal->window.x;
        int char_y      = WIN_TITLEHEIGHT + WIN_BORDERWIDTH + old_cursor_y * VESA_LINE_HEIGHT + terminal->window.y - 2;
        int bottom      = char_y + VESA_CHAR_HEIGHT;
        int right       = char_x + VESA_CHAR_WIDTH;
        auto dirty_rect = rect_new(char_y, char_x, bottom, right);
        list_add(dirty_regions, dirty_rect);
        // context_draw_rect(
        //     terminal->window.context, char_x - 1, char_y - 1, VESA_CHAR_WIDTH + 1, VESA_CHAR_HEIGHT + 1, 0xFFFF0000);
    }


    window_paint((window_t *)terminal, dirty_regions, 1);
}

void spawn_calculator([[maybe_unused]] button_t *button, [[maybe_unused]] int x, [[maybe_unused]] int y)
{
    calculator_t *temp_calc = calculator_new();
    window_insert_child((window_t *)desktop, (window_t *)temp_calc);
    window_move((window_t *)temp_calc, button->window.context->width / 2, button->window.context->height / 2);
}

void spawn_terminal()
{
    terminal = vterm_new();
    window_insert_child((window_t *)desktop, (window_t *)terminal);
    window_move((window_t *)terminal, 0, 0);
    vesa_terminal_init(putchar_handler);
}

void kernel_main(const multiboot_info_t *mbd, const uint32_t magic)
{
    cli();

#ifdef PIXEL_RENDERING
    set_vbe_info(mbd);
#endif

    init_serial();

#ifndef PIXEL_RENDERING
    vga_buffer_init();
#endif

    gdt_init();

    init_symbols(mbd);
    display_grub_info(mbd, magic);
    paging_init();
    pic_init();
    idt_init();
    pit_init();
    timer_init(1000);
    threads_init();
    vfs_init();
    pci_scan();
    disk_init();
    root_inode_init();
    register_syscalls();
    keyboard_init();

    struct thread *idle_task = thread_allocate(idle, TASK_READY, "idle", KERNEL_MODE);
    set_idle_thread(idle_task);
    wait_for_network();

#ifdef PIXEL_RENDERING
    video_context_t *context = context_new(vbe_info->width, vbe_info->height);
    desktop                  = desktop_new(context);
    button_t *launch_button  = button_new(10, 10, 150, 30);
    window_set_title((window_t *)launch_button, "New Calculator");
    launch_button->onmousedown = spawn_calculator;
    window_insert_child((window_t *)desktop, (window_t *)launch_button);

    window_paint((window_t *)desktop, nullptr, 1);

    spawn_terminal();
    mouse_init(main_mouse_event_handler);
#else
    // start_shell();
#endif
    // vesa_terminal_init(putchar_handler);
    start_shell();

    scheduler();

    panic("Kernel terminated");
}

void start_shell()
{
    struct process *process = nullptr;
    int res                 = process_load_enqueue("/bin/sh", &process);
    if (res < 0) {
        panic("Failed to load shell");
    }

    res = process_set_current_directory(process, "/");
    if (res < 0) {
        panic("Failed to set current directory");
    }

    process->priority      = 1;
    process->thread->state = TASK_READY;
}


void display_grub_info(const multiboot_info_t *mbd, const unsigned int magic)
{
#ifdef GRUB
    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        panic("invalid magic number!");
    }

    if (mbd->flags & MULTIBOOT_INFO_FRAMEBUFFER_INFO) {
        printf("[ " KBGRN "INFO" KWHT " ] Framebuffer info available\n");
        printf("[ " KBGRN "INFO" KWHT " ] Type: %d (0=indexed, 1=RGB, 2=EGA text)\n", mbd->framebuffer_type);
    } else {
        printf("[ " KBYEL "WARN" KWHT " ] No framebuffer info from GRUB\n");
    }

    /* Check bit 6 to see if we have a valid memory map */
    if (!(mbd->flags >> 6 & 0x1)) {
        panic("invalid memory map given by GRUB bootloader");
    }

    /* Loop through the memory map and display the values */
    for (unsigned int i = 0; i < mbd->mmap_length; i += sizeof(multiboot_memory_map_t)) {
        const multiboot_memory_map_t *mmmt = (multiboot_memory_map_t *)(mbd->mmap_addr + i);

        const uint32_t type = mmmt->type;

        if (type == MULTIBOOT_MEMORY_AVAILABLE) {
            if (mmmt->len > 0x100000) {
                printf("[ " KBGRN "OK" KWHT " ] ");
                printf("Available memory: %u MiB\n", (uint16_t)(mmmt->len / 1024 / 1024));
                kernel_heap_init(mmmt->len);
            }
        }
    }
#endif
}

void system_reboot()
{
    uint8_t good = 0x02;
    while (good & 0x02)
        good = inb(0x64);
    outb(0x64, 0xFE);
}

void system_shutdown()
{
    outw(0x604, 0x2000);

    hlt();
}
