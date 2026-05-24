// Starblazer 3-D

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
#include <search.h>             // this one is needed for qsort()

// include all of our stuff

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black17.h"

#define NUM_ASTEROIDS   18  // total number of asteroids in demo

#define SCANNER_X       121 // position of scanner
#define SCANNER_Y       151

typedef struct AsteroidType {
    int state;          // state of asteroid
    int xv, yv, zv;     // velocity of asteroid
    int rx, ry, rz;     // rotation rate of asteroid
} Asteroid, *AsteroidPtr;

Object TestObjects[NUM_ASTEROIDS];  // objects in universe, need to be in global
                                    // data segment to make things easier

void drawSpeedo(int speed) {
    // this function draws the digital speedometer

    int markX = 258,    // starting tick mark
        index,          // looping variable
        color;          // color of tick mark

    // compute number of iterations
    speed /= 5;

    if (speed >= 0) {
        // select forward color
        color = 10;
    } else {
        // set color to backward
        color = 12;

        // take abs of speed
        speed = -speed;
    }

    // draw the ticks
    for (index = 0; index < speed; index++, markX += 6) {
        // draw a vertical line
        lineV(4, 14, markX, color);
    }

    // erase remaining tick marks
    for (; index < 10; index++, markX += 6) {
        // draw a vertical line
        lineV(4, 14, markX, 0);
    }
}

void drawScanner(AsteroidPtr asteroids, int draw) {
    // this function erases or draws the blips on the scanner

    int index,      // looping variable
        xb, yb,     // position of blip in screen coordinates on scanner
        cb;         // color of blip

    static int blipX[NUM_ASTEROIDS + 1],    // position 0 holds the ship
               blipY[NUM_ASTEROIDS + 1],    // position 0 holds the ship
               activeBlips;                 // number of active blips this cycle

    // should the scanner image be erased or drawn
    if (!draw) {
        // erase all the blips
        for (index = 0; index <= activeBlips; index++) {
            writePixel(SCANNER_X + blipX[index], SCANNER_Y + blipY[index], 0);
        }
    } else {
        // draw the scanner blips

        // draw asteroids above player as red, below as blue and player as green
        // the asteroids exits in positions 1..n and the player at 0
        activeBlips = 0;

        for (index = 0; index < NUM_ASTEROIDS; index++) {
            // test if asteroids is alive, if so, draw it, and save it in record
            if (asteroids[index].state) {
                // increase active number of blips
                activeBlips++;

                // compute screen coordinates of blip
                xb = (TestObjects[index].worldPos.x + 2500) / 63;
                yb = (2500 - TestObjects[index].worldPos.z) / 139;

                // compute color
                if (TestObjects[index].worldPos.y < ViewPoint.y) {
                    cb = 12;
                } else {
                    cb = 9;
                }

                // save the blip
                blipX[activeBlips] = xb;
                blipY[activeBlips] = yb;

                // draw the blip
                writePixel(SCANNER_X + xb, SCANNER_Y + yb, cb);
            }
        }

        // now process the ship blip
        blipX[0] = (ViewPoint.x + 2500) / 63;
        blipY[0] = (2500 - ViewPoint.z) / 139;

        writePixel(SCANNER_X + blipX[0], SCANNER_Y + blipY[0], 10);
    }
}

