#include <assert.h>
#include <config.h>
#include <debug.h>
#include <gdt.h>
#include <gui/bmp.h>
#include <gui/button.h>
#include <gui/calculator.h>
#include <gui/desktop.h>
#include <gui/icon.h>
#include <gui/rect.h>
#include <gui/vterm.h>
#include <gui/window.h>
#include <icons.h>
#include <idt.h>
#include <io.h>
#include <kernel.h>
#include <kernel_heap.h>
#include <keyboard.h>
#include <list.h>
#include <memory.h>
#include <mouse.h>
#include <net/network.h>
#include <paging.h>
#include <pat.h>
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

#include "fpu.h"


void display_grub_info(const multiboot_info_t *mbd, unsigned int magic);

#define STACK_CHK_GUARD 0xe2dee396
uint32_t wait_for_network_start;
uint32_t wait_for_network_timeout = 15'000;

uintptr_t __stack_chk_guard = STACK_CHK_GUARD; // NOLINT(*-reserved-identifier)

extern struct vbe_mode_info *vbe_info;
static desktop_t *desktop;
static vterm_t *terminal;
static calculator_t *calculator;

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
    terminal->putchar(terminal, c);
}

void clear_screen_handler()
{
    terminal->clear_screen(terminal);
}

void spawn_calculator([[maybe_unused]] icon_t *icon, [[maybe_unused]] int x, [[maybe_unused]] int y)
{
    if (calculator) {
        return;
    }

    calculator = calculator_new();
    window_insert_child((window_t *)desktop, (window_t *)calculator);
    window_move((window_t *)calculator, icon->window.context->width / 2, icon->window.context->height / 2);
}

void spawn_terminal(icon_t *icon, int x, int y)
{
    if (terminal) {
        return;
    }

    terminal = vterm_new();
    window_insert_child((window_t *)desktop, (window_t *)terminal);
    window_move((window_t *)terminal, 0, 0);
    vesa_terminal_init(putchar_handler, clear_screen_handler);

    start_shell();
}

void kernel_main(const multiboot_info_t *mbd, const uint32_t magic)
{
    fpu_init();
    cli();

#ifdef PIXEL_RENDERING
    set_vbe_info(mbd);
    vesa_clear_screen(DESKTOP_BACKGROUND_COLOR);
#endif

    init_serial();

#ifndef PIXEL_RENDERING
    vga_buffer_init();
#endif

    gdt_init();
    init_symbols(mbd);
    display_grub_info(mbd, magic);
#ifdef PIXEL_RENDERING
    vesa_put_bitmap_32(1024 - 40, 10, (unsigned int *)xterm);
#endif
    paging_init();
    pat_init();
#ifdef PIXEL_RENDERING
    vesa_enable_write_combining();
#endif
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

    uint32_t *pixels = nullptr;
    bitmap_load_argb("wpaper.bmp", &pixels);

    // ASSERT(pixels);


    video_context_t *context = context_new(vbe_info->width, vbe_info->height);
    desktop                  = desktop_new(context, pixels);
    mouse_init(main_mouse_event_handler);
    // button_t *launch_button = button_new(10, 10, 100, 30);
    // window_set_title((window_t *)launch_button, "Calculator");
    // launch_button->onmousedown = spawn_calculator;
    // window_insert_child((window_t *)desktop, (window_t *)launch_button);

    vesa_put_bitmap_32(900, 100, (unsigned int *)xterm);

    icon_t *term = icon_new((unsigned int *)xterm, 900, 10);
    window_set_title((window_t *)term, "Term");
    window_insert_child((window_t *)desktop, (window_t *)term);
    term->onmousedown = spawn_terminal;

    icon_t *calc = icon_new((unsigned int *)xterm, 950, 10);
    window_set_title((window_t *)calc, "Calc");
    window_insert_child((window_t *)desktop, (window_t *)calc);
    calc->onmousedown = spawn_calculator;


    window_paint((window_t *)desktop, nullptr, 1);
    // context_draw_bitmap(context, 0, 0, 1024, 768, pixels);

    // spawn_terminal();
#else
    start_shell();
#endif


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
        printf(KWHT "[ " KBGRN "INFO" KWHT " ] Framebuffer info available\n");
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
                kernel_heap_init((int)mmmt->len);
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
