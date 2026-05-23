// lockon.c - A demo of tracking and evasion algorithms

#include <io.h>
#include <conio.h>
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include <fcntl.h>
#include <memory.h>
#include <malloc.h>
#include <math.h>
#include <string.h>

#include "black3.h"
#include "black4.h"
#include "black5.h"

// there are the animation cell indices for the alien walking in all the directions
#define ALIEN_START_RIGHT   0
#define ALIEN_START_LEFT    4
#define ALIEN_START_UP      8
#define ALIEN_START_DOWN    12

#define ALIEN_END_RIGHT     3
#define ALIEN_END_LEFT      7
#define ALIEN_END_UP        11
#define ALIEN_END_DOWN      15

// these are the directions the alien can move in
#define ALIEN_RIGHT         0
#define ALIEN_LEFT          1
#define ALIEN_UP            2
#define ALIEN_DOWN          3

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

Sprite AlienSprite, CreatureSprite; // the player's alien and the creature

void main(int argc, char** argv) {
    int index,
        chase = 1,  // used to select creature's mode of operation, 1 = case, 0 = evade
        done = 0;
    char buffer[64];    // used to print strings

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // load the imagery for the player's alien
    pcxInit(&ImagePcx);
    pcxLoad("lockaln.pcx", &ImagePcx, 1);

    // initialize the alien sprite
    spriteInit(&AlienSprite, 32, 164, 8, 12, 0, 0, 0, 0, 0, 0);

    // start the alien walking down
    AlienSprite.state = ALIEN_DOWN;
    AlienSprite.currFrame = ALIEN_START_DOWN;

    // extract the bitmaps for the alien, there are 16 animation cells
    for (index = 0; index < 16; index++) {
        pcxGetSprite(&ImagePcx, &AlienSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // load the imagery for creature
    pcxInit(&ImagePcx);
    pcxLoad("lockcrt.pcx", &ImagePcx, 1);

    // initialize the creature sprite
    spriteInit(&CreatureSprite, 160, 100, 24, 12, 0, 0, 0, 0, 0, 0);

    // extract the bitmaps for the creature, there are 4 animation cells
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &CreatureSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // now load the background image
    pcxInit(&ImagePcx);
    pcxLoad("lockbak.pcx", &ImagePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);
    pcxDelete(&ImagePcx);

    // scan under alien and creature before entering the event loop, this must be
    // done or else on the first cycle the "erase" function will draw garbage
    spriteUnderClip(&AlienSprite, DoubleBuffer);
    spriteUnderClip(&CreatureSprite, DoubleBuffer);

    // install new keyboard driver
    keyboardInstallDriver();

    // put up exit instructions
    printStringDb(96, 2, 9, "Press Q to exit", 0);

    // main event loop, process until keyboard hit
    while (!done) {
        // do animation cycle, erase, move draw...
        // erase all objects by replacing what was under the them
        spriteEraseClip(&AlienSprite, DoubleBuffer);
        spriteEraseClip(&CreatureSprite, DoubleBuffer);

        // move player
        // test for right motion
        if (KeyboardState[MAKE_RIGHT]) {
            // move alien
            if ((AlienSprite.x += 2) > 320) {
                AlienSprite.x = -8;
            }

            // first test if alien was already moving right
            if (AlienSprite.state == ALIEN_RIGHT) {
                // animate and test for end of sequence
                if (++AlienSprite.currFrame == ALIEN_END_RIGHT) {
                    AlienSprite.currFrame = ALIEN_START_RIGHT;
                }
            } else {
                // set state and current frame to right
                AlienSprite.state = ALIEN_RIGHT;
                AlienSprite.currFrame = ALIEN_START_RIGHT;
            }
        // test for left motion
        } else if (KeyboardState[MAKE_LEFT]) {
            // move alien
            if ((AlienSprite.x -= 2) < -8) {
                AlienSprite.x = 320;
            }

            // first test if alien was already moving left
            if (AlienSprite.state == ALIEN_LEFT) {
                // animate and test for end of sequence
                if (++AlienSprite.currFrame == ALIEN_END_LEFT) {
                    AlienSprite.currFrame = ALIEN_START_LEFT;
                }
            } else {
                // set state and current frame to left
                AlienSprite.state = ALIEN_LEFT;
                AlienSprite.currFrame = ALIEN_START_LEFT;
            }
        // test for upward motion
        } else if (KeyboardState[MAKE_UP]) {
            // move alien
            if ((AlienSprite.y -= 2) < -12) {
                AlienSprite.y = 200;
            }

            // first test if alien was already moving up
            if (AlienSprite.state == ALIEN_UP) {
                // animate and test for end of sequence
                if (++AlienSprite.currFrame == ALIEN_END_UP) {
                    AlienSprite.currFrame = ALIEN_START_UP;
                }
            } else {
                // set state and current frame to up
                AlienSprite.state = ALIEN_UP;
                AlienSprite.currFrame = ALIEN_START_UP;
            }
        // test for downward motion
        } else if (KeyboardState[MAKE_DOWN]) {
            // move alien
            if ((AlienSprite.y += 2) > 200) {
                AlienSprite.y = -12;
            }

            // first test if alien was already moving down
            if (AlienSprite.state == ALIEN_DOWN) {
                // animate and test for end of sequence
                if (++AlienSprite.currFrame == ALIEN_END_DOWN) {
                    AlienSprite.currFrame = ALIEN_START_DOWN;
                }
            } else {
                // set state and current frame to right
                AlienSprite.state = ALIEN_DOWN;
                AlienSprite.currFrame = ALIEN_START_DOWN;
            }
        }

        // test for tracking toggle
        if (KeyboardState[MAKE_SPACE]) {
            chase = -chase;

            while (KeyboardState[MAKE_SPACE]);
        }

        // test for exit key
        if (KeyboardState[MAKE_Q]) {
            done = 1;
        }

        // BEGIN TRACKING LOGIC /////////////////////////////////////////

        // move creature, test if creature is chasing or evading player
        if (chase == 1) {
            // track on x coordinate
            if (AlienSprite.x > CreatureSprite.x) {
                CreatureSprite.x++;
            } else if (AlienSprite.x < CreatureSprite.x) {
                CreatureSprite.x--;
            }

            // now track on y coordinate
            if (AlienSprite.y > CreatureSprite.y) {
                CreatureSprite.y++;
            } else if (AlienSprite.y < CreatureSprite.y) {
                CreatureSprite.y--;
            }
        } else {
            // must be evading

            // evade on x coordinate
            if (AlienSprite.x < CreatureSprite.x) {
                CreatureSprite.x++;
            } else if (AlienSprite.x > CreatureSprite.x) {
                CreatureSprite.x--;
            }

            // now evade on y coordinate
            if (AlienSprite.y < CreatureSprite.y) {
                CreatureSprite.y++;
            } else if (AlienSprite.y > CreatureSprite.y) {
                CreatureSprite.y--;
            }
        }

        // test if creature has moved off screen
        if (CreatureSprite.x > 310) {
            CreatureSprite.x = 310;
        } else if (CreatureSprite.x < -14) {
            CreatureSprite.x = -14;
        }

        if (CreatureSprite.y > 190) {
            CreatureSprite.y = 190;
        } else if (CreatureSprite.y < -2) {
            CreatureSprite.y = -2;
        }

        // END TRACKING LOGIC ////////////////////////////////////////////

        // do animation for creature
        if (++CreatureSprite.currFrame == 4) {
            CreatureSprite.currFrame = 0;
        }

        // ready to draw objects, but first scan backgound under them
        spriteUnderClip(&AlienSprite, DoubleBuffer);
        spriteUnderClip(&CreatureSprite, DoubleBuffer);
        spriteDrawClip(&AlienSprite, DoubleBuffer, 1);
        spriteDrawClip(&CreatureSprite, DoubleBuffer, 1);

        // display test message of current tracking mode
        if (chase == 1) {
            sprintf(buffer, "Creature is Chasing!!!");
        } else {
            sprintf(buffer, "Creature is Evading!!!");
        }

        printStringDb(64, 190, 12, buffer, 0);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_WHITENESS);

    // free up all resources
    spriteDelete(&AlienSprite);
    spriteDelete(&CreatureSprite);
    deleteDoubleBuffer();
    setGraphicsMode(TEXT_MODE);
    keyboardRemoveDriver();
}
