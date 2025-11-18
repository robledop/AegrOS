#include "framebuffer.h"
#include "file.h"
#include "string.h"
#include "font.h"
#include "defs.h"
#include "x86.h"
#include "vga_terminal.h"
#include "multiboot.h"
#include "proc.h"


static struct vbe_mode_info vbe_info_;
struct vbe_mode_info *vbe_info = &vbe_info_;
extern struct page_directory *kernel_page_directory;
extern struct devsw devsw[];
static bool framebuffer_wc_enabled;
static bool configure_framebuffer_pat_entry(void);
static void *framebuffer_map_with_pat(u32 fb_addr, u32 fb_size);


void framebuffer_enable_write_combining(void)
{
    framebuffer_wc_enabled = true;
}

bool framebuffer_write_combining_enabled(void)
{
    return framebuffer_wc_enabled;
}

void framebuffer_set_vbe_info(const multiboot_info_t *mbd)
{
    if (mbd == nullptr) {
        memset(vbe_info, 0, sizeof(*vbe_info));
        vga_enter_text_mode();
        return;
    }

    vbe_info->height              = mbd->framebuffer_height;
    vbe_info->width               = mbd->framebuffer_width;
    vbe_info->bpp                 = mbd->framebuffer_bpp;
    vbe_info->pitch               = mbd->framebuffer_pitch;
    vbe_info->framebuffer         = (u32)mbd->framebuffer_addr;
    vbe_info->framebuffer_virtual = 0;

    if (vbe_info->framebuffer == 0 || vbe_info->pitch == 0 || vbe_info->height == 0) {
        memset(vbe_info, 0, sizeof(*vbe_info));
        boot_message(WARNING_LEVEL_WARNING,
                     "Bootloader did not provide a usable framebuffer; disabling graphics console");
        vga_enter_text_mode();
        return;
    }
}

bool framebuffer_map_boot_framebuffer(struct cpu *bsp)
{
    const u32 fb_phys = vbe_info->framebuffer;
    if (fb_phys == 0 || vbe_info->pitch == 0 || vbe_info->height == 0) {
        return false;
    }

    const u32 fb_size = (u32)vbe_info->pitch * (u32)vbe_info->height;
    if (fb_size == 0) {
        return false;
    }

    void *fb_virt = framebuffer_map_with_pat(fb_phys, fb_size);
    if (fb_virt == nullptr) {
        fb_virt = kernel_map_mmio(fb_phys, fb_size);
    }
    if (fb_virt == nullptr) {
        boot_message(WARNING_LEVEL_WARNING, "Failed to map framebuffer; graphics console disabled");
        vga_enter_text_mode();
        return false;
    }
    vbe_info->framebuffer_virtual = (u32)(uptr)fb_virt;
    framebuffer_prepare_cpu(bsp);
    boot_message(WARNING_LEVEL_INFO,
                 "Framebuffer at 0x%x: %ux%u, %u bpp, pitch %u",
                 fb_phys,
                 vbe_info->width,
                 vbe_info->height,
                 vbe_info->bpp,
                 vbe_info->pitch);
    return true;
}

void framebuffer_prepare_cpu(struct cpu *cpu)
{
    if (cpu == nullptr) {
        return;
    }
    if (!framebuffer_write_combining_enabled()) {
        cpu->pat_wc_ready = true;
        return;
    }
    cpu->pat_wc_ready = configure_framebuffer_pat_entry();
}

int framebuffer_write(struct inode *ip, char *buf, int n, u32 offset)
{
    ip->iops->iunlock(ip);

    u8 *fb      = framebuffer_kernel_bytes();
    if (fb == nullptr) {
        ip->iops->ilock(ip);
        return 0;
    }
    u32 pitch   = vbe_info->pitch;
    u32 fb_size = pitch * vbe_info->height;

    if (offset >= fb_size) {
        ip->iops->ilock(ip);
        return 0;
    }

    if (offset + (u32)n > fb_size) {
        n = (int)(fb_size - offset);
    }

    memcpy(fb + offset, buf, (u32)n);
    ip->iops->ilock(ip);

    return n;
}

void framebuffer_init()
{
    devsw[FRAMEBUFFER].write = framebuffer_write;
    // devsw[FRAMEBUFFER].read  = consoleread;
}

static inline int framebuffer_supports_32bpp(void)
{
    return vbe_info->bpp == 32;
}

static inline void framebuffer_fill_span32_scalar(u8 *dst, u32 pixel_count, u32 color)
{
    while (pixel_count--) {
        *((u32 *)dst) = color;
        dst += 4;
    }
}

static inline void framebuffer_copy_span32_scalar(u8 *dst, const u32 *src, u32 pixel_count)
{
    while (pixel_count--) {
        *((u32 *)dst) = *src++;
        dst += 4;
    }
}

