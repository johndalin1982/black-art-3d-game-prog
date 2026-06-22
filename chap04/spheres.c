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

#ifdef VBE_SUPPORT
#define NUM_SPHERES 50

int Sphere0X[NUM_SPHERES], Sphere0Y[NUM_SPHERES],
    Sphere0Radius[NUM_SPHERES], Sphere0Color[NUM_SPHERES];

int Sphere1X[NUM_SPHERES], Sphere1Y[NUM_SPHERES],
    Sphere1Radius[NUM_SPHERES], Sphere1Color[NUM_SPHERES];
#endif

void drawSphere(int x0, int y0, int radius, int color) {
    // this function draws a sphere in the active video page
    int x1, y1, y2, // location coordinates
        angle;      // used to track current angle

    // iterate thru 90 degrees and use symmetry to draw circle
    for (angle = 0; angle < 90; angle++) {
        // draw the sphere as a collection of vertical strips
#ifdef VBE_SUPPORT
        x1 = (int)(radius * cos(PI * (float)angle / 180.0f));
        y1 = (int)(-radius * sin(PI * (float)angle / 180.0f));
#else
        x1 = radius * cos(PI * (float)angle / 180.0f);
        y1 = -radius * sin(PI * (float)angle / 180.0f) * 1.66f;
#endif

        // draw the next vertical strip
        for (y2 = y0 + y1; y2 < y0 - y1; y2++) {
#ifdef VBE_SUPPORT
            writePixelDb(x0 + x1, y2, color);
            writePixelDb(x0 - x1, y2, color);
#else
            writePixelZ(x0 + x1, y2, color);
            writePixelZ(x0 - x1, y2, color);
#endif
        }
    }
}

void main(int argc, char** argv) {
    int index;

#ifdef VBE_SUPPORT
    // set the graphics mode to SVGA 640x480x256
    setGraphicsModeVesa(640, 480, 8);

    // create the off-screen double buffer (replaces the two hardware VRAM pages)
    createDoubleBuffer(480);

    srand(time(NULL));

    // precompute a set of spheres for each of the two alternating frames
    // (matching what used to be page 0 and page 1)
    for (index = 0; index < NUM_SPHERES; index++) {
        Sphere0X[index] = 40 + rand() % 560;
        Sphere0Y[index] = 24 + rand() % 432;
        Sphere0Radius[index] = rand() % 30;
        Sphere0Color[index] = 34 + rand() % 6;
    }

    for (index = 0; index < NUM_SPHERES; index++) {
        Sphere1X[index] = 40 + rand() % 560;
        Sphere1Y[index] = 24 + rand() % 432;
        Sphere1Radius[index] = rand() % 30;
        Sphere1Color[index] = 34 + rand() % 6;
    }

    // now toggle between the two sphere layouts, redrawing into the double
    // buffer and flipping it to the screen in place of the old page swap
    while (!kbhit()) {
        fillDoubleBuffer(0);

        for (index = 0; index < NUM_SPHERES; index++) {
            drawSphere(Sphere0X[index], Sphere0Y[index], Sphere0Radius[index], Sphere0Color[index]);
        }

        displayDoubleBuffer(DoubleBuffer, 0);
        timeDelay(5);

        fillDoubleBuffer(0);

        for (index = 0; index < NUM_SPHERES; index++) {
            drawSphere(Sphere1X[index], Sphere1Y[index], Sphere1Radius[index], Sphere1Color[index]);
        }

        displayDoubleBuffer(DoubleBuffer, 0);
        timeDelay(5);
    }

    deleteDoubleBuffer();
#else
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
#endif

    // restore the video system to text
    setGraphicsMode(TEXT_MODE);
}
