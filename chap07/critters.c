// critters.c - A demo of convergence or flocking

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

#define NUM_CRITTERS    200

typedef struct CritterType {
    float x, y,     // position of critter
          xv, yv;   // velocity of critter
    int back;       // the background color under the critter
} Critter, *CritterPtr;

PcxPicture ImagePcx;    // general PCX image used to load background and imagery

Critter Critters[NUM_CRITTERS]; // the array that holds all the critters

void main(int argc, char** argv) {
    int index;
    float speed,        // used to scale velocity vector of trajectory
          dx, dy,       // used to compute trajectory vectors
          length,       // length of trajectory vector, used to normalize
          centroidX,    // center of mass of all critters
          centroidY;

    // set the graphics mode to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // create the double buffer
    createDoubleBuffer(200);

    // now load the background image
    pcxInit(&ImagePcx);
    pcxLoad("critback.pcx", &ImagePcx, 1);

    // copy PCX image to double buffer
    pcxCopyToBuffer(&ImagePcx, DoubleBuffer);
    pcxDelete(&ImagePcx);

    // put up exit instructions
    printStringDb(80, 2, 9, "Hit any key to exit", 1);

    // create the arrays of little critters
    for (index = 0; index < NUM_CRITTERS; index++) {
        // select a random position for critter
        Critters[index].x = rand() % 320;
        Critters[index].y = rand() % 200;
    }

    // now compute the meeting place for the critters. This could be any point
    // you wish, but in this case we'll use their centroid, but in many cases you
    // may want them to converge upon a player or specific spot
    centroidX = 0;
    centroidY = 0;

    for (index = 0; index < NUM_CRITTERS; index++) {
        // average in next critter position
        centroidX += Critters[index].x;
        centroidY += Critters[index].y;
    }

    // compute final average
    centroidX /= NUM_CRITTERS;
    centroidY /= NUM_CRITTERS;

    // mark centroid
    printStringDb(centroidX, centroidY, 15, "C", 1);

    // now vector each critter toward centroid on a straight line trajectory
    for (index = 0; index < NUM_CRITTERS; index++) {
        // compute deltas
        dx = centroidX - Critters[index].x;
        dy = centroidY - Critters[index].y;

        // compute a unit vector pointing toward centroid for this critter
        length = sqrt(dx * dx + dy * dy);
        dx = dx / length;
        dy = dy / length;

        // now scale the vector by some factor to synthesize velocity
        speed = 2 + rand() % 3;

        // compute trajectory vector
        Critters[index].xv = dx * speed;
        Critters[index].yv = dy * speed;
    }

    // scan under all critters
    for (index = 0; index < NUM_CRITTERS; index++) {
        Critters[index].back = readPixelDb((int)Critters[index].x, (int)Critters[index].y);
    }

    // main event loop, process until keyboard hit
    while (!kbhit()) {
        // do animation cycle, erase, move draw...

        // erase all critters by replacing what was under them
        for (index = 0; index < NUM_CRITTERS; index++) {
            writePixelDb(
                (int)Critters[index].x,
                (int)Critters[index].y,
                Critters[index].back);
        }

        // BEGIN CONVERGENCE CODE /////////////////////////////////////////////////////////////

        // move critters toward centroid if they are far enough away
        for (index = 0; index < NUM_CRITTERS; index++) {
            // test if critter is far enough away from centroid
            // use manhattan distance
            if (abs(Critters[index].x - centroidX) + abs(Critters[index].y - centroidY) > 20) {
                Critters[index].x += Critters[index].xv;
                Critters[index].y += Critters[index].yv;
            }
        }

        // END CONVERGENCE CODE ///////////////////////////////////////////////////////////////

        // scan under critters
        for (index = 0; index < NUM_CRITTERS; index++) {
            Critters[index].back = readPixelDb((int)Critters[index].x, (int)Critters[index].y);
        }

        // draw critters
        for (index = 0; index < NUM_CRITTERS; index++) {
            writePixelDb((int)Critters[index].x, (int)Critters[index].y, 10);
        }

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 9 frames per second max
        timeDelay(2);
    }

    // exit in a very cool way
    screenTransition(SCREEN_SWIPE_X);

    // free up all resources
    deleteDoubleBuffer();
    setGraphicsMode(TEXT_MODE);
}
