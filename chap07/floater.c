// floater.c - A demo of 2D terrain following

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

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

Sprite SpeederSprite;   // the floating speeder

void main(int argc, char** argv) {
    int index,
        terrY = 160,        // these are used to draw random terrain
        terrDraw = 1,
        rough,              // roughness of terrain, input by user
        x,
        y,
        hoverHeight = 2;    // minimum terrain following height

    // query user about terrain roughness and terrain following height
    printf("Enter the roughness of terrain from 1-10?");
    scanf("%d", &rough);
    printf("Enter the terrain following height from 1-50?");
    scanf("%d", &hoverHeight);

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // load the imagery for the speeder
    pcxInit(&ImagePcx);
    pcxLoad("floatspd.pcx", &ImagePcx, 1);

    // initialize the speeder
    spriteInit(&SpeederSprite, 320, 100, 40, 10, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the speeder, there are 4 animation cells
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &SpeederSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // draw the terrain one vertical strip at a time
    // seed the random number generator with current time
    srand(*(int FAR*)0x0000046CL);

    for (x = 0; x < 320; x++) {
        // test if it's time to change directions
        if (--terrDraw <= 0) {
            terrDraw = rand() % (20 / rough);
            terrY = terrY - 1 + rand() % 3;
        }

        // draw a vertical strip at current x location
        writePixelDb(x, terrY, 15);

        for (y = terrY + 1; y < 200; y++) {
            writePixelDb(x, y, 200 + rand() % 16);
        }
    }

    // scan background before entering event loop
    spriteUnderClip(&SpeederSprite, DoubleBuffer);

    // put up exit instructions
    printStringDb(80, 2, 9, "Hit any key to exit", 1);

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // do animation cycle, erase, move, draw...

        // erase all objects by replacing what was under them
        spriteEraseClip(&SpeederSprite, DoubleBuffer);

        // move speeder

        // if there is no terrain under speeder then apply downward thrust
        // at constant velocity
        if (!readPixelDb(SpeederSprite.x + 4, SpeederSprite.y + 12 + hoverHeight)) {
            SpeederSprite.y += 2;
        }

        // now horizontal thrust
        SpeederSprite.x -= 6;

        // now probe under speeder for terrain and apply upward thrust
        // if crust is found
        if (readPixelDb(SpeederSprite.x + 4, SpeederSprite.y + 12 + hoverHeight)) {
            SpeederSprite.y -= 2;
        }

        // test if speeder has moved off screen
        if (SpeederSprite.x < -40) {
            SpeederSprite.x = 320;
        }

        // this should never happen, but just in case
        if (SpeederSprite.y > 200) {
            SpeederSprite.y = 200;
        }

        // animate speeder
        if (++SpeederSprite.currFrame == 4) {
            SpeederSprite.currFrame = 0;
        }

        // ready to draw speeder, but first scan background under it
        spriteUnderClip(&SpeederSprite, DoubleBuffer);
        spriteDrawClip(&SpeederSprite, DoubleBuffer, 1);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_DARKNESS);

    // free up all resources
    spriteDelete(&SpeederSprite);
    deleteDoubleBuffer();
    setGraphicsMode(TEXT_MODE);
}
