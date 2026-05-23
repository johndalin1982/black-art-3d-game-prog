#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <bios.h>
#include <fcntl.h>

// ROM character set is at F000:FA6E in BIOS
#define ROM_FONT_ADDRESS    0xF000FA6EL
#define FONT_SIZE           2048        // 256 characters * 8 bytes each
#define OUTPUT_FILENAME     "font.bin"

int main(void) {
    unsigned char far* romCharSet;      // pointer to ROM BIOS character set
    FILE* outputFile;                   // output file handle
    int bytesWritten;                   // number of bytes written
    int index;                          // loop counter

    // display program header
    printf("\n");
    printf("ROM Character Set Extractor\n");
    printf("===========================\n");
    printf("\n");

    // point to the ROM BIOS 8x8 character set
    romCharSet = (unsigned char far*)ROM_FONT_ADDRESS;

    printf("Reading ROM font from F000:FA6E...\n");

    // open output file for binary writing
    if ((outputFile = fopen(OUTPUT_FILENAME, "wb")) == NULL) {
        printf("Error: Could not create %s\n", OUTPUT_FILENAME);
        return 1;
    }

    printf("Writing font data to %s...\n", OUTPUT_FILENAME);

    // write the entire 2KB font to the file
    bytesWritten = 0;
    for (index = 0; index < FONT_SIZE; index++) {
        if (fputc(romCharSet[index], outputFile) == EOF) {
            printf("Error: Failed to write byte %d\n", index);
            fclose(outputFile);
            return 1;
        }
        bytesWritten++;
    }

    // close the file
    fclose(outputFile);

    // display success message
    printf("\n");
    printf("Success!\n");
    printf("--------\n");
    printf("Extracted %d bytes from ROM BIOS\n", bytesWritten);
    printf("Font data saved to: %s\n", OUTPUT_FILENAME);
    printf("\n");
    printf("You can now use this file with your DOS4GW 32-bit programs.\n");
    printf("\n");

    return 0;
}
