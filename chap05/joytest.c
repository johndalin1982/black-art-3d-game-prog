// joytest.c - A demo of the joystick driver

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

#define SHIP_FRAMES 16  // number of animation frames of ship

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

Sprite ShipSprite;      // the players ship

// these are the velocity lookup tables, they have pre-computed velocities
// for each of the 16 directions the ship can point
int XVelocity[SHIP_FRAMES] = {  0,  2,  4,  4, 6, 4, 4, 2, 0, -2, -4, -4, -6, -4, -4, -2 };
int YVelocity[SHIP_FRAMES] = { -6, -4, -4, -2, 0, 2, 4, 4, 6,  4,  4,  2,  0, -2, -4, -4 };

void main(int argc, char** argv) {
    int index,
        dx,     // use to hold roughly 15% of the range of each
        dy,     // joystick axis
        joyX,   // the final normalized joystick position values
        joyY;

    // test if there is a joystick
    if (!joystickAvailable(JOYSTICK_1)) {
        printf("Joystick 1 not connected. Exiting.\n");
        return;
    }

    printf("Joystick 1 detected...\n");

    // calibrate the stick
    joystickCalibrate(JOYSTICK_1, USE_LOW_LEVEL);

    // compute 15% of range
    dx = (int)(0.5f + 0.15f * (Joystick1MaxX - Joystick1MinX));
    dy = (int)(0.5f + 0.15f * (Joystick1MaxY - Joystick1MinY));

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // load the imagery for ship
    pcxInit(&ImagePcx);
    pcxLoad("falcon.pcx", &ImagePcx, 1);

    // initialize the ship sprite
    spriteInit(&ShipSprite, 160, 100, 24, 20, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the ship, there are 16 of them, one for each
    // pre-rotated angle

    // get images off first row of template 0-11
    for (index = 0; index < 12; index++) {
        pcxGetSprite(&ImagePcx, &ShipSprite, index, index, 0);
    }

    // get images off second row of template 12-15
    for (index = 12; index < 16; index++) {
        pcxGetSprite(&ImagePcx, &ShipSprite, index, index - 12, 1);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // now load the background starfield
    pcxInit(&ImagePcx);
    pcxLoad("frontier.pcx", &ImagePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);

    // delete the pcx image
    pcxDelete(&ImagePcx);

    // scan under the ship, so the first time through the event loop
    // there is something to replace
    spriteUnderClip(&ShipSprite, DoubleBuffer);

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // do animation cycle: 1. erase, 2. game logic, 3. scan, 4. draw

        // erase the ship
        spriteEraseClip(&ShipSprite, DoubleBuffer);

        // PLAYERS SHIP LOGIC

        // get joystick position and subtract away center to
        // compute delta from center
        joyX = joystick(JOYSTICK_1_X) - Joystick1NeutralX;
        joyY = joystick(JOYSTICK_1_Y) - Joystick1NeutralY;

        // test if player has moved stick past the 10% mark, if so transform ship
        if (joyX > dx) {
            // rotate ship right
            if (++ShipSprite.currFrame == SHIP_FRAMES) {
                ShipSprite.currFrame = 0;
            }
        } else if (joyX < -dx) {
            // rotate ship left
            if (--ShipSprite.currFrame == -1) {
                ShipSprite.currFrame = SHIP_FRAMES - 1;
            }
        }

        // test if player is moving ship forward or backward
        if (joyY > dy) {
            // move ship backward

            // index into velocity table and translate ship with values
            ShipSprite.x -= XVelocity[ShipSprite.currFrame];
            ShipSprite.y -= YVelocity[ShipSprite.currFrame];
        } else if (joyY < -dy) {
            // move ship forward
            ShipSprite.x += XVelocity[ShipSprite.currFrame];
            ShipSprite.y += YVelocity[ShipSprite.currFrame];
        }

        // clip ship to screen universe
        if (ShipSprite.x > 319) {
            ShipSprite.x = -24;
        } else if (ShipSprite.x < -24) {
            ShipSprite.x = 319;
        }

        if (ShipSprite.y > 199) {
            ShipSprite.y = -20;
        } else if (ShipSprite.y < -20) {
            ShipSprite.y = 199;
        }

        // ready to draw ship, but first scan background under them
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
