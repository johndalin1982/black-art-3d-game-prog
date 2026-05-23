// worms.c - A demo of sprites, clipping and double buffering

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

PcxPicture ImagePcx;                // general PCX image used to load background and imagery
Sprite WormSprite, AntSprite;       // the worm and ant

void main(int argc, char** argv) {
    int index;  // loop variable

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    if (!createDoubleBuffer(200)) {
        return;
    }

    // load the imagery for worm
    pcxInit(&ImagePcx);
    pcxLoad("wormimg.pcx", &ImagePcx, 1);

    // initialize the worm sprite
    spriteInit(&WormSprite, 160, 100, 38, 20, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the worm, there are 4 animation cells
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &WormSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // load the imagery for ant
    pcxInit(&ImagePcx);
    pcxLoad("antimg.pcx", &ImagePcx, 1);

    // initialize the ant sprite
    spriteInit(&AntSprite, 160, 180, 12, 6, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the ant, there are 3 animation cells
    for (index = 0; index < 3; index++) {
        pcxGetSprite(&ImagePcx, &AntSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // now load the background that the worm and ant will run around in
    pcxInit(&ImagePcx);
    pcxLoad("mushroom.pcx", &ImagePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);

    pcxDelete(&ImagePcx);

    // scan under ant and worm before entering the event loop, this must be
    // done or else on the first cycle the "erase" function will draw garbage
    spriteUnder(&AntSprite, DoubleBuffer);
    spriteUnderClip(&WormSprite, DoubleBuffer);

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // do animation cycle, erase, move draw...

        // erase all objects by replacing what was under them
        spriteErase(&AntSprite, DoubleBuffer);
        spriteEraseClip(&WormSprite, DoubleBuffer);

        // move objects, test if they have run off right edge of screen
        if ((AntSprite.x += 1) > 320 - 12) {
            AntSprite.x = 0;
        }

        if ((WormSprite.x += 6) > 320) {
            WormSprite.x = -40;   // start worm back one length beyond the left edge of screen
        }

        // do animation for objects
        if (++AntSprite.currFrame == 3) {
            AntSprite.currFrame = 0;  // if all frames have been displayed then reset back to frame 0
        }

        if (++WormSprite.currFrame == 4) {
            WormSprite.currFrame = 0;     // if all frames have been displayed then reset back to frame 0
        }

        // ready to draw objects, but first scan background under them
        spriteUnder(&AntSprite, DoubleBuffer);
        spriteUnderClip(&WormSprite, DoubleBuffer);

        spriteDraw(&AntSprite, DoubleBuffer, 1);
        spriteDrawClip(&WormSprite, DoubleBuffer, 1);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_SWIPE_X);

    // free up all resources
    spriteDelete(&WormSprite);
    spriteDelete(&AntSprite);
    deleteDoubleBuffer();
    setGraphicsMode(TEXT_MODE);
}
