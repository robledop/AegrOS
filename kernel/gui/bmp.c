#include <gui/bmp.h>
#include <kernel.h>
#include <kernel_heap.h>
#include <vfs.h>

enum { BI_RGB = 0, BI_BITFIELDS = 3 };

int bitmap_load_argb(const char *path, uint32_t **out_pixels)
{
    const int fd = vfs_open(nullptr, path, O_RDONLY);
    if (fd == -1) {
        return -1;
    }

    struct stat stat;
    vfs_stat(nullptr, fd, &stat);

    if (stat.st_size < sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER)) {
        vfs_close(nullptr, fd);
        return -1;
    }

    uint8_t *buffer = kzalloc(stat.st_size * sizeof(uint8_t));

    vfs_read(nullptr, buffer, stat.st_size, 1, fd);
    vfs_close(nullptr, fd);

    BITMAPFILEHEADER *fh = (BITMAPFILEHEADER *)buffer;
    BITMAPINFOHEADER ih  = *(BITMAPINFOHEADER *)(buffer + sizeof(BITMAPFILEHEADER));

    if (fh->bfType != 0x4D42) {
        panic("Not a BMP file\n");
    }

    if (ih.biPlanes != 1) {
        panic("Not a BMP file\n");
    }

    if (ih.biBitCount != 24) {
        panic("Unsupported BMP format\n");
    }

    if (!(ih.biCompression == BI_RGB || (ih.biCompression == BI_BITFIELDS && ih.biBitCount == 32))) {
        panic("Unsupported BMP format\n");
    }

    const int width    = ih.biWidth;
    const int height   = (ih.biHeight > 0) ? ih.biHeight : -ih.biHeight;
    const int top_down = (ih.biHeight < 0);

    *out_pixels = (uint32_t *)kzalloc(sizeof(uint32_t) * width * height);

    if (ih.biBitCount == 24) {
        const int bytes_per_row = ((width * 3 + 3) / 4) * 4; // padded to 4

        for (int y = 0; y < height; y++) {
            uint8_t *row = buffer + fh->bfOffBits + y * bytes_per_row;

            int destY = top_down ? y : (height - 1 - y);
            for (int x = 0; x < width; x++) {
                uint8_t B = row[x * 3 + 0];
                uint8_t G = row[x * 3 + 1];
                uint8_t R = row[x * 3 + 2];
                (*out_pixels)[destY * width + x] =
                    (0xFFu << 24) | ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B;
            }
        }
    }

    kfree(buffer);

    return 0;
}
