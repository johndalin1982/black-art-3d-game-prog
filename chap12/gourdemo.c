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
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black11.h"

void main(void) {
    int done = 0,
        x = 140,
        y = 60,
        intensity1 = 15,
        intensity2 = 50,
        intensity3 = 5;
    char buffer[80];

    // set graphics mode to 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // alter the palette and introduce some greys
    makeGreyPalette();

    // draw gouraud shaded triangle
    drawTriangle2DGouraud(x, y, x - 50, y + 60, x + 30, y + 80, VideoBuffer, intensity1, intensity2, intensity3);

    // label vertices
    printString(x, y - 10, 9, "1", 1);
    printString(x - 60, y + 60, 9, "2", 1);
    printString(x + 40, y + 80, 9, "3", 1);

    // main loop
    while (!done) {
        // test for key
        if (kbhit()) {
            // get the key
            switch (getch()) {
                case 27: { // escape key
                    // exit system
                    done = 1;
                } break;

                case '7': { // increase vertex 1 intensity
                    if (++intensity1 > 63) {
                        intensity1 = 63;
                    }
                } break;

                case '4': { // decrease vertex 1 intensity
                    if (--intensity1 < 0) {
                        intensity1 = 0;
                    }
                } break;

                case '8': { // increase vertex 2 intensity
                    if (++intensity2 > 63) {
                        intensity2 = 63;
                    }
                } break;

                case '5': { // decrease vertex 2 intensity
                    if (--intensity2 < 0) {
                        intensity2 = 0;
                    }
                } break;

                case '9': { // increase vertex 31 intensity
                    if (++intensity3 > 63) {
                        intensity3 = 63;
                    }
                } break;

                case '6': { // decrease vertex 3 intensity
                    if (--intensity3 < 0) {
                        intensity3 = 0;
                    }
                } break;

                default:
                    break;
            }

            // draw gouraud shaded triangle
            drawTriangle2DGouraud(
                x,
                y,
                x - 50,
                y + 60,
                x + 30,
                y + 80,
                VideoBuffer,
                intensity1,
                intensity2,
                intensity3);
        }

        // print out vertex intensities
        sprintf(buffer, "Vertex 1 = %d  ", intensity1);
        printString(0, 0, 12, buffer, 0);

        sprintf(buffer, "Vertex 2 = %d  ", intensity2);
        printString(0, 10, 12, buffer, 0);

        sprintf(buffer, "Vertex 3 = %d  ", intensity3);
        printString(0, 20, 12, buffer, 0);
    }

    // restore text mode
    setGraphicsMode(TEXT_MODE);
}