static inline void framebuffer_fill_span32(u8 *dst, u32 pixel_count, u32 color)
{
    if (pixel_count == 0) {
        return;
    }

    if (!memory_sse_available()) {
        framebuffer_fill_span32_scalar(dst, pixel_count, color);
        return;
    }

    u32 remaining      = pixel_count;
    u32 pattern[4] = { color, color, color, color };
    const u32 saved_cr0 = rcr0();
    clts();
    __asm__ volatile("movdqu (%0), %%xmm0" : : "r"(pattern));

    while (remaining >= 4) {
        __asm__ volatile("movdqu %%xmm0, (%0)" : : "r"(dst) : "memory");
        dst += 16;
        remaining -= 4;
    }
    lcr0(saved_cr0);

    if (remaining) {
        framebuffer_fill_span32_scalar(dst, remaining, color);
    }
}

static inline void framebuffer_copy_span32(u8 *dst, const u32 *src, u32 pixel_count)
{
    if (pixel_count == 0) {
        return;
    }

    if (!memory_sse_available()) {
        framebuffer_copy_span32_scalar(dst, src, pixel_count);
        return;
    }

    u32 remaining       = pixel_count;
    const u8 *src_bytes = (const u8 *)src;
    u8 *dst_bytes       = dst;

    const u32 saved_cr0 = rcr0();
    clts();
    while (remaining >= 4) {
        __asm__ volatile("movdqu (%[s]), %%xmm0\n\t"
                         "movdqu %%xmm0, (%[d])"
                         :
                         : [s] "r"(src_bytes), [d] "r"(dst_bytes)
                         : "memory");
        src_bytes += 16;
        dst_bytes += 16;
        remaining -= 4;
    }
    lcr0(saved_cr0);

    if (remaining) {
        framebuffer_copy_span32_scalar(dst_bytes, (const u32 *)src_bytes, remaining);
    }
}

static bool configure_framebuffer_pat_entry(void)
{
    u32 eax, ebx, ecx, edx;
    cpuid(0x01, &eax, &ebx, &ecx, &edx);
    if ((edx & CPUID_FEAT_EDX_PAT) == 0) {
        return false;
    }

    u64 pat           = rdmsr(MSR_IA32_PAT);
    const u64 wc_mask = 0xFFull << 8; // PAT entry 1 (PWT=1, PCD=0)
    const u64 wc_val  = 0x01ull << 8; // Write-combining encoding
    if ((pat & wc_mask) != wc_val) {
        pat = (pat & ~wc_mask) | wc_val;
        wrmsr(MSR_IA32_PAT, pat);
    }
    return true;
}

static void *framebuffer_map_with_pat(u32 fb_addr, u32 fb_size)
{
    if (!configure_framebuffer_pat_entry()) {
        return nullptr;
    }

    framebuffer_enable_write_combining();
    return kernel_map_mmio_wc(fb_addr, fb_size);
}

static inline void framebuffer_zero_span(u8 *dst, u32 byte_count)
{
    if (byte_count == 0) {
        return;
    }

    if (!memory_sse_available()) {
        memset(dst, 0, byte_count);
        return;
    }

    u32 remaining       = byte_count;
    const u32 saved_cr0 = rcr0();
    clts();
    __asm__ volatile("pxor %xmm0, %xmm0");
    while (remaining >= 16) {
        __asm__ volatile("movdqu %%xmm0, (%0)" : : "r"(dst) : "memory");
        dst += 16;
        remaining -= 16;
    }
    lcr0(saved_cr0);

    if (remaining) {
        memset(dst, 0, remaining);
    }
}

/**
 * @brief Fill a rectangle in the framebuffer using 32-bit pixels when possible.
 */
void framebuffer_fill_rect32(int x, int y, int width, int height, u32 color)
{
    if (width <= 0 || height <= 0) {
        return;
    }

    if (!framebuffer_supports_32bpp()) {
        for (int j = 0; j < height; j++) {
            for (int i = 0; i < width; i++) {
                framebuffer_putpixel(x + i, y + j, color);
            }
        }
        return;
    }

    const int screen_w = (int)vbe_info->width;
    const int screen_h = (int)vbe_info->height;

    int x0 = x;
    int y0 = y;
    int x1 = x + width;
    int y1 = y + height;

    if (x1 <= 0 || y1 <= 0 || x0 >= screen_w || y0 >= screen_h) {
        return;
    }

    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > screen_w) {
        x1 = screen_w;
    }
    if (y1 > screen_h) {
        y1 = screen_h;
    }

    u32 span_pixels = (u32)(x1 - x0);
    u8 *fb = framebuffer_kernel_bytes();
    if (fb == nullptr) {
        return;
    }
    u8 *row = fb + (u32)y0 * vbe_info->pitch + (u32)x0 * 4U;

    for (int j = y0; j < y1; j++) {
        framebuffer_fill_span32(row, span_pixels, color);
        row += vbe_info->pitch;
    }
}

