// mode13.c - A demo of all the mode 13h functions for this chapter

#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <bios.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

#include "black3.h"  // the header file for this module

void main(int argc, char** argv) {
    RgbColor color;
    int index;
    RgbPalette savePalette;

    srand(time(NULL));

    printf("\nHit any key to switch to mode 13h\n");
    getch();

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // show some text
    printString(0, 0, 15, "Hit a key to see text printing          ", 0);
    getch();

    // print a few hundred strings on the screen
    for (index = 0; index < 1000; index++) {
        printString(
            rand() % (320 - 256),
            rand() % 190,
            rand() % 256,
            "This is a demo of text printing",
            1);
    }

    printString(0, 0, 15, "Hit a key to see screen filling         ", 0);
    getch();

    // fill the screen dark grey
    fillScreen(8);
    printString(0, 0, 15, "Hit a key to see pixel plotting         ", 0);
    getch();

    // plot 10000 random pixels
    for (index = 0; index < 10000; index++) {
        writePixel(rand() % 320, rand() % 200, 12);
    }

    printString(0, 0, 15, "Hit a key to see lines                  ", 0);
    getch();

    // draw 1000 randomly positioned horizontal and vertical lines
    for (index = 0; index < 1000; index++) {
        lineH(rand() % 320, rand() % 320, rand() % 200, rand() % 256);
        lineV(rand() % 200, rand() % 200, rand() % 320, rand() % 256);
    }

    printString(0, 0, 15, "Hit a key to change color registers     ", 0);
    getch();

    // save the palette
    readPalette(0, 255, &savePalette);

    // change the palette
    for (index = 0; index < 256; index++) {
        // set the color to bright green
        color.red   = 0;
        color.green = 63;
        color.blue  = 0;

        // change the currently index color register
        writeColorReg(index, &color);

        // let user see it happen
        timeDelay(1);
    }

    // make color 15 visible so user can read text
    color.red   = 63;
    color.green = 63;
    color.blue  = 63;
    writeColorReg(15, &color);

    printString(0, 0, 15, "Hit a key to restore palette            ", 0);
    getch();

    // restore palette
    writePalette(&savePalette);

    printString(0, 0, 15, "Hit a key to switch back to text mode   ", 0);
    getch();

    // restore graphics mode to text
    setGraphicsMode(TEXT_MODE);
}