void main(int argc, char** argv) {
    int done = 0,           // exit flag
        index,              // looping variable
        shipPitch = 0,      // current direction of ship
        shipYaw = 0,
        shipRoll = 0,
        shipSpeed = 0;

    Vector3D unitZ          = { 0, 0, 1, 1 },   // a unit vector in the Z direction
             shipDirection  = { 0, 0, 1, 1 };   // the ships direction

    Asteroid asteroids[NUM_ASTEROIDS];  // the asteroid field
    char buffer[80];                    // output string buffer
    PcxPicture imagePcx;                // used to load in the background imagery

    Matrix4x4 rotate;                   // used to build up rotation matrix

    // set 2-D clipping region to take into consideration the instrument panels
    PolyClipMinY = 0;
    PolyClipMaxY = 121;

    // set up viewing and 3D clipping parameters
    ClipNearZ       = 100;
    ClipFarZ        = 4000;
    ViewingDistance = 250;

    // turn the damn light up a bit!
    AmbientLight = 8;

    LightSource.x =  0.918926;
    LightSource.y =  0.248436;
    LightSource.z = -0.306359;

    // build all look up tables
    buildLookUpTables();

    // load in small asteroids
    for (index = 0; index < 6; index++) {
        if (plgLoadObject(&TestObjects[index], "pyramid.plg", 2)) {
            printf("\nplg loaded.");
        } else {
            printf("\nCouldn't load file");
        }
    }

    // load in medium asteroids
    for (index = 6; index < 12; index++) {
        if (plgLoadObject(&TestObjects[index], "diamond.plg", 3)) {
            printf("\nplg loaded.");
        } else {
            printf("\nCouldn't load file");
        }
    }

    // load in large asteroids
    for (index = 12; index < 18; index++) {
        if (plgLoadObject(&TestObjects[index], "cube.plg", 4)) {
            printf("\nplg loaded.");
        } else {
            printf("\nCouldn't load file");
        }
    }

    // position and set velocity of all asteroids
    for (index = 0; index < NUM_ASTEROIDS; index++) {
        // set position
        TestObjects[index].worldPos.x = -1000 + rand() % 2000;
        TestObjects[index].worldPos.y = -1000 + rand() % 2000;
        TestObjects[index].worldPos.z = -1000 + rand() % 2000;

        // set velocity
        asteroids[index].xv = -20 + rand() % 40;
        asteroids[index].yv = -20 + rand() % 40;
        asteroids[index].zv = -20 + rand() % 40;

        // set angular rotation rate
        asteroids[index].rx = rand() % 10;
        asteroids[index].ry = rand() % 10;
        asteroids[index].rz = rand() % 10;

        // set state to alive
        asteroids[index].state = 1;
    }

    // set graphics to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // read the 3D color palette off disk
    loadPaletteDisk("standard.pal", (RgbPalettePtr)&ColorPalette3D);
    writePalette((RgbPalettePtr)&ColorPalette3D);

    // allocate double buffer
    createDoubleBuffer(122);

    // load in background image
    pcxInit((PcxPicturePtr)&imagePcx);
    pcxLoad("blz3dcoc.pcx", (PcxPicturePtr)&imagePcx, 1);
    pcxShowBuffer((PcxPicturePtr)&imagePcx);

    // install the isr keyboard driver
    keyboardInstallDriver();

#ifdef DOS_32_BIT
    // load the ROM font from font.bin
    if (!initRomCharSet()) {
        printf("\nFatal error: Could not initialize font system\n");
        return;
    }
#endif

    // scan the asteroid field
    drawScanner((AsteroidPtr)&asteroids[0], 1);

    // main event loop
    while (!done) {
        // compute starting time of this frame
        StartingTime = timerQuery();

        // erase all objects
        // drawPolyList(1);
        fillDoubleBuffer32(0);

        // erase the scanner
        drawScanner((AsteroidPtr)&asteroids[0], 0);

        // test what user is doing

        // change ship velocity
        if (KeyboardState[MAKE_RGT_BRACKET]) {
            // speed up
            if ((shipSpeed += 5) > 50) {
                shipSpeed = 50;
            }
        }

        if (KeyboardState[MAKE_LFT_BRACKET]) {
            // slow down
            if ((shipSpeed -= 5) < -50) {
                shipSpeed = -50;
            }
        }

        // test for turns
        if (KeyboardState[MAKE_RIGHT]) {
            // rotate ship to right
            if ((shipYaw += 4) >= 360) {
                shipYaw -= 360;
            }
        }

        if (KeyboardState[MAKE_LEFT]) {
            // rotate ship to left
            if ((shipYaw -= 4) < 0) {
                shipYaw += 360;
            }
        }

        // test for up and down
        if (KeyboardState[MAKE_UP]) {
            ViewPoint.y -= 25;
        }

        if (KeyboardState[MAKE_DOWN]) {
            ViewPoint.y += 25;
        }

        // test for exit
        if (KeyboardState[MAKE_ESC]) {
            done = 1;
        }

        // rotate trajectory vector to align with view direction
        matIdentity4x4(rotate);

        rotate[0][0] = ( CosLook[shipYaw]);
        rotate[0][2] = (-SinLook[shipYaw]);
        rotate[2][0] = ( SinLook[shipYaw]);
        rotate[2][2] = ( CosLook[shipYaw]);

        // x component
        shipDirection.x =
            unitZ.x * rotate[0][0] +
            unitZ.z * rotate[2][0];

        // y component
        shipDirection.y =
            unitZ.y * rotate[1][1];

        // z component
        shipDirection.z =
            unitZ.x * rotate[0][2] +
            unitZ.z * rotate[2][2];

        // move viewpoint based on ship trajectory
        ViewPoint.x += shipDirection.x * shipSpeed;
        ViewPoint.y += shipDirection.y * shipSpeed;
        ViewPoint.z += shipDirection.z * shipSpeed;

        // test ship against universe boundaries
        if (ViewPoint.x > 2500) {
            ViewPoint.x = -2500;
        } else if (ViewPoint.x < -2500) {
            ViewPoint.x = 2500;
        }

        if (ViewPoint.y > 2500) {
            ViewPoint.y = -2500;
        } else if (ViewPoint.y < -2500) {
            ViewPoint.y = 2500;
        }

        if (ViewPoint.z > 2500) {
            ViewPoint.z = -2500;
        } else if (ViewPoint.z < -2500) {
            ViewPoint.z = 2500;
        }

        // set view angles based on trajectory of ship
        ViewAngle.angX = shipPitch;
        ViewAngle.angY = shipYaw;
        ViewAngle.angZ = shipRoll;

        // rotate asteroids
        for (index = 0; index < NUM_ASTEROIDS; index++) {
            // rotate object based on rotation rates
            rotateObject(&TestObjects[index], asteroids[index].rx,
                                              asteroids[index].ry,
                                              asteroids[index].rz);
        }

        // translate asteroids
        for (index = 0; index < NUM_ASTEROIDS; index++) {
            // translate each asteroids world position
            TestObjects[index].worldPos.x += asteroids[index].xv;
            TestObjects[index].worldPos.y += asteroids[index].yv;
            TestObjects[index].worldPos.z += asteroids[index].zv;

            // test if the asteroid has gone off the screen anywhere
            if (TestObjects[index].worldPos.x > 2500 ||
                TestObjects[index].worldPos.x < -2500) {
                asteroids[index].xv = -asteroids[index].xv;
            }

            if (TestObjects[index].worldPos.y > 2500 ||
                TestObjects[index].worldPos.y < -2500) {
                asteroids[index].yv = -asteroids[index].yv;
            }

            if (TestObjects[index].worldPos.z > 2500 ||
                TestObjects[index].worldPos.z < -2500) {
                asteroids[index].zv = -asteroids[index].zv;
            }
        }

        // now that user has possible moved viewpoint, create the global
        // world to camera transformation matrix
        createWorldToCamera();

        // reset the polygon list
        generatePolyList(NULL, RESET_POLY_LIST);

        // perform general 3-D pipeline
        for (index = 0; index < NUM_ASTEROIDS; index++) {
            // test if object is visible
            if (!removeObject(&TestObjects[index], OBJECT_CULL_XYZ_MODE)) {
                // convert object local coordinates to world coordinate
                localToWorldObject(&TestObjects[index]);

                // remove the backfaces and shade object
                removeBackfacesAndShade(&TestObjects[index]);

                // convert world coordinates to camera coordinate
                worldToCameraObject(&TestObjects[index]);

                // clip the objects polygons against viewing volume
                clipObject3D(&TestObjects[index], CLIP_Z_MODE);

                // generate the final polygon list
                generatePolyList(&TestObjects[index], ADD_TO_POLY_LIST);
            }
        }

        // sort the polygons
        sortPolyList();

        // draw the polygon list
        drawPolyList();

        // display the pitch yaw and row of ship
        sprintf(buffer, "%4d", shipPitch);
        printString(80, 6, 10, buffer, 0);

        sprintf(buffer, "%4d", shipYaw);
        printString(144, 6, 10, buffer, 0);

        sprintf(buffer, "%4d", shipRoll);
        printString(208, 6, 10, buffer, 0);

        // draw speedo
        drawSpeedo(shipSpeed);

        // draw the scanner
        drawScanner((AsteroidPtr)&asteroids[0], 1);

        // display double buffer
        displayDoubleBuffer32(DoubleBuffer, 18);

        // lock onto 18 frames per second max
        while ((timerQuery() - StartingTime) < 1);
    }

#ifdef DOS_32_BIT
    freeRomCharSet();
#endif

    // restore graphics mode back to text
    setGraphicsMode(TEXT_MODE);

    // restore the old keyboard driver
    keyboardRemoveDriver();

#ifdef DOS_32_BIT
    exitToDos(0);
#endif
}
