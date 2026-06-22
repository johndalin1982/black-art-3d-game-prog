#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dos.h>
#include <bios.h>
#include <fcntl.h>

// ROM 8x8 character set at F000:FA6E in BIOS
#define ROM_FONT_ADDRESS    0xF000FA6EL

// all font sizes to export: font.bin (8x8) plus the scaled fontNN.bin
// variants used by the vesa demos (16=640x480, 24=800x600, 32=1024x768)
static int Sizes[] = { 8, 16, 24, 32 };
#define NUM_SIZES (sizeof(Sizes) / sizeof(Sizes[0]))

static int exportFont(unsigned char far* romCharSet, int size) {
    // write one bit-packed square-cell font file scaled from the 8x8 ROM
    // glyphs by nearest-neighbour sampling (handles non-integer scales
    // such as 8 -> 12)
    FILE* outputFile;
    int ch, row, col;
    int bpr, glyph_bytes;
    unsigned char glyphBuf[4 * 32];     // big enough for the largest size
    char outputFilename[32];

    bpr         = (size + 7) / 8;       // bytes per row (bit-packed)
    glyph_bytes = bpr * size;

    // size 8 writes font.bin (the original); others write fontNN.bin
    if (size == 8)
        strcpy(outputFilename, "font.bin");
    else
        sprintf(outputFilename, "font%d.bin", size);

    outputFile = fopen(outputFilename, "wb");
    if (!outputFile) {
        printf("Error: Could not create %s\n", outputFilename);
        return 0;
    }

    for (ch = 0; ch < 256; ch++) {
        const unsigned char far* src = romCharSet + ch * 8;
        memset(glyphBuf, 0, glyph_bytes);

        for (row = 0; row < size; row++) {
            unsigned char srcByte = src[row * 8 / size];
            for (col = 0; col < size; col++) {
                if (srcByte & (0x80 >> (col * 8 / size))) {
                    glyphBuf[row * bpr + col / 8] |= (0x80 >> (col % 8));
                }
            }
        }
        fwrite(glyphBuf, 1, glyph_bytes, outputFile);
    }

    fclose(outputFile);

    printf("  %-12s %2dx%-2d  %6d bytes\n",
           outputFilename, size, size, 256 * glyph_bytes);
    return 1;
}

int main(void) {
    unsigned char far* romCharSet;
    int index;

    printf("\nROM Character Set Extractor\n");
    printf("===========================\n\n");

    romCharSet = (unsigned char far*)ROM_FONT_ADDRESS;
    printf("Reading ROM font from F000:FA6E...\n\n");

    for (index = 0; index < NUM_SIZES; index++) {
        if (!exportFont(romCharSet, Sizes[index])) {
            return 1;
        }
    }

    printf("\nDone.\n\n");
    return 0;
}
