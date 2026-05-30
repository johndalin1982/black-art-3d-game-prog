// light.c - An example of real-time event loops

#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <math.h>
#include <graph.h>

#include "black3.h"

#define NORTH 0
#define EAST 1
#define SOUTH 2
#define WEST 3

void delay(int clicks) {
    // this function uses the internal timer to delay a number of clock ticks
    unsigned long FAR* clock = (unsigned long FAR*)0x0000046CL;
    unsigned long now;

    // get current time
    now = *clock;

    // wait for number of click to pass
    while (abs(*clock - now) < clicks) {}
}

void drawGameGrid(void) {
    // this function draw a game grid out of horizontal and vertical lines
    short x, y;

    // set the line color to white
    //_setcolor(15);
    int whiteColor = 15;

    // draw the vertical lines
    for (x = 0; x < 320; x += 20) {
        // position the pen and draw the line
        lineV(0, 199, x, whiteColor);
        //_moveto(x, 0);
        //_lineto(x, 199);
    }

    // draw the horizontal lines
    for (y = 0; y < 200; y += 20) {
        // position the pen and draw the line
        lineH(0, 319, y, whiteColor);
        //_moveto(0, y);
        //_lineto(319, y);
    }
}

void main(void) {
    int done = 0,       // main event loop exit flag
        playerX = 160,  // starting position and direction of player
        playerY = 150,
        playerDirection = NORTH;

    // SECTION 1
    // set the graphics mode to mode 13h 320x200x256
    //_setvideomode(_MRES256COLOR);
    setGraphicsMode(GRAPHICS_MODE13);

    // draw the game grid
    drawGameGrid();

    // SECTION 2
    // begin real time event loop
    while (!done) {
        // SECTION 3
        // is the player steering his light cycle or trying to exit?
        if (kbhit()) {
            // test for <A>, <S> or <Q>
            switch (getch()) {
                case 'a': // turn left
                {
                    // turn 90 to the left
                    if (--playerDirection < NORTH) {
                        playerDirection = WEST;
                    }

                    break;
                }

                case 's': // turn right
                {
                    // turn 90 to the right
                    if (++playerDirection > WEST) {
                        playerDirection = NORTH;
                    }

                    break;
                }

                case 'q': // quit game
                {
                    // set exit flag to true
                    done = 1;
                }

                break;

                default:
                    break;
            }
        }

        // SECTION 4
        // at this point we need to move the light cycle in the direction it's
        // currently pointing
        switch (playerDirection) {
            case NORTH:
            {
                if (--playerY < 0) {
                    playerY = 199;
                }

                break;
            }

            case SOUTH:
            {
                if (++playerY > 199) {
                    playerY = 0;
                }

                break;
            }

            case EAST:
            {
                if (++playerX > 319) {
                    playerX = 0;
                }

                break;
            }

            case WEST:
            {
                if (--playerX < 0) {
                    playerX = 319;
                }

                break;
            }

            default:
                break;
        }

        // SECTION 5
        // render the lightcycle
        //_setcolor((short)9);
        //_setpixel((short)playerX, (short)playerY);
        writePixel(playerX, playerY, 9);

        // wait a moment and lock the game to 18 fps
        delay(1);
    }

    // SECTION 6
    // restore the video mode back to text
    //_setvideomode(_DEFAULTMODE);
    setGraphicsMode(TEXT_MODE);
}

