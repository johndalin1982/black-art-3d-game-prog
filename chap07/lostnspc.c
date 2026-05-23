// lostnspc.c - A demo of random motion

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

Sprite ShipSprite;      // the alien ship

void main(int argc, char** argv) {
    int index,
        velocityX = 0,  // used to control velocity of ship
        velocityY = 0;

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // load the imagery for the ship
    pcxInit(&ImagePcx);
    pcxLoad("lostship.pcx", &ImagePcx, 1);

    // initialize the ship
    spriteInit(&ShipSprite, 160, 100, 18, 16, 0, 0, 0, 0, 0, 0);

    // make ship sit for the first 1.5 seconds
    ShipSprite.counter1 = 0;
    ShipSprite.threshold1 = 25;

    // extract the bitmaps for the ship, there are 2 animation cells
    for (index = 0; index < 2; index++) {
        pcxGetSprite(&ImagePcx, &ShipSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // now load the background image
    pcxInit(&ImagePcx);
    pcxLoad("lostback.pcx", &ImagePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);
    pcxDelete(&ImagePcx);

    // scan background before entering event loop
    spriteUnderClip(&ShipSprite, DoubleBuffer);

    // put up exit instructions
    printStringDb(80, 2, 9, "Hit any key to exit", 1);

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // do animation cycle, erase, move draw...

        // erase all objects by replacing what was under them
        spriteEraseClip(&ShipSprite, DoubleBuffer);

        // BEGIN RANDOM MOTION LOGIC //////////////////////////////////////////

        // test if ship is complete with current trajectory and
        // needs a new one selected
        if (++ShipSprite.counter1 > ShipSprite.threshold1) {
            // select new direction vector
            velocityX = -5 + rand() % 10;
            velocityY = -5 + rand() % 10;

            // select a random number of frames to stay on new heading
            ShipSprite.threshold1 = 5 + rand() % 50;

            // reset counter
            ShipSprite.counter1 = 0;
        }

        // move ship
        ShipSprite.x += velocityX;
        ShipSprite.y += velocityY;

        // test if ship went beyond screen edges
        if (ShipSprite.x > 320) {
            ShipSprite.x = -18;
        } else if (ShipSprite.x < -18) {
            ShipSprite.x = 320;
        }

        if (ShipSprite.y > 200) {
            ShipSprite.y = -16;
        } else if (ShipSprite.y < -16) {
            ShipSprite.y = 200;
        }

        // END RANDOM MOTION LOGIC ////////////////////////////////////////////

        // animate ship
        if (++ShipSprite.currFrame == 2) {
            ShipSprite.currFrame = 0;
        }

        // add some special effects via a vapor trail
        if (rand() % 10 == 1) {
            writePixelDb(
                ShipSprite.x + rand() % 20,
                ShipSprite.y + 12 + rand() % 4,
                24 + rand() % 4);
        }

        // ready to draw ship, but first scan background under it
        spriteUnderClip(&ShipSprite, DoubleBuffer);
        spriteDrawClip(&ShipSprite, DoubleBuffer, 1);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_DARKNESS);

    // free up all resources
    spriteDelete(&ShipSprite);
    deleteDoubleBuffer();
    setGraphicsMode(TEXT_MODE);
}
