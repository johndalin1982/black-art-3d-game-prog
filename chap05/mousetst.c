// mousetst.c - A demo of the mouse driver with some added fun

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

#define ANT_DEAD        0   // the ant is dead (smashed)
#define ANT_EAST        1   // the ant is moving east
#define ANT_WEST        2   // the ant is moving west

#define HAMMER_UP       0   // the hammer is in its resetting position
#define HAMMER_MOVING   1   // the hammer is hammering!!!

PcxPicture ImagePcx;        // general PCX image used to load background and imagery

Sprite AntSprite,           // the ant
       HammerSprite;        // the players hammer

void main(int argc, char** argv) {
    int index,
        mouseX, // mouse status
        mouseY,
        buttons;

    srand(time(NULL));

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // load the imagery for ant
    pcxInit(&ImagePcx);
    pcxLoad("moreants.pcx", &ImagePcx, 1);

    // initialize the ant sprite
    spriteInit(&AntSprite, 0, 0, 12, 6, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the ant, there are 3 animation cells for each
    // direction thus 6 cells
    for (index = 0; index < 6; index++) {
        pcxGetSprite(&ImagePcx, &AntSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // load the imagery for players hammer
    pcxInit(&ImagePcx);
    pcxLoad("hammer.pcx", &ImagePcx, 1);

    // initialize the hammer sprite that will take the place of the mouse pointer
    spriteInit(&HammerSprite, 0, 0, 22, 20, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the hammer there are 5
    for (index = 0; index < 5; index++) {
        pcxGetSprite(&ImagePcx, &HammerSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // now load the picnic area background
    pcxInit(&ImagePcx);
    pcxLoad("grass.pcx", &ImagePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);

    // delete the pcx image
    pcxDelete(&ImagePcx);

    // scan under AntSprite and HammerSprite before entering the event loop, this must be
    // done or else on the first cycle the "erase" function will draw garbage
    spriteUnderClip(&AntSprite, DoubleBuffer);
    spriteUnderClip(&HammerSprite, DoubleBuffer);

    // reset the mouse and hide the pointer
    mouseControl(MOUSE_RESET, NULL, NULL, &buttons);
    mouseControl(MOUSE_HIDE, NULL, NULL, NULL);

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // do animation cycle: 1. erase, 2. game logic, 3. scan, 4. draw

        // erase all objects by replacing what was under them
        if (AntSprite.state != ANT_DEAD) {
            spriteEraseClip(&AntSprite, DoubleBuffer);
        }

        spriteEraseClip(&HammerSprite, DoubleBuffer);

        // PLAYERS HAMMER LOGIC

        // obtain the new position of mouse and state of buttons
        mouseControl(MOUSE_POSITION_BUTTONS, &mouseX, &mouseY, &buttons);

        // map the mouse position to the screen and assign it to HammerSprite
        HammerSprite.x = (mouseX >> 1) - 16;
        HammerSprite.y = mouseY;

        // test if player is trying to use HammerSprite
        if (buttons == MOUSE_LEFT_BUTTON && HammerSprite.state == HAMMER_UP) {
            // set state of hammer to moving
            HammerSprite.state = HAMMER_MOVING;
        }

        // test if HammerSprite is animating
        if (HammerSprite.state == HAMMER_MOVING) {
            // test if sequence complete
            if (++HammerSprite.currFrame == 4) {
                HammerSprite.state = HAMMER_UP;
                HammerSprite.currFrame = 0;
            }

            // test if HammerSprite is hitting AntSprite
            if (HammerSprite.currFrame == 3 && AntSprite.state != ANT_DEAD) {
                // do a collision test between the HammerSprite and the AntSprite
                if (AntSprite.x > HammerSprite.x && AntSprite.x + 12 < HammerSprite.x + 22 &&
                    AntSprite.y > HammerSprite.y && AntSprite.y + 6 < HammerSprite.y + 20) {

                    // kill ant
                    AntSprite.state = ANT_DEAD;

                    // draw a smashed ant, use frame 5 of HammerSprite
                    // set current frame to blood splat
                    HammerSprite.currFrame = 4;

                    // draw the splat
                    spriteDrawClip(&HammerSprite, DoubleBuffer, 1);

                    // restore the hammer frame
                    HammerSprite.currFrame = 3;
                }
            }
        }

        // ANT LOGIC

        // test if it's time to start an ant
        if (AntSprite.state == ANT_DEAD && rand() % 10 == 0) {
            // which direction will ant move in
            if (rand() % 2 == 0) {
                // move ant east
                AntSprite.y = rand() % 200;             // starting y position
                AntSprite.x = 0;                        // starting x position
                AntSprite.counter1 = 2 + rand() % 10;   // ant speed
                AntSprite.state = ANT_EAST;             // ant direction
                AntSprite.currFrame = 0;                // starting animation frame
            } else {
                // move ant west
                AntSprite.y = rand() % 200;             // starting y position
                AntSprite.x = 320;                      // starting x position
                AntSprite.counter1 = -2 - rand() % 10;  // ant speed
                AntSprite.state = ANT_WEST;             // ant direction
                AntSprite.currFrame = 0;                // starting animation frame
            }
        }

        // test if ant is alive
        if (AntSprite.state != ANT_DEAD) {
            // process ant
            // move the ant
            AntSprite.x += AntSprite.counter1;

            // is ant off screen?
            if (AntSprite.x < 0 || AntSprite.x > 320) {
                AntSprite.state = ANT_DEAD;
            }

            // animate the ant use proper animation cells based on direction
            // cells 0-2 are for eastward motion, cells 3-5 are for westward motion
            if (AntSprite.state == ANT_EAST) {
                if (++AntSprite.currFrame > 2) {
                    AntSprite.currFrame = 0;
                }
            }

            if (AntSprite.state == ANT_WEST) {
                if (++AntSprite.currFrame > 5) {
                    AntSprite.currFrame = 3;
                }
            }
        }

        // ready to draw objects, but first scan background under them
        if (AntSprite.state != ANT_DEAD) {
            spriteUnderClip(&AntSprite, DoubleBuffer);
        }

        spriteUnderClip(&HammerSprite, DoubleBuffer);

        if (AntSprite.state != ANT_DEAD) {
            spriteDrawClip(&AntSprite, DoubleBuffer, 1);
        }

        spriteDrawClip(&HammerSprite, DoubleBuffer, 1);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock into 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_SWIPE_X);

    // free up all resources
    spriteDelete(&AntSprite);
    spriteDelete(&HammerSprite);
    deleteDoubleBuffer();

    setGraphicsMode(TEXT_MODE);
}
