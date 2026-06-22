// volcano.c - A demo of multiple data single logic programming

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
#include "black8.h"

#define MAX_CINDERS         100 // maximum number of cinders in the simulation

#ifdef VBE_SUPPORT
#define VOLCANO_MOUTH_X     290 // position where cinders will be emitted (2x 145)
#define VOLCANO_MOUTH_Y     245 // (2.4x 102)
#else
#define VOLCANO_MOUTH_X     145 // position where cinders will be emitted
#define VOLCANO_MOUTH_Y     102
#endif

#define CINDER_START_COLOR  48  // starting cinder color
#define CINDER_END_COLOR    (48+15) // ending cinder color

typedef struct CinderType {
    int x, y;       // position of cinder
    int xv, yv;     // velocity of cinder
    int color;      // color of cinder
    int lifetime;   // total number of frames cinder will live for
    int counter1;   // tracks when cinder color will change
    int threshold1;
    int counter2;   // tracks when gravity will be applied
    int threshold2;
    int col;        // the current color of the cinder
    int under;      // this holds the pixel under the cinder
} Cinder, *CinderPtr;

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

Cinder Cinders[MAX_CINDERS];    // this holds all the cinders
int ActiveCinders;              // number of active cinders in the world

void initCinders(int startingCinder, int endingCinder) {
    // this function initializes cinders, it can initialize a sequence of cinders,
    // or a single cinder
    int index;

    // initialize each cinder or restart a single cinder
    for (index = startingCinder; index <= endingCinder; index++) {
        // fill in position, velocity and color of cinder
#ifdef VBE_SUPPORT
        Cinders[index].x = VOLCANO_MOUTH_X - 10 + rand() % 21;
        Cinders[index].y = VOLCANO_MOUTH_Y - rand() % 7;

        Cinders[index].xv = -4 + rand() % 9;
        Cinders[index].yv = -10 - rand() % 10;
#else
        Cinders[index].x = VOLCANO_MOUTH_X - 5 + rand() % 11;
        Cinders[index].y = VOLCANO_MOUTH_Y - rand() % 3;

        Cinders[index].xv = -2 + rand() % 5;
        Cinders[index].yv = -5 - rand() % 5;
#endif

        Cinders[index].col = CINDER_START_COLOR;

        // scan under cinder
        Cinders[index].under = readPixelDb(Cinders[index].x, Cinders[index].y);

        // set timing fields
        Cinders[index].lifetime = 20 + rand() % 100;

        Cinders[index].counter1 = 0;
        Cinders[index].counter2 = 0;

        // set how long it will take for cinder to cool
        Cinders[index].threshold1 = 2 + rand() % 6;

        // set how long it will take for gravity to take effect
        Cinders[index].threshold2 = 1 + rand() % 3;
    }
}

void eraseCinders(void) {
    // this function replaces the pixel that was under a cinder
    int index;

    for (index = 0; index < ActiveCinders; index++) {
        if (Cinders[index].y >= 0) {
            writePixelDb(Cinders[index].x, Cinders[index].y, Cinders[index].under);
        }
    }
}

void scanCinders(void) {
    // this function scans under the cinders
    int index;

    for (index = 0; index < ActiveCinders; index++) {
        if (Cinders[index].y >= 0) {
            Cinders[index].under = readPixelDb(Cinders[index].x, Cinders[index].y);
        }
    }
}

void drawCinders(void) {
    // this function draws the cinders
    int index;

    for (index = 0; index < ActiveCinders; index++) {
        if (Cinders[index].y >= 0) {
            writePixelDb(Cinders[index].x, Cinders[index].y, Cinders[index].col);
        }
    }
}

void moveCinders(void) {
    // this function moves and updates all the timing fields of the cinder
    // it also applices gravity to the cinders
    int index,
        pixel;  // used to read the pixels under the cinder

    // process each cinder
    for (index = 0; index < ActiveCinders; index++) {
        // move the cinder
        Cinders[index].x += Cinders[index].xv;
        Cinders[index].y += Cinders[index].yv;

        // apply gravity
        if (++Cinders[index].counter2 >= Cinders[index].threshold2) {
            // apply a downward velocity of 1
            Cinders[index].yv++;

            // reset gravity counter
            Cinders[index].counter2 = 0;
        }

        // test if it's time to update cinder color
        if (++Cinders[index].counter1 >= Cinders[index].threshold1) {
            // reset counter
            Cinders[index].counter1 = 0;

            // test if cinder is already out
            if (Cinders[index].col < CINDER_END_COLOR) {
                Cinders[index].col++;
            }
        }

        // test if cinder is dead, off screen, lifetime over, or hit mountain
        pixel = readPixelDb(Cinders[index].x, Cinders[index].y);

        // test if the pixel is part of the mountain
        if (pixel != 0 && (pixel < CINDER_START_COLOR || pixel > CINDER_END_COLOR)) {
            // restart this cinder
            initCinders(index, index);
        } else if (--Cinders[index].lifetime <= 0) {
            // restart this cinder
            initCinders(index, index);
#ifdef VBE_SUPPORT
        } else if (Cinders[index].x > 640 || Cinders[index].x < 0) {
#else
        } else if (Cinders[index].x > 320 || Cinders[index].x < 0) {
#endif
            // restart this cinder
            initCinders(index, index);
        }
    }
}

void main(int argc, char** argv) {
#ifdef VBE_SUPPORT
    // set the graphics mode to SVGA 640x480x256
    setGraphicsModeVesa(640, 480, 8);

    // create the double buffer
    createDoubleBuffer(480);
#else
    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);
#endif

#ifdef DOS_32_BIT
    // load the character set for text output (RomCharSet starts NULL
    // under DOS_32_BIT - 16-bit builds read the BIOS ROM font directly)
#ifdef VBE_SUPPORT
    // font16.bin matches this demo's 640x480 resolution (generated by
    // exp_font.exe alongside the base font.bin)
    loadFontSet("font16.bin", 16);
#else
    initRomCharSet();
#endif
#endif

    // now load the background image
    pcxInit(&ImagePcx);
    pcxLoad("volcano.pcx", &ImagePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);
    pcxDelete(&ImagePcx);

    // draw instructions
#ifdef VBE_SUPPORT
    printStringDb(160, 5, 9, "Hit any key to exit", 1);
#else
    printStringDb(80, 2, 9, "Hit any key to exit", 1);
#endif

    // initialize all the cinders
    initCinders(0, 19);
    ActiveCinders = 20;

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // erase all the cinders coming out of the volcano
        eraseCinders();

        // move all the cinders
        moveCinders();

        // scan under and draw the cinders
        scanCinders();
        drawCinders();

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_DARKNESS);

    // free up all resources
    deleteDoubleBuffer();
    setGraphicsMode(TEXT_MODE);
}