/**
 * @brief Blit a horizontal span of 32-bit pixels to the framebuffer.
 */
void framebuffer_blit_span32(int x, int y, const u32 *src, u32 pixel_count)
{
    if (!src || pixel_count == 0) {
        return;
    }

    if (!framebuffer_supports_32bpp()) {
        for (u32 i = 0; i < pixel_count; i++) {
            framebuffer_putpixel(x + (int)i, y, src[i]);
        }
        return;
    }

    const int screen_w = (int)vbe_info->width;
    const int screen_h = (int)vbe_info->height;

    if (y < 0 || y >= screen_h) {
        return;
    }

    int x0     = x;
    u32 offset = 0;
    if (x0 < 0) {
        offset = (u32)(-x0);
        if (offset >= pixel_count) {
            return;
        }
        pixel_count -= offset;
        x0 = 0;
    }

    if (x0 >= screen_w) {
        return;
    }

    if (x0 + (int)pixel_count > screen_w) {
        pixel_count = (u32)(screen_w - x0);
    }

    u8 *fb = framebuffer_kernel_bytes();
    if (fb == nullptr) {
        return;
    }
    u8 *row = fb + (u32)y * vbe_info->pitch + (u32)x0 * 4U;
    framebuffer_copy_span32(row, src + offset, pixel_count);
}

/**
 * @brief Write a single pixel to the framebuffer.
 */
void framebuffer_putpixel(int x, int y, u32 rgb)
{
    if (x < 0 || x >= vbe_info->width || y < 0 || y >= vbe_info->height) {
        return; // Out of bounds
    }

    auto framebuffer    = framebuffer_kernel_bytes();
    if (framebuffer == nullptr) {
        return;
    }
    u32 bytes_per_pixel = vbe_info->bpp / 8;
    u8 *pixel           = framebuffer + (y * vbe_info->pitch) + (x * bytes_per_pixel);
    *((u32 *)pixel)     = rgb;
}

/**
 * @brief Read a single pixel from the framebuffer.
 */
u32 framebuffer_getpixel(int x, int y)
{
    if (x < 0 || x >= vbe_info->width || y < 0 || y >= vbe_info->height) {
        return 0; // Out of bounds
    }
    auto framebuffer    = framebuffer_kernel_bytes();
    if (framebuffer == nullptr) {
        return 0;
    }
    u32 bytes_per_pixel = vbe_info->bpp / 8;
    u8 *pixel           = framebuffer + (y * vbe_info->pitch) + (x * bytes_per_pixel);
    return *((u32 *)pixel);
}


/**
 * @brief Draw a 32x32 monochrome icon to the framebuffer.
 */
void framebuffer_puticon32(int x, int y, const unsigned char *icon)
{
    for (int j = 0; j < 32; j++) {
        for (int i = 0; i < 32; i++) {
            u8 color_index = icon[j * 32 + i];
            if (color_index != 0xfa) {
                // Assuming 0xfa is the transparent color
                u8 r    = color_index;
                u8 g    = color_index;
                u8 b    = color_index;
                u32 rgb = (r << 16) | (g << 8) | b;
                framebuffer_putpixel(x + i, y + j, rgb);
            }
        }
    }
}

/**
 * @brief Draw a 32x32 ARGB bitmap to the framebuffer.
 */
void framebuffer_put_bitmap_32(int x, int y, const unsigned int *icon)
{
    if (!icon) {
        return;
    }

    const int icon_w = 32;
    const int icon_h = 32;

    if (framebuffer_supports_32bpp() && x >= 0 && y >= 0 && x + icon_w <= (int)vbe_info->width &&
        y + icon_h <= (int)vbe_info->height) {
        u8 *fb = framebuffer_kernel_bytes();
        if (fb == nullptr) {
            return;
        }
        u8 *row = fb + (u32)y * vbe_info->pitch + (u32)x * 4U;
        for (int j = 0; j < icon_h; j++) {
            const unsigned int *src_row = icon + (u32)j * (u32)icon_w;
            framebuffer_copy_span32(row, (const u32 *)src_row, (u32)icon_w);
            row += vbe_info->pitch;
        }
        return;
    }

    for (int j = 0; j < icon_h; j++) {
        for (int i = 0; i < icon_w; i++) {
            u32 color = icon[j * icon_w + i];
            framebuffer_putpixel(x + i, y + j, color);
        }
    }
}

/**
 * @brief Draw a 16x16 monochrome icon to the framebuffer.
 */
void framebuffer_put_black_and_white_icon16(int x, int y, const unsigned char *icon)
{
    for (int j = 0; j < 16; j++) {
        for (int i = 0; i < 16; i++) {
            u8 pixel = icon[j * 16 + i];
            if (pixel == 1) {
                framebuffer_putpixel(x + i, y + j, 0xFFFFFF);
            }
        }
    }
}

