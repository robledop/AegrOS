#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bmp.h"

enum { BI_RGB = 0, BI_BITFIELDS = 3 };

static inline int mask_shift(uint32_t mask)
{
    if (mask == 0) {
        return -1;
    }
    int s = 0;
    while ((mask & 1u) == 0u) {
        mask >>= 1;
        s++;
    }
    return s;
}

static inline uint8_t extract_chan(uint32_t px, uint32_t mask)
{
    if (!mask) {
        return 0;
    }
    int s = mask_shift(mask);

    if (s < 0) {
        return 0;
    }
    uint32_t v = (px & mask) >> s;
    // normalize to 8-bit if mask wider than 8 bits
    int width  = 0;
    uint32_t m = mask >> s;
    while (m) {
        width++;
        m >>= 1;
    }
    if (width == 8) {
        return (uint8_t)v;
    }
    if (width == 0) {
        return 0;
    }
    // scale to 8 bits: v * 255 / ((1<<width)-1)
    uint32_t maxv = (1u << width) - 1u;
    return (uint8_t)((v * 255u + maxv / 2u) / maxv);
}

int load_bmp_argb(const char *path, uint32_t *out_pixels)
{
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("open");
        return -1;
    }

    BITMAPFILEHEADER fh;
    BITMAPINFOHEADER ih;

    if (fread(&fh, sizeof fh, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }
    if (fh.bfType != 0x4D42) {
        fclose(fp);
        return -1;
    } // not 'BM'
    if (fread(&ih, sizeof ih, 1, fp) != 1) {
        fclose(fp);
        return -1;
    }

    if (ih.biPlanes != 1) {
        fclose(fp);
        return -1;
    }
    // if (ih.biWidth != 32 || (ih.biHeight != 32 && ih.biHeight != -32)) {
    //     fclose(fp);
    //     return -1;
    // }
    if (!(ih.biBitCount == 24 || ih.biBitCount == 32)) {
        fclose(fp);
        return -1;
    }
    if (!(ih.biCompression == BI_RGB || (ih.biCompression == BI_BITFIELDS && ih.biBitCount == 32))) {
        fclose(fp);
        return -1;
    }

    // Default masks for 32bpp BI_RGB are BGRA in little-endian files: B=0x000000FF, G=0x0000FF00, R=0x00FF0000,
    // A=0xFF000000 (often A is unused/zero).
    uint32_t rmask = 0x00FF0000, gmask = 0x0000FF00, bmask = 0x000000FF, amask = 0xFF000000;

    // If BI_BITFIELDS and 32bpp, masks follow the info header (for V3+). For V4/V5, masks are inside the extended
    // header, but many files still have the three masks immediately after the 40-byte header.
    if (ih.biBitCount == 32 && ih.biCompression == BI_BITFIELDS) {
        // The spec places masks immediately after the 40-byte header for V3.
        // If biSize > 40 (V4/V5), the masks are part of the larger header. We must seek back to the start of masks
        // appropriately.
        long after_info         = ftell(fp);
        long expected_masks_pos = (long)sizeof(BITMAPFILEHEADER) + (long)ih.biSize; // start of masks per header size
        if (ih.biSize >= 40 && after_info < expected_masks_pos) {
            fseek(fp, expected_masks_pos, SEEK_SET);
        }
        if (fread(&rmask, 4, 1, fp) != 1 || fread(&gmask, 4, 1, fp) != 1 || fread(&bmask, 4, 1, fp) != 1) {
            fclose(fp);
            return -1;
        }
        // Alpha mask may follow (V3 optional, V4/V5 present). If available and room before pixel data, read it.
        long pos = ftell(fp);
        if ((long)fh.bfOffBits - pos >= 4) {
            fread(&amask, 4, 1, fp); // if not present, this reads part of gap; acceptable if we correct with seek later
        }
    }

    // Seek to pixel array
    if (fseek(fp, (long)fh.bfOffBits, SEEK_SET) != 0) {
        fclose(fp);
        return -1;
    }

    const int width    = ih.biWidth;
    const int height   = (ih.biHeight > 0) ? ih.biHeight : -ih.biHeight;
    const int top_down = (ih.biHeight < 0);

    out_pixels = (uint32_t *)malloc(sizeof(uint32_t) * width * height);
    memset(out_pixels, 0, sizeof(uint32_t) * width * height);

    if (ih.biBitCount == 24) {
        const int bytes_per_row = ((width * 3 + 3) / 4) * 4; // padded to 4
        uint8_t *row            = (uint8_t *)malloc(bytes_per_row);
        if (!row) {
            fclose(fp);
            return -1;
        }

        for (int y = 0; y < height; y++) {
            if (fread(row, 1, bytes_per_row, fp) != (size_t)bytes_per_row) {
                free(row);
                fclose(fp);
                return -1;
            }
            int destY = top_down ? y : (height - 1 - y);
            for (int x = 0; x < width; x++) {
                uint8_t B                     = row[x * 3 + 0];
                uint8_t G                     = row[x * 3 + 1];
                uint8_t R                     = row[x * 3 + 2];
                out_pixels[destY * width + x] = (0xFFu << 24) | ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B;
            }
        }
        free(row);
    } else { // 32-bit
        // For 32bpp BI_RGB, rows are tightly packed (width*4), 4-byte aligned already.
        const int bytes_per_row = width * 4;
        uint8_t *row            = (uint8_t *)malloc(bytes_per_row);
        if (!row) {
            fclose(fp);
            return -1;
        }

        for (int y = 0; y < height; y++) {
            if (fread(row, 1, bytes_per_row, fp) != (size_t)bytes_per_row) {
                free(row);
                fclose(fp);
                return -1;
            }
            int destY = top_down ? y : (height - 1 - y);

            // If BI_RGB with the common BGRA layout, the masks above already match (B,G,R,A).
            // If BI_BITFIELDS, we use the masks we read.
            for (int x = 0; x < width; x++) {
                uint32_t px = ((uint32_t *)row)[x]; // little-endian
                uint8_t R   = extract_chan(px, rmask);
                uint8_t G   = extract_chan(px, gmask);
                uint8_t B   = extract_chan(px, bmask);
                uint8_t A   = amask ? extract_chan(px, amask) : 0xFF; // if no alpha mask, force opaque
                out_pixels[destY * width + x] =
                    ((uint32_t)A << 24) | ((uint32_t)R << 16) | ((uint32_t)G << 8) | (uint32_t)B;
            }
        }
        free(row);
    }

    fclose(fp);
    return 0;
}

void print_row(uint32_t row[32])
{
    for (int x = 0; x < 32; x++) {
        uint32_t px = row[x];
        uint8_t A   = (px >> 24) & 0xFF;
        char c;

        if (A == 0x00) {
            c = ' '; // fully transparent
        } else if (A < 0x80) {
            c = '.'; // semi-transparent
        } else if (A < 0xFF) {
            c = '*'; // nearly opaque
        } else {
            c = '#'; // fully opaque
        }

        putchar(c);
    }
    putchar('\n');
}

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s file.bmp\n", argv[0]);
        return 1;
    }
    uint32_t *pixels;
    if (load_bmp_argb(argv[1], pixels) != 0) {
        fprintf(stderr, "Failed to load 32x32 BMP.\n");
        return 1;
    }

    // printf("{");
    // for (int y = 0; y < 32; y++) {
    //     printf("{");
    //     for (int x = 0; x < 32; x++) {
    //         uint32_t px = pixels[y * x];
    //         printf("0x%x,", px);
    //     }
    //     printf("},");
    //     printf("\n");
    // }
    // printf("};\n");
    //
    // for (int y = 0; y < 32; y++) {
    //     print_row(pixels[y]);
    // }
    return 0;
}
