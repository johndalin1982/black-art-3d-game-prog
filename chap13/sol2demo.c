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

#include "black3.h"
#include "black4.h"
#include "black5.h"
#include "black6.h"
#include "black8.h"
#include "black9.h"
#include "black11.h"

Object TestObject;  // the test object

void main(int argc, char** argv) {
    int done = 0;       // exit flag
    char buffer[80];    // used to print strings

    // load in the object from the command line
    if (!plgLoadObject(&TestObject, argv[1], 1)) {
        printf("\nCouldn't find file %s", argv[1]);
        return;
    }

    // position the object 300 units in front of user
    positionObject((ObjectPtr)&TestObject, 0, 0, 300);

    // set the viewpoint
    ViewPoint.x = 0;
    ViewPoint.y = 0;
    ViewPoint.z = 0;

    // create the sin/cos lookup tables used for the rotation function
    buildLookUpTables();

    // set graphics to mode 13h
    setGraphicsMode(GRAPHICS_MODE13);

    // allocate double buffer
    createDoubleBuffer(200);

    // read the 3d color palette off disk
    loadPaletteDisk("standard.pal", (RgbPalettePtr)&ColorPalette3D);
    writePalette((RgbPalettePtr)&ColorPalette3D);

    // install the isr keyboard driver
    keyboardInstallDriver();

    // set viewing distance
    ViewingDistance = 250;

    // main event loop
    while (!done) {
        // compute starting time of this frame
        StartingTime = timerQuery();

        // erase the screen
        fillDoubleBuffer(0);

        // test what key(s) user is pressing

        // test if user is moving viewpoint in positive X
        if (KeyboardState[MAKE_RIGHT]) {
            ViewPoint.x += 5;
        }

        // test if user is moving viewpoint in negative X
        if (KeyboardState[MAKE_LEFT]) {
            ViewPoint.x -= 5;
        }

        // test if user is moving viewpoint in positive Y
        if (KeyboardState[MAKE_UP]) {
            ViewPoint.y += 5;
        }

        // test if user is moving viewpoint in negative Y
        if (KeyboardState[MAKE_DOWN]) {
            ViewPoint.y -= 5;
        }

        // test if user is moving viewpoint in positive Z
        if (KeyboardState[MAKE_PGUP]) {
            ViewPoint.z += 5;
        }

        // test if user is moving viewpoint in negative Z
        if (KeyboardState[MAKE_PGDWN]) {
            ViewPoint.z -= 5;
        }

        // this section takes care of view angle rotation
        if (KeyboardState[MAKE_Z]) {
            if ((ViewAngle.angX += 10) > 360) {
                ViewAngle.angX = 0;
            }
        }

        if (KeyboardState[MAKE_A]) {
            if ((ViewAngle.angX -= 10) < 0) {
                ViewAngle.angX = 360;
            }
        }

        if (KeyboardState[MAKE_X]) {
            if ((ViewAngle.angY += 10) > 360) {
                ViewAngle.angY = 0;
            }
        }

        if (KeyboardState[MAKE_S]) {
            if ((ViewAngle.angY -= 5) < 0) {
                ViewAngle.angY = 360;
            }
        }

        if (KeyboardState[MAKE_C]) {
            if ((ViewAngle.angZ += 5) > 360) {
                ViewAngle.angZ = 0;
            }
        }

        if (KeyboardState[MAKE_D]) {
            if ((ViewAngle.angZ -= 5) < 0) {
                ViewAngle.angZ = 360;
            }
        }

        // test for exit key
        if (KeyboardState[MAKE_ESC]) {
            done = 1;
        }

        // rotate the object on all three axes
        rotateObject((ObjectPtr)&TestObject, 2, 4, 6);

        // convert to world coordinates
        localToWorldObject((ObjectPtr)&TestObject);

        // shade and remove backfaces, ignore the backface part for now
        // notice that backface shading and backface removal is done in world coordinates
        removeBackfacesAndShade((ObjectPtr)&TestObject);

        // create the global world to camera transformation matrix
        createWorldToCamera();

        // convert to camera coordinates
        worldToCameraObject((ObjectPtr)&TestObject);

        // draw the object
        drawObjectSolid((ObjectPtr)&TestObject);

        // print out viewpoint
        sprintf(
            buffer,
            "Viewpoint is at (%d,%d,%d)     ",
            (int)ViewPoint.x,
            (int)ViewPoint.y,
            (int)ViewPoint.z);

        printStringDb(0, 0, 9, buffer, 0);

        sprintf(
            buffer,
            "Viewangle is at (%d,%d,%d)     ",
            (int)ViewAngle.angX,
            (int)ViewAngle.angY,
            (int)ViewAngle.angZ);

        printStringDb(0, 10, 9, buffer, 0);

        // display double buffer
        displayDoubleBuffer(DoubleBuffer, 0);

        // lock onto 18 frames per second max
        while ((timerQuery() - StartingTime) < 1);
    }

    // restore graphics mode back to text
    setGraphicsMode(TEXT_MODE);

    // restore the old keyboard driver
    keyboardRemoveDriver();
}