/**
 * @brief Scroll the framebuffer contents up by one text line.
 */
void framebuffer_scroll_up()
{
    u8 *framebuffer = framebuffer_kernel_bytes();
    if (framebuffer == nullptr) {
        return;
    }
    // u32 bytes_per_pixel = vbe_info->bpp / 8;
    u32 pitch  = vbe_info->pitch;
    u32 height = vbe_info->height;

    memmove(framebuffer, framebuffer + pitch * VESA_LINE_HEIGHT, pitch * (height - VESA_LINE_HEIGHT));

    framebuffer_zero_span(framebuffer + pitch * (height - VESA_LINE_HEIGHT), pitch * VESA_LINE_HEIGHT);
}

/**
 * @brief Clear the entire screen to a solid colour.
 */
void framebuffer_clear_screen(u32 color)
{
    if (framebuffer_kernel_bytes() == nullptr) {
        return;
    }

    framebuffer_fill_rect32(0, 0, (int)vbe_info->width, (int)vbe_info->height, color);
}

/**
 * @brief Render an 8x8 character glyph at the specified position.
 */
void framebuffer_put_char8(unsigned char c, int x, int y, u32 color, u32 bg)
{
    if (c > 128) {
        return;
    }

    framebuffer_fill_rect32(x, y, VESA_CHAR_WIDTH, VESA_LINE_HEIGHT, bg);

    if (!framebuffer_supports_32bpp()) {
        for (int l = 0; l < 8; l++) {
            for (int i = 0; i < 8; i++) {
                if (font8x8[c][l] & (1 << i)) {
                    framebuffer_putpixel(x + i, y + l, color);
                }
            }
        }
        return;
    }

    u8 *fb = framebuffer_kernel_bytes();
    if (fb == nullptr) {
        return;
    }
    u8 *row = fb + (u32)y * vbe_info->pitch + (u32)x * 4U;

    for (int l = 0; l < 8; l++) {
        u8 mask    = font8x8[c][l];
        if (mask == 0) {
            row += vbe_info->pitch;
            continue;
        }
        u32 *dst32 = (u32 *)row;
        while (mask) {
            int bit = __builtin_ctz(mask);
            dst32[bit] = color;
            mask &= mask - 1;
        }
        row += vbe_info->pitch;
    }
}

/**
 * @brief Print a string using 8x8 glyphs starting at the given position.
 */
void framebuffer_print_string(const char *str, int len, int x, int y, u32 color, u32 bg)
{
    for (int i = 0; i < len; i++) {
        framebuffer_put_char8(str[i], x, y, color, bg);
        x += VESA_CHAR_WIDTH;
    }
}

/**
 * @brief Render a magnified 16x16 character glyph.
 */
void framebuffer_put_char16(unsigned char c, int x, int y, u32 color)
{
    for (int l = 0; l < 16; l++) {
        for (int i = 15; i >= 0; i--) {
            if (font8x8[c][l / 2] & (1 << (i / 2))) {
                framebuffer_putpixel((x) + i, (y) + l, color);
            }
        }
    }
}

/**
 * @brief Draw a Bresenham line between two points.
 */
void framebuffer_draw_line(int x1, int y1, int x2, int y2, u32 color)
{
    int dx = x2 - x1;
    int dy = y2 - y1;
    int d  = 2 * dy - dx;
    int y  = y1;
    int x  = x1;
    framebuffer_putpixel(x, y, color);
    while (x < x2) {
        if (d >= 0) {
            y++;
            d -= 2 * dx;
        }
        x++;
        d += 2 * dy;
        framebuffer_putpixel(x, y, color);
    }

    d = 2 * dx - dy;
    x = x2;
    while (y < y2) {
        if (d >= 0) {
            x--;
            d -= 2 * dy;
        }

        y++;
        d += 2 * dx;
        framebuffer_putpixel(x, y, color);
    }
}

/**
 * @brief Placeholder for drawing a text cursor.
 */
void framebuffer_draw_cursor(int x, int y)
{
    int top = y + VESA_CHAR_HEIGHT - 1;
    if (top < y) {
        top = y;
    }
    framebuffer_fill_rect32(x,
                            top,
                            VESA_CHAR_WIDTH,
                            1,
                            0xFFFFFF);
}

/**
 * @brief Placeholder for erasing a text cursor.
 */
void framebuffer_erase_cursor(int x, int y)
{
    int top = y + VESA_CHAR_HEIGHT - 1;
    if (top < y) {
        top = y;
    }
    framebuffer_fill_rect32(x,
                            top,
                            VESA_CHAR_WIDTH,
                            1,
                            0x000000);
}
