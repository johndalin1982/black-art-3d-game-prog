// jumper.c - A demo of spider jumping around using patterns

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

#define NUM_PATTERNS 6

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

Sprite SpiderSprite;    // the jumping spider

// these are the patterns encoded as strings, with a max of 64 commands
// kinda like DNA patterns
char* Patterns[NUM_PATTERNS] = {
    "rrrrrrrrrrrrruuuuuuuuuuuulllllllllllllllldddddddddddddlllllllll.",
    "urururururururururrrrrrrrdrdrdrdrdrdrdrdrlllllllllllllddddllddd.",
    "rrrrurrrrurrrruururururururuulululululululdldldldldrdrdrdrdrdrd.",
    "xxxxxxxxxxxuuuuuuuuuudddduuuudddduuuddduuuddduuuddduuuddduuulll.",
    "rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrruuuuuuuuuuuuuuuuuuuuuuuuuuuuuu.",
    "lllllllllllllllrrrrrrrrrrrrrrrrrddddddddddddddddxxxxxxxrrrrrruu."
};

void main(int argc, char** argv) {
    int index;
    char buffer[64];    // used to print strings

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // load the imagery for the spider
    pcxInit(&ImagePcx);
    pcxLoad("jumpspd.pcx", &ImagePcx, 1);

    // initialize the spider
    spriteInit(&SpiderSprite, 160, 100, 30, 24, 0, 0, 0, 0, 0, 0);
    SpiderSprite.state = 0; // select pattern one
    SpiderSprite.counter1 = 0;

    // extract the bitmaps for the spider, there are 4 animation cells
    for (index = 0; index < 4; index++) {
        pcxGetSprite(&ImagePcx, &SpiderSprite, index, index, 0);
    }

    // done with this PCX file so delete memory associated with it
    pcxDelete(&ImagePcx);

    // now load the background image
    pcxInit(&ImagePcx);
    pcxLoad("jumpbak.pcx", &ImagePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);
    pcxDelete(&ImagePcx);

    // scan background before entering event loop
    spriteUnderClip(&SpiderSprite, DoubleBuffer);

    // put up exit instructions
    printStringDb(80, 2, 9, "Hit any key to exit", 1);

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // do animation cycle, erase, move draw...
        // erase all objects by replacing what was under them
        spriteEraseClip(&SpiderSprite, DoubleBuffer);

        // BEGIN PATTERN MOTION LOGIC ///////////////////////////////////////////

        // move spider, test if there is still data commands in current pattern
        if (Patterns[SpiderSprite.state][SpiderSprite.counter1] != '.') {
            // what is the command
            switch (Patterns[SpiderSprite.state][SpiderSprite.counter1]) {
                case 'r': // right
                {
                    // move spider
                    SpiderSprite.x += 4;

                    // test if off edge
                    if (SpiderSprite.x > 320) {
                        SpiderSprite.x = -30;
                    }
                } break;

                case 'l': // left
                {
                    // move spider
                    SpiderSprite.x -= 4;

                    // test if off edge
                    if (SpiderSprite.x < -30) {
                        SpiderSprite.x = 320;
                    }
                } break;

                case 'u': // up
                {
                    // move spider
                    SpiderSprite.y -= 4;

                    // test if off edge
                    if (SpiderSprite.y < -24) {
                        SpiderSprite.y = 200;
                    }
                } break;

                case 'd': // down
                {
                    // move spider
                    SpiderSprite.y += 4;

                    // test if off edge
                    if (SpiderSprite.y > 200) {
                        SpiderSprite.y = -24;
                    }
                } break;

                case 'x': // do nothing
                {
                } break;

                default:
                    break;
            }

            // increment pattern index
            SpiderSprite.counter1++;
        } else {
            // select a new pattern and reset pattern index
            SpiderSprite.state = rand() % NUM_PATTERNS;
            SpiderSprite.counter1 = 0;
        }

        // END PATTERN MOTION LOGIC //////////////////////////////////////////////

        // animate spider
        if (++SpiderSprite.currFrame == 4) {
            SpiderSprite.currFrame = 0;
        }

        // display current pattern and data
        sprintf(
            buffer,
            "Pattern #%d, data=%c",
            SpiderSprite.state,
            Patterns[SpiderSprite.state][SpiderSprite.counter1]);

        printStringDb(88, 190, 15, buffer, 0);

        // ready to draw spider, but first scan background under it
        spriteUnderClip(&SpiderSprite, DoubleBuffer);
        spriteDrawClip(&SpiderSprite, DoubleBuffer, 1);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        timeDelay(1);
    }

    // exit in a very cool way
    screenTransition(SCREEN_DARKNESS);

    // free up all resources
    spriteDelete(&SpiderSprite);
    deleteDoubleBuffer();
    setGraphicsMode(TEXT_MODE);
}
