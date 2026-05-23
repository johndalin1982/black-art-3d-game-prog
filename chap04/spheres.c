// spheres.c - A demo of mode Z page flipping

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

#define PI 3.14159f

void drawSphere(int x0, int y0, int radius, int color) {
    // this function draws a sphere in the active video page
    int x1, y1, y2, // location coordinates
        angle;      // used to track current angle

    // iterate thru 90 degrees and use symmetry to draw circle
    for (angle = 0; angle < 90; angle++) {
        // draw the sphere as a collection of vertical strips
        x1 = radius * cos(PI * (float)angle / 180.0f);
        y1 = -radius * sin(PI * (float)angle / 180.0f) * 1.66f;

        // draw the next vertical strip
        for (y2 = y0 + y1; y2 < y0 - y1; y2++) {
            writePixelZ(x0 + x1, y2, color);
            writePixelZ(x0 - x1, y2, color);
        }
    }
}

void main(int argc, char** argv) {
    int index;

    // set the graphics mode to mode Z 320x400x256
    setModeZ();

    // clear out all of display memory, only page 1 was cleared during setModeZ
    setWorkingPageModeZ(PAGE_1);
    fillScreenZ(0);

    // set visual and working page to page 0
    setVisualPageModeZ(PAGE_0);
    setWorkingPageModeZ(PAGE_0);

    srand(time(NULL));

    // draw some colored spheres on this page
    for (index = 0; index < 50; index++) {
        drawSphere(20 + rand() % 280, 20 + rand() % 360, rand() % 15, 34 + rand() % 6);
    }

    // now draw grey spheres on page 1
    setWorkingPageModeZ(PAGE_1);

    for (index = 0; index < 50; index++) {
        drawSphere(20 + rand() % 280, 20 + rand() % 360, rand() % 15, 34 + rand() % 6);
    }

    // now toggle between pages
    while (!kbhit()) {
        setVisualPageModeZ(PAGE_0);
        timeDelay(5);

        setVisualPageModeZ(PAGE_1);
        timeDelay(5);
    }

    // restore the video system to text
    setGraphicsMode(TEXT_MODE);
}
