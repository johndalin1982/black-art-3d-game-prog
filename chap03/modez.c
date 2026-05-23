// modez.c - A demo of mode Z (320x400x256)

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

#include "black3.h"

void main(int argc, char** argv) {
    int index,
        x,
        y,
        color;

    // set the graphics mode to mode Z 320x400x256
    setModeZ();

    // fill the screen with dark grey
    fillScreenZ(8);
    getch();

    srand(time(NULL));

    // plot 1000 pixels in each of the colors
    for (index = 0; index < 1000; index++) {
        for (color = 1; color < 256; color++) {
            writePixelZ(rand() % 320, rand() % 400, color);
        }
    }

    getch();

    // wipe screen
    for (x = 320; x >= 0; x--) {
        for (y = 0; y < 400; y++) {
            writePixelZ(x, y, 0);
        }
    }

    // restore the video system to text
    setGraphicsMode(TEXT_MODE);
}
